#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

#include "upnp.h"


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)
#define UNAME_SIZE      ((int)24)
#define CHAT_STACK_SIZE ((int)24)
#define BUFFER_SIZE     ((int)512)

static char buffer[BUFFER_SIZE]         = { '\0' };
static char* chatstack[CHAT_STACK_SIZE] = { NULL };
static int chatstack_idx                = 0;
static char host_ip[IPSTR_SIZE]         = { '\0' };
static char client_ip[IPSTR_SIZE]       = { '\0' };
static char host_uname[UNAME_SIZE]      = { '\0' };
static char client_uname[UNAME_SIZE]    = { '\0' };
static char* local_uname                = NULL;
static char* remote_uname               = NULL;

static enum ConMode { CONMODE_HOST, CONMODE_CLIENT } conmode;


static inline bool readInto(char* const dest, const int fdsrc, const int maxsize)
{
	int n;
	if ((n = read(fdsrc, dest, maxsize - 1)) <= 0) {
		perror("read");
		return false;
	}

	if (dest[n - 1] == '\n')
		--n;

	dest[n] = '\0';
	return true;
}


static inline bool writeInto(const int fd_dest, const char* const src)
{
	if (write(fd_dest, src, strlen(src)) <= 0) {
		perror("write");
		return false;
	}
	return true;
}


static inline bool exchange(const int fd, const char* const send, char* const recv, const int size)
{
	bool ret = true;

	if (conmode == CONMODE_HOST) {
		if (write(fd, send, size) == -1 || read(fd, recv, size) == -1)
			ret = false;
	} else {
		if (read(fd, recv, size) == -1 || write(fd, send, size) == -1)
			ret = false;
	}

	if (!ret)
		perror("exchange failed");

	return ret;
}


static inline void clearScreen(void)
{
	system("clear");
}


static inline void askUserFor(const char* const msg, char* const dest, const int size)
{
	writeInto(STDOUT_FILENO, msg);
	readInto(dest, STDIN_FILENO, size);
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


static inline bool stackMsg(const char* const nick, const char* const msg)
{
	if (chatstack_idx >= CHAT_STACK_SIZE) {
		free(chatstack[0]);
		for (int i = 0; i < chatstack_idx - 1; ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = CHAT_STACK_SIZE - 1;
	}

	const int len = strlen(msg) + strlen(nick) + 3;
	chatstack[chatstack_idx] = malloc(len + 1);
	sprintf(chatstack[chatstack_idx], "%s: %s", nick, msg);
	++chatstack_idx;
	return true;
}


static inline void freeMsgStack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
}


static inline void printChat(void)
{
	printf("Host: %s (%s). Client: %s (%s).\n",
	       host_uname, host_ip, client_uname, client_ip);

	int i;
	for (i = 0; i < chatstack_idx; ++i)
		puts(chatstack[i]);
	for (; i < CHAT_STACK_SIZE; ++i)
		putchar('\n');

	printf("===============================================\n> ");
	fflush(stdout);
}


static inline bool parseChatCommand(const char* const cmd, const bool is_local)
{
	if (strcmp(cmd, "/quit") == 0) {
		if (is_local)
			puts("You closed the connection.");
		else
			printf("\nConnection closed by %s.\n", remote_uname);
		return false;
	} else if (is_local) {
		printf("Unknown command %s.\n", cmd);
		sleep(3);
	}
	return true;
}


static inline int chat(const int confd)
{
	const int usecs = 1000 * 50;
	const char* uname;
	int rfd, wfd, ready;

	for (;;) {
		clearScreen();
		printChat();

		while ((ready = waitDataBy(STDIN_FILENO, confd, usecs)) == 0)
			continue;

		if (ready == 1) {
			rfd = STDIN_FILENO;
			wfd = confd;
			uname = local_uname;
		} else if (ready == 2) {
			rfd = confd;
			wfd = -1;
			uname = remote_uname;
		} else {
			break;
		}

		if (!readInto(buffer, rfd, BUFFER_SIZE) ||
		    (wfd != -1 && !writeInto(wfd, buffer)))
			return EXIT_FAILURE;

		if (buffer[0] == '/') {
			if (!parseChatCommand(buffer, rfd == STDIN_FILENO))
				break;
		} else {
			stackMsg(uname, buffer);
		}
	}

	freeMsgStack();
	return EXIT_SUCCESS;
}


static void upnpSigHandler(const int sig)
{
	fprintf(stderr, "\nreceived signal: %d\nClosing...\n", sig);
	terminate_upnp();
	exit(sig);
}


static inline int host(void)
{
	int ret;
	conmode = CONMODE_HOST;
	local_uname = host_uname;
	remote_uname = client_uname;
	askUserFor("Enter your username: ", host_uname, UNAME_SIZE);
	askUserFor("Enter the connection port: ", buffer, BUFFER_SIZE);
	const unsigned short port = strtoll(buffer, NULL, 0);

	if (!initialize_upnp(buffer))
		return EXIT_FAILURE;

	signal(SIGINT, upnpSigHandler);
	signal(SIGKILL, upnpSigHandler);
	signal(SIGTERM, upnpSigHandler);

	/* socket(), creates an endpoint for communication and returns a
	 * file descriptor that refers to that endpoint                */
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		RETFAIL(Lterminate_upnp);
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
	servaddr.sin_port = htons(port);        /* converts the unsigned short 
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

	inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, IPSTR_SIZE);

	if (!exchange(clifd, host_uname, client_uname, UNAME_SIZE) ||
	    !exchange(clifd, client_ip, host_ip, IPSTR_SIZE))
		RETFAIL(Lclose_clifd);

	ret = chat(clifd);

Lclose_clifd:
	close(clifd);
Lclose_fd:
	close(fd);
Lterminate_upnp:
	terminate_upnp();
	return ret;
}


static inline int client(void)
{
	int ret;
	conmode = CONMODE_CLIENT;
	local_uname = client_uname;
	remote_uname = host_uname;
	askUserFor("Enter your username: ", client_uname, UNAME_SIZE);
	askUserFor("Enter the connection port: ", buffer, BUFFER_SIZE);
	const unsigned short port = strtoll(buffer, NULL, 0);

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("Couldn't open socket");
		return EXIT_FAILURE;
	}
	
	/* The gethostbyname() function returns a struct of type hostent
	 * for the given host name. Here name is either a hostname or an
	 * IPv4 address.
	 * */
	printf("Enter the host IP: ");
	fflush(stdout);
	readInto(host_ip, STDIN_FILENO, IPSTR_SIZE);
	struct hostent *hostent = gethostbyname(host_ip);
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
	hostaddr.sin_port = htons(port);
	memcpy(&hostaddr.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
	if (connect(fd, (struct sockaddr*)&hostaddr, sizeof(hostaddr)) == -1) {
		perror("Couldn't connect");
		RETFAIL(Lclose_fd);
	}

	if (!exchange(fd, client_uname, host_uname, UNAME_SIZE) ||
	    !exchange(fd, host_ip, client_ip, IPSTR_SIZE))
		RETFAIL(Lclose_fd);

	ret = chat(fd);

Lclose_fd:
	close(fd);
	return ret;
}


int main(const int argc, const char* const * const argv)
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

