#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

#include "upnp.h"


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

