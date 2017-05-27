#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)


static const int kPort = 7171;
static const int kNickSize = 24;
static const int kBufferSize = 512;
static char buffer[512] = { 0 };
static char localnick[24];
static char remotenick[24];


static inline int sendMsg(const int fd, const char* const msg)
{
	return write(fd, msg, strlen(msg));
}


static inline int recieveMsg(const int fd)
{
	return read(fd, buffer, kBufferSize - 1);
}


static inline int exchangeServer(const int fd, const void* const send, void* const recieve, const int size)
{
	if (write(fd, send, size) < 0 || read(fd, recieve, size) < 0) {
		perror("Exchange failed");
		return 1;
	}
	return 0;
}


static inline int exchangeClient(const int fd, const void* const send, void* const recieve, const int size)
{
	if (read(fd, recieve, size) < 0 || write(fd, send, size) < 0) {
		perror("Exchange failed");
		return 1;
	}
	return 0;
}


static inline void getlocalnick(void)
{
	printf("Enter Your Nick: ");
	fgets(localnick, sizeof(localnick) - 1, stdin);
	localnick[strlen(localnick) - 1] = '\0';
}


static int server(void)
{
	int fd, newfd;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int ret;

	getlocalnick();
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
	newfd = accept(fd, (struct sockaddr*) &cliaddr, &clilen);

	if (newfd < 0) {
		perror("Couldn't accept socket");
		RETFAIL(close_fd);
	}

	exchangeServer(newfd, localnick, remotenick, kNickSize);

	if (recieveMsg(newfd) < 0) {
		perror("Couldn't read message");
		RETFAIL(close_newfd);
	}

	printf("%s says: %s", remotenick, buffer);

	if (sendMsg(newfd, "Got Your Message!") < 0) {
		perror("Couldn't write message");
		RETFAIL(close_newfd);
	}

	ret = EXIT_SUCCESS;

close_newfd:
	close(newfd);
close_fd:
	close(fd);
	return ret;
}


static int client(void)
{
	int fd;
	struct sockaddr_in servaddr;
	struct hostent *server;
	int ret;

	getlocalnick();
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

	exchangeClient(fd, localnick, remotenick, kNickSize);

	printf("Please enter the message: ");
	fgets(buffer, kBufferSize - 1,stdin);

	if (sendMsg(fd, buffer) < 0)  {
		perror("Couldn't write to socket");
		RETFAIL(close_fd);
	}

	if (recieveMsg(fd) < 0) {
		perror("Couldn't read from socket");
		RETFAIL(close_fd);
	}

	printf("%s says: %s\n", remotenick, buffer);

	ret = EXIT_SUCCESS;
close_fd:
	close(fd);
	return ret;
}


static void printusage(const char* argv_0)
{
	fprintf(stderr, "Usage: %s [type: server, client]\n", argv_0);
}


int main(int argc, char** argv)
{
	if (argc > 1) {
		if (strcmp(argv[1], "client") == 0)
			return client();
		else if (strcmp(argv[1], "server") == 0)
			return server();
	}

	printusage(argv[0]);
	return EXIT_FAILURE;
}

