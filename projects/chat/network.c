#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include "utils/io.h"
#include "network.h"
#include "upnp.h"


static inline bool host(void);
static inline bool client(void);
void upnpSigHandler(int sig);


static ConnectionInfo connection_info;
static const int signums[3] = { SIGINT, SIGKILL, SIGTERM };
static void(*prev_sig_handlers[3])(int) = { NULL };



static inline void installUPNPSigHandler(void)
{
	for (int i = 0; i < (int)(sizeof(signums)/sizeof(signums[0])); ++i)
		prev_sig_handlers[i] = signal(signums[i], upnpSigHandler);
}


static inline void uninstallUPNPSigHandler(void)
{
	for (int i = 0; i < (int)(sizeof(signums)/sizeof(signums[0])); ++i)
		signal(signums[i], prev_sig_handlers[i]);
}


void upnpSigHandler(const int sig)
{
	// remove port forwarding
	terminate_upnp();

	// get the prev handler for this particular sig 
	void(*prev_handler)(int) = NULL;

	for (int i = 0; i < (int)(sizeof(signums)/sizeof(signums[0])); ++i) {
		if (sig == signums[i]) {
			prev_handler = prev_sig_handlers[i];
			break;
		}
	}

	uninstallUPNPSigHandler();

	if (prev_handler == NULL)
		exit(sig);

	prev_handler(sig);
}


const ConnectionInfo* initialize_connection(const enum ConnectionMode mode)
{
	if (mode == CONMODE_HOST) {
		connection_info.local_uname = connection_info.host_uname;
		connection_info.remote_uname = connection_info.client_uname;
		connection_info.local_ip = connection_info.host_ip;
		connection_info.remote_ip = connection_info.client_ip;
	} else if (mode == CONMODE_CLIENT) {
		connection_info.local_uname = connection_info.client_uname;
		connection_info.remote_uname = connection_info.host_uname;
		connection_info.local_ip = connection_info.client_ip;
		connection_info.remote_ip = connection_info.host_ip;
	} else {
		fprintf(stderr, "Unknown ConnectionMode value specified.\n");
		return NULL;
	}

	connection_info.mode = mode;
	askUserFor("Enter your username: ", connection_info.local_uname, UNAME_SIZE);
	askUserFor("Enter the connection port: ", connection_info.port, PORT_STR_SIZE);
	
	if (mode == CONMODE_HOST) {
		if (!host())
			return NULL;
		write(connection_info.remote_fd, connection_info.host_uname, UNAME_SIZE);
		read(connection_info.remote_fd, connection_info.client_uname, UNAME_SIZE);
		write(connection_info.remote_fd, connection_info.client_ip, IP_STR_SIZE);
		read(connection_info.remote_fd, connection_info.host_ip, IP_STR_SIZE);
	} else {
		if (!client())
			return NULL;
		read(connection_info.remote_fd, connection_info.host_uname, UNAME_SIZE);
		write(connection_info.remote_fd, connection_info.client_uname, UNAME_SIZE);
		read(connection_info.remote_fd, connection_info.client_ip, IP_STR_SIZE);
		write(connection_info.remote_fd, connection_info.host_ip, IP_STR_SIZE);
	}
	
	return &connection_info;
}


void terminate_connection(const ConnectionInfo* const cinfo)
{
	if (cinfo->mode == CONMODE_HOST) {
		close(cinfo->remote_fd);
		close(cinfo->local_fd);
		terminate_upnp();
	} else if (cinfo->mode == CONMODE_CLIENT) {
		close(cinfo->remote_fd);
	}
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

	if (inet_ntop(AF_INET, &cliaddr.sin_addr, connection_info.client_ip, IP_STR_SIZE) == NULL) {
		perror("Couldn't get client ip");
		goto Lclose_clifd;
	}

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
	askUserFor("Enter the host IP: ", connection_info.host_ip, IP_STR_SIZE);
	struct hostent *hostent = gethostbyname(connection_info.host_ip);
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

	connection_info.remote_fd = fd;
	return true;

Lclose_fd:
	close(fd);
	return false;
}

