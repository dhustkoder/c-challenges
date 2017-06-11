#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>


#define RETFAIL(label) do { ret = EXIT_FAILURE; goto label; } while (0)
#define UNAME_SIZE      ((int)24)
#define IPSTR_SIZE      (INET_ADDRSTRLEN)
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

static struct UPNPInfo {
	struct UPNPDev* dev;
	struct UPNPUrls urls;
	struct IGDdatas data;
	char* port;
	const char* proto;
} upnp_info = { .dev = NULL, .port = NULL, .proto = NULL };


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


static void terminateUPNP(void);
static inline void upnpSigHandler(const int sig)
{
	fprintf(stderr, "\nreceived signal: %d\nClosing...\n", sig);
	terminateUPNP();
	exit(sig);
}


static inline bool initializeUPNP(const char* const port)
{
	int error = 0;
	char lan_addr[IPSTR_SIZE];
	char wan_addr[IPSTR_SIZE];
	upnp_info.port = malloc(strlen(port) + 1);
	strcpy(upnp_info.port, port);
	upnp_info.proto = "TCP";

	upnp_info.dev = upnpDiscover(
	        4500   , // time to wait (milliseconds)
	        NULL   , // multicast interface (or null defaults to 239.255.255.250)
	        NULL   , // path to minissdpd socket (or null defaults to /var/run/minissdpd.sock)
	        0      , // source port to use (or zero defaults to port 1900)
	        0      , // 0==IPv4, 1==IPv6
	        #if MINIUPNPC_API_VERSION >= 16
		0      , // ttl (only on >= 16 MINIUPNC_API_VERSION)
	        #endif
	        &error); // error condition

	if (error != UPNPDISCOVER_SUCCESS) {
		fprintf(stderr, "Couldn't set UPnP: %s\n", strupnperror(error));
		return false;
	}

	const int status = UPNP_GetValidIGD(upnp_info.dev, &upnp_info.urls, &upnp_info.data,
	                                    lan_addr, sizeof(lan_addr));
	// look up possible "status" values, the number "1" indicates a valid IGD was found
	if (status == 0) {
		fprintf(stderr, "Couldn't find a valid IGD.\n");
		goto Lfree_upnp_dev;
	}
	

	// get the external (WAN) IP address
	error = UPNP_GetExternalIPAddress(upnp_info.urls.controlURL,
	                                  upnp_info.data.first.servicetype,
				          wan_addr);

	if (error != UPNPCOMMAND_SUCCESS)
		goto Lupnp_command_error;

	// add a new TCP port mapping from WAN port 12345 to local host port 24680
	error = UPNP_AddPortMapping(
	            upnp_info.urls.controlURL,
	            upnp_info.data.first.servicetype,
	            upnp_info.port ,  // external (WAN) port requested
	            upnp_info.port ,  // internal (LAN) port to which packets will be redirected
	            lan_addr          ,  // internal (LAN) address to which packets will be redirected
	            "Chat"            ,  // text description to indicate why or who is responsible for the port mapping
	            upnp_info.proto   ,  // protocol must be either TCP or UDP
	            NULL              ,  // remote (peer) host address or nullptr for no restriction
	            NULL              ); // port map lease duration (in seconds) or zero for "as long as possible"

	if (error != UPNPCOMMAND_SUCCESS)
		goto Lupnp_command_error;

	signal(SIGINT, upnpSigHandler);
	signal(SIGKILL, upnpSigHandler);
	signal(SIGTERM, upnpSigHandler);
	return true;

Lupnp_command_error:
	fprintf(stderr, "Couldn't set UPnP: %s\n", strupnperror(error));
	FreeUPNPUrls(&upnp_info.urls);
Lfree_upnp_dev:
	freeUPNPDevlist(upnp_info.dev);
	return false;
}


static inline void terminateUPNP(void)
{
	const int error = UPNP_DeletePortMapping(upnp_info.urls.controlURL,
	                       upnp_info.data.first.servicetype,
			       upnp_info.port,
			       upnp_info.proto,
			       NULL);
	
	freeUPNPDevlist(upnp_info.dev);
	FreeUPNPUrls(&upnp_info.urls);
	free(upnp_info.port);

	if (error != 0) {
		fprintf(stderr, "Couldn't delete port mapping: %s",
		        strupnperror(error));
	}
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

	if (!initializeUPNP(buffer))
		return EXIT_FAILURE;

	/* socket(), creates an endpoint for communication and returns a
	 * file descriptor that refers to that endpoint                */
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		RETFAIL(LterminateUPNP);
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
LterminateUPNP:
	terminateUPNP();
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

