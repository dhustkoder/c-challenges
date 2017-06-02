#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)


static enum ConMode { CONMODE_HOST, CONMODE_CLIENT } conmode;
static const unsigned short kPort = 7171;
static const int kNickSize = 24;
static const int kChatStackSize = 24;
static const int kBufferSize = 512;
static char buffer[512] = { '\0' };
static char* chatstack[24] = { NULL };
static int chatstack_idx = 0;
static char localnick[24] = { '\0' };
static char remotenick[24] = { '\0' };


static inline bool readIntoBuffer(const int fd)
{
	int n;
	if ((n = read(fd, buffer, kBufferSize - 1)) <= 0) {
		perror("read");
		return false;
	}

	if (buffer[n - 1] == '\n')
		buffer[n - 1] = '\0';
	else
		buffer[n] = '\0';

	return true;
}


static inline bool writeFromBuffer(const int fd)
{
	if (write(fd, buffer, strlen(buffer)) <= 0) {
		perror("write");
		return false;
	}
	return true;
}


static inline bool exchangeNicks(const int fd)
{
	bool ret = true;

	if (conmode == CONMODE_HOST) {
		if (write(fd, localnick, kNickSize) < 0 ||
		    read(fd, remotenick, kNickSize) < 0)
			ret = false;
	} else {
		if (read(fd, remotenick, kNickSize) < 0 ||
		    write(fd, localnick, kNickSize) < 0)
			ret = false;
	}

	if (ret == false)
		perror("Couldn't exchange nicks");

	return ret;
}


static inline void clearscr(void)
{
	system("clear");
}


static inline void getLocalNick(void)
{
	printf("Enter Your Nick: ");
	fgets(localnick, sizeof(localnick) - 1, stdin);
	localnick[strlen(localnick) - 1] = '\0';
}


static inline int waitDataBy(const int fd1, const int fd2, const long usecs)
{
	const int fdmax = fd1 > fd2 ? fd1 : fd2;
	struct timeval timeout = { 0, usecs };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(fd1, &fds);
	FD_SET(fd2, &fds);

	int r = select(fdmax + 1, &fds, NULL, NULL, &timeout);

	if (r > 0)
		r = FD_ISSET(fd1, &fds) ? 1 : 2;
	else if (r == -1)
		perror("Failed to wait data");

	return r;
}


static inline bool stackmsg(const char* const nick, const char* const msg)
{
	if (chatstack_idx >= kChatStackSize) {
		free(chatstack[0]);
		for (int i = 0; i < chatstack_idx - 1; ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = kChatStackSize - 1;
	}

	const int len = strlen(msg) + strlen(nick) + 3;
	chatstack[chatstack_idx] = malloc(len + 1);
	sprintf(chatstack[chatstack_idx], "%s: %s", nick, msg);
	++chatstack_idx;
	return true;
}


static inline void freemsgstack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
}


static inline void printchat(void)
{
	printf("Host: %s. Client: %s\n",
	       conmode == CONMODE_HOST ? localnick : remotenick,
	       conmode == CONMODE_CLIENT ? localnick : remotenick);

	int i;
	for (i = 0; i < chatstack_idx; ++i)
		puts(chatstack[i]);
	for (; i < kChatStackSize; ++i)
		printf("\n");

	printf("===============================================\n> ");
	fflush(stdout);
}


static inline int chat(const int confd)
{
	const int usecs = 1000 * 50;
	const char* nick;
	int rfd, wfd;

	clearscr();
	printchat();

	for (;;) {
		const int n = waitDataBy(STDIN_FILENO, confd, usecs);
		if (n <= 0) {
			continue;
		} else if (n == 1) {
			rfd = STDIN_FILENO;
			wfd = confd;
			nick = localnick;
		} else if (n == 2) {
			rfd = confd;
			wfd = -1;
			nick = remotenick;
		}

		if (!readIntoBuffer(rfd) || (wfd != -1 && !writeFromBuffer(wfd)))
			return EXIT_FAILURE;

		if (buffer[0] == '/') {	
			if (strcmp(buffer, "/quit") == 0) {
				if (rfd == confd)
					printf("\nConnection closed by %s.\n", remotenick);
				else
					printf("Connection closed.\n");
				break;
			} else {
				if (rfd == STDIN_FILENO)
					printf("Unknown command %s\n", buffer);
				continue;
			}
		}

		stackmsg(nick, buffer);
		clearscr();
		printchat();
	}

	freemsgstack();
	return EXIT_SUCCESS;
}


static inline int host(void)
{
	int ret;
	conmode = CONMODE_HOST;
	getLocalNick();
	/* socket(), creates an endpoint for communication and returns a
	 * file descriptor that refers to that endpoint                */
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		return EXIT_FAILURE;
	}

	/* The setsockopt() function shall set the option specified by the
	 * option_name argument, at the protocol level specified by the level
	 * argument, t the value pointed to by the option_value argument
	 * for the socket associated with the file descriptor specified by the socket
	 * argument
	 * */
	const int optionval = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optionval, sizeof(int)) == -1) {
		perror("Couldn't set socket opt");
		RETFAIL(Lclose_fd);
	}


	/* the bind() function shall assign a local socket address to
	 * a socket indentified by descriptor socket that has no local 
	 * socket address assigned. Sockets created with socket()
	 * are initially unnamed; they are identified only by their 
	 * address family
	 * */
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;          // IPv4 protocol
	servaddr.sin_addr.s_addr = INADDR_ANY;  // bind to any interface
	servaddr.sin_port = htons(kPort);       /* converts the unsigned short 
	                                         * int from hostbyte order to
						 * network byte order
						 * */
	if (bind(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
		perror("Couldn't bind");
		RETFAIL(Lclose_fd);
	}

	/* the listen() function shall mark a connection-mode socket,
	 * specified by the socket argument, as accepting connections
	 * */
	if (listen(fd, 1) == -1) {
		perror("Couldn't set listen");
		RETFAIL(Lclose_fd);
	}

	puts("Waiting for client...");

	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);

	/* the accept() system call is used with connection-based socket types
	 * (SOCK_STREAM, SOCK_SEQPACKET). It extracts the first connection 
	 * request on the queue of pending connections for the listening socket,
	 * sockfd, creates a new connected socket, and returns a new file
	 * descriptor referring to that socket. The newly created socket is not
	 * listening state. The original socket sockfd is unaffected by this call
	 * */
	const int clifd = accept(fd, (struct sockaddr*)&cliaddr, &clilen);

	if (clifd == -1) {
		perror("Couldn't accept socket");
		RETFAIL(Lclose_fd);
	}

	if (!exchangeNicks(clifd))
		RETFAIL(Lclose_clifd);

	ret = chat(clifd);

Lclose_clifd:
	close(clifd);
Lclose_fd:
	close(fd);
	return ret;
}


static inline int client(void)
{
	int ret;
	conmode = CONMODE_CLIENT;
	getLocalNick();
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		return EXIT_FAILURE;
	}
	
	/* The gethostbyname() function returns a struct of type hostent
	 * for the given host name. Here name is either a hostname or an
	 * IPv4 address.
	 * */
	struct hostent *hostent = gethostbyname("localhost");
	if (hostent == NULL) {
		perror("Couldn't get host by name");
		RETFAIL(Lclose_fd);
	}

	/* The connect() function shall attempt to make a connection on a 
	 * mode socket or to set or reset the peer address of a connectionless
	 * mode socket.
	 * */
	struct sockaddr_in hostaddr;
	memset(&hostaddr, 0, sizeof(hostaddr));
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = htons(kPort);
	memcpy(&hostaddr.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
	if (connect(fd, (struct sockaddr*)&hostaddr, sizeof(hostaddr)) == -1) {
		perror("Couldn't connect");
		RETFAIL(Lclose_fd);
	}

	if (!exchangeNicks(fd))
		RETFAIL(Lclose_fd);

	ret = chat(fd);

Lclose_fd:
	close(fd);
	return ret;
}


int main(int argc, char** argv)
{
	if (argc > 1) {
		if (strcmp(argv[1], "client") == 0)
			return client();
		else if (strcmp(argv[1], "host") == 0)
			return host();
	}

	fprintf(stderr, "Usage: %s [type: host, client]\n", argv[0]);
	return EXIT_FAILURE;
}


