#ifndef CHAT_NETWORK_H_
#define CHAT_NETWORK_H_
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>


#define UNAME_SIZE    ((int)24)
#define IP_STR_SIZE   (INET_ADDRSTRLEN)
#define PORT_STR_SIZE ((int)6)


enum ConnectionMode {
	CONMODE_HOST,
	CONMODE_CLIENT
};


struct ConnectionInfo {
	char host_uname[UNAME_SIZE];
	char client_uname[UNAME_SIZE];
	char ip[IP_STR_SIZE];
	char port[PORT_STR_SIZE];
	char* local_uname;
	char* remote_uname;
	int local_fd;
	int remote_fd;
	enum ConnectionMode mode;
};


extern const struct ConnectionInfo* initialize_connection(enum ConnectionMode mode);
extern void terminate_connection(const ConnectionInfo* cinfo);



#endif
