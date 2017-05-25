#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <strings.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)


const int kPort = 7171;


static char buffer[512] = { 0 };


static int server(void)
{
	int fd, newfd, n;
	socklen_t clilen;
	struct sockaddr_in servaddr, cliaddr;
	int ret;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("Couldn't open socket");
		return EXIT_FAILURE;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(kPort);

	if (bind(fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		perror("Couldn't bind");
		RETFAIL(close_fd);
	}

	listen(fd, 5);
	clilen = sizeof(cliaddr);
	newfd = accept(fd, (struct sockaddr*) &cliaddr, &clilen);

	if (newfd < 0) {
		perror("Couldn't accept socket");
		RETFAIL(close_fd);
	}

	n = read(newfd, buffer, sizeof(buffer) - 1);

	if (n < 0) {
		perror("Couldn't read message");
		RETFAIL(close_newfd);
	}

	printf("MSG: %s", buffer);

	n = write(newfd, "Got Your Message!", 17);

	if (n < 0) {
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
	int fd, n;
	struct sockaddr_in servaddr;
	struct hostent *server;
	int ret;

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

	printf("Please enter the message: ");
	bzero(buffer,sizeof(buffer));
	fgets(buffer,sizeof(buffer) - 1,stdin);

	n = write(fd,buffer,strlen(buffer));

	if (n < 0)  {
		perror("Couldn't write to socket");
		RETFAIL(close_fd);
	}

	bzero(buffer, 256);
	n = read(fd, buffer, sizeof(buffer) - 1);

	if (n < 0) {
		perror("Couldn't read from socket");
		RETFAIL(close_fd);
	}

	printf("%s\n", buffer);

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
	if (argc < 2) {
		printusage(argv[0]);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "client") == 0)
		return client();
	else if (strcmp(argv[1], "server") == 0)
		return server();

	printusage(argv[0]);
	return EXIT_FAILURE;
}

