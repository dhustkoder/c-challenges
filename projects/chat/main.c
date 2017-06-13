#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include "utils/io.h"
#include "network.h"


#define CHAT_STACK_SIZE ((int)24)
#define BUFFER_SIZE     ((int)512)

static char buffer[BUFFER_SIZE]         = { '\0' };
static char* chatstack[CHAT_STACK_SIZE] = { NULL };
static int chatstack_idx                = 0;


static inline void clearScreen(void)
{
	system("clear");
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


static inline bool stackMsg(const char* const uname, const char* const msg)
{
	if (chatstack_idx >= CHAT_STACK_SIZE) {
		free(chatstack[0]);
		for (int i = 0; i < chatstack_idx - 1; ++i)
			chatstack[i] = chatstack[i + 1];
		chatstack_idx = CHAT_STACK_SIZE - 1;
	}

	const int len = strlen(msg) + strlen(uname) + 3;
	chatstack[chatstack_idx] = malloc(len + 1);
	sprintf(chatstack[chatstack_idx], "%s: %s", uname, msg);
	++chatstack_idx;
	return true;
}


static inline void freeMsgStack(void)
{
	for (int i = 0; i < chatstack_idx; ++i)
		free(chatstack[i]);
}


static inline void printChat(const ConnectionInfo* const cinfo)
{
	printf("Host: %s (%s). Client: %s (%s).\n",
	       cinfo->host_uname, cinfo->host_ip,
	       cinfo->client_uname, cinfo->client_ip);

	int i;
	for (i = 0; i < chatstack_idx; ++i)
		puts(chatstack[i]);
	for (; i < CHAT_STACK_SIZE; ++i)
		putchar('\n');

	printf("===============================================\n> ");
	fflush(stdout);
}


static inline bool parseChatCommand(const ConnectionInfo* const cinfo, const char* const cmd, const bool is_local)
{
	if (strcmp(cmd, "/quit") == 0) {
		if (is_local)
			puts("You closed the connection.");
		else
			printf("\nConnection closed by %s.\n", cinfo->remote_uname);
		return false;
	} else if (is_local) {
		printf("Unknown command %s.\n", cmd);
		sleep(3);
	}
	return true;
}


static inline int chat(const ConnectionInfo* const cinfo)
{
	const int usecs = 1000 * 50;
	const char* uname;
	int rfd, wfd, ready;

	for (;;) {
		clearScreen();
		printChat(cinfo);

		while ((ready = waitDataBy(STDIN_FILENO, cinfo->remote_fd, usecs)) == 0)
			continue;

		if (ready == 1) {
			rfd = STDIN_FILENO;
			wfd = cinfo->remote_fd;
			uname = cinfo->local_uname;
		} else if (ready == 2) {
			rfd = cinfo->remote_fd;
			wfd = -1;
			uname = cinfo->remote_uname;
		} else {
			break;
		}

		if (!readInto(buffer, rfd, BUFFER_SIZE) ||
		    (wfd != -1 && !writeInto(wfd, buffer)))
			return EXIT_FAILURE;

		if (buffer[0] == '/') {
			if (!parseChatCommand(cinfo, buffer, rfd == STDIN_FILENO))
				break;
		} else {
			stackMsg(uname, buffer);
		}
	}

	freeMsgStack();
	return EXIT_SUCCESS;
}


int main(const int argc, const char* const * const argv)
{
	if (argc > 1) {
		const ConnectionInfo* cinfo = NULL;
		if (strcmp(argv[1], "client") == 0) {
			if ((cinfo = initialize_connection(CONMODE_CLIENT)) == NULL)
				return EXIT_FAILURE;
		} else if (strcmp(argv[1], "host") == 0) {
			if ((cinfo = initialize_connection(CONMODE_HOST)) == NULL)
				return EXIT_FAILURE;
		}

		if (cinfo != NULL) {
			const int ret = chat(cinfo);
			terminate_connection(cinfo);
			return ret;
		}
	}

	fprintf(stderr, "Usage: %s [type: host, client]\n", argv[0]);
	return EXIT_FAILURE;
}

