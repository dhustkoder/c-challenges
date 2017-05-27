#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <pthread.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)


static const int kPort = 7171;
static const int kNickSize = 24;
static const int kBufferSize = 512;
static char buffer[512] = { '\0' };
static char localnick[24] = { '\0' };
static char remotenick[24] = { '\0' };


static inline void clearscr(void)
{
	system("clear");
}


static inline bool readIntoBuffer(const int fd)
{
	int n;
	if ((n = read(fd, buffer, kBufferSize - 1))) {
		if (buffer[n - 1] == '\n')
			buffer[n - 1] = '\0';
		else
			buffer[n] = '\0';
		return true;
	}
	return false;
}


static inline void getLocalNick(void)
{
	printf("Enter Your Nick: ");
	fgets(localnick, sizeof(localnick) - 1, stdin);
	localnick[strlen(localnick) - 1] = '\0';
}


static inline bool waitForDataBy(const int fd, const long usecs)
{
	struct timeval timeout = { 0, usecs };
	fd_set fds;
	
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
		
	if (select(fd + 1, &fds, NULL, NULL, &timeout))
		return true;

	return false;
}


static inline int chat(const int fd)
{
	const int usecs = 1000 * 50;
	const char* nick;
	for (;;) {
		if (waitForDataBy(STDIN_FILENO, usecs)) {
			if (!readIntoBuffer(STDIN_FILENO)) {
				perror("Couldn't read stdin");
				return EXIT_FAILURE;
			}
			write(fd, buffer, strlen(buffer));
			nick = localnick;
		} else if (waitForDataBy(fd, usecs)) {
			if (!readIntoBuffer(fd)) {
				perror("Couldn't read msg");
				return EXIT_FAILURE;
			}
			nick = remotenick;
		} else {
			continue;
		}

		if (strcmp(buffer, "/quit") == 0) {
			puts("Connection ended.");
			break;
		}
		
		printf("%s says: %s\n", nick, buffer);
	}
	return EXIT_SUCCESS;
}


static inline int server(void)
{
	int fd, newfd;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int ret;

	getLocalNick();
	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		perror("Couldn't open socket");
		return EXIT_FAILURE;
	}

	const int optionval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optionval, sizeof(int));
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(kPort);

	if (bind(fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("Couldn't bind");
		RETFAIL(close_fd);
	}

	listen(fd, 5);
	puts("Waiting for client...");
	newfd = accept(fd, (struct sockaddr*) &cliaddr, &clilen);

	if (newfd < 0) {
		perror("Couldn't accept socket");
		RETFAIL(close_fd);
	}

	if (write(newfd, localnick, kNickSize) < 0 ||
	    read(newfd, remotenick, kNickSize) < 0) {
		perror("Nicks exchange failed");
		RETFAIL(close_newfd);
	}
	
	ret = chat(newfd);

close_newfd:
	close(newfd);
close_fd:
	close(fd);
	return ret;
}


static inline int client(void)
{
	int fd;
	struct sockaddr_in servaddr;
	struct hostent *server;
	int ret;

	getLocalNick();
	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		perror("Couldn't opening socket");
		return EXIT_FAILURE;
	}

	server = gethostbyname("localhost");
	if (server == NULL) {
		perror("Couldn't find host");
		RETFAIL(close_fd);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	bcopy(server->h_addr_list[0], (char *)&servaddr.sin_addr.s_addr, server->h_length);
	servaddr.sin_port = htons(kPort);

	if (connect(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("Couldn't connect");
		RETFAIL(close_fd);
	}

	if (read(fd, remotenick, kNickSize) < 0 ||
	    write(fd, localnick, kNickSize) < 0) {
		perror("Nicks exchange failed");
		RETFAIL(close_fd);
	}

	ret = chat(fd);

close_fd:
	close(fd);
	return ret;
}


int main(int argc, char** argv)
{
	if (argc > 1) {
		if (strcmp(argv[1], "client") == 0)
			return client();
		else if (strcmp(argv[1], "server") == 0)
			return server();
	}

	fprintf(stderr, "Usage: %s [type: server, client]\n", argv[0]);
	return EXIT_FAILURE;
}


