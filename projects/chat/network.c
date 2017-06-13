#include "network.h"
#include "upnp.h"


#define UNAME_SIZE  ((int)24)


struct ConnectionInfo {
	char host_uname[UNAME_SIZE];
	char client_uname[UNAME_SIZE];
	char ip[IP_STR_SIZE];
	char port[PORT_STR_SIZE];
	char* local_uname;
	char* remote_uname;
	enum ConnectionMode mode;
	int local_fd;
	int remote_fd;
};


struct ConenctionInfo connection_info;


static const int signums[3] = { SIGINT, SIGKILL, SIGTERM };
static void(*prev_sig_handlers[3])(int) = { NULL };


static inline bool host(void);
static inline bool client(void);


static inline void installUPNPSigHandler(void)
{
	void upnpSigHandler(int);
	for (int i = 0; i < (int)sizeof(signums)/sizeof(signums[0]); ++i)
		prev_sig_handlers[i] = signal(signums[i], upnpSigHandler);
}


static inline void uninstallUPNPSigHandler(void)
{
	for (int i = 0; i < (int)sizeof(signums)/sizeof(signums[0]); ++i)
		signal(signums[i], prev_sig_handlers[i]);
}


void upnpSigHandler(const int sig)
{
	// remove port forwarding
	terminate_upnp();

	// get the prev handler for this particular sig 
	void(*prev_handler)(int) = NULL;

	for (int i = 0; i < (int)sizeof(signums)/sizeof(signums[0]); ++i) {
		if (sig == signums[i]) {
			prev_handler = prev_sig_handlers[i];
			break;
		}
	}

	uninstallUPNPSigHandler();

	if (prev_handler != NULL)
		prev_handler();

}


struct ConnectionInfo* initialize_connection(const enum ConnectionMode mode)
{
	int(*confun)(void) = NULL;
	if (mode == CONMODE_HOST) {
		connection_info.local_uname = host_uname;
		connection_info.remote_uname = client_uname;
		confun = host;
	} else if (mode == CONMODE_CLIENT) {
		connection_info.local_uname = client_uname;
		connection_info.remote_uname = host_uname;
		confun = client;
	}

	if (confun == NULL)
		return NULL;

	connection_info.mode = mode;
	askUserFor("Enter your username: ", connection_info.local_uname, UNAME_SIZE);
	askUserFor("Enter the connection port: ", connection_info.port);
	
	if (confun())
		return &connection_info;

}


static inline bool host(void)
{
	if (!initialize_upnp(connection_info.port))
		return false;

	// prevent the upnp to keep the port forwarding
	// if a SIGNAL is received
	installUPNPSigHandler();

	/* socket(), creates an endpoint for communication and returns a
	 * file descriptor that refers to that endpoint                */
	const int fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1) {
		perror("Couldn't open socket");
		goto Lterminate_upnp;
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
		goto Lclose_fd;
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
	servaddr.sin_port = htons(strtoll(connection_info.port, NULL, 0));
	                                        /* htons() converts the unsigned short 
	                                         * int from hostbyte order to
						 * network byte order
						 * */
	if (bind(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
		perror("Couldn't bind");
		goto Lclose_fd;
	}

	/* the listen() function shall mark a connection-mode socket,
	 * specified by the socket argument, as accepting connections
	 * */
	if (listen(fd, 1) == -1) {
		perror("Couldn't set listen");
		goto Lclose_fd;
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
		goto Lclose_fd;
	}

	inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, IPSTR_SIZE);

	if (!exchange(clifd, host_uname, client_uname, UNAME_SIZE) ||
	    !exchange(clifd, client_ip, host_ip, IPSTR_SIZE))
		goto Lclose_clifd;

	connection_info.local_fd = fd;
	connection_info.remote_fd = clifd;
	return true;

Lclose_clifd:
	close(clifd);
Lclose_fd:
	close(fd);
Lterminate_upnp:
	terminate_upnp();
	return false;
}


static inline bool client(void)
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("Couldn't open socket");
		return false;
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
		goto Lclose_fd;
	}

	/* The connect() function shall attempt to make a connection on a 
	 * mode socket or to set or reset the peer address of a connectionless
	 * mode socket.
	 * */
	struct sockaddr_in hostaddr;
	memset(&hostaddr, 0, sizeof(hostaddr));
	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = htons(strtol(connection_info.port, NULL, 0));
	memcpy(&hostaddr.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);
	if (connect(fd, (struct sockaddr*)&hostaddr, sizeof(hostaddr)) == -1) {
		perror("Couldn't connect");
		goto Lclose_fd;
	}

	if (!exchange(fd, client_uname, host_uname, UNAME_SIZE) ||
	    !exchange(fd, host_ip, client_ip, IPSTR_SIZE))
		goto Lclose_fd;

	return true;

Lclose_fd:
	close(fd);
	return false;
}

