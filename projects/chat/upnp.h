#ifndef CHAT_UPNP_H_
#define CHAT_UPNP_H_
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>


extern bool initialize_upnp(const char* const port);
extern void terminate_upnp(void);


#define IPSTR_SIZE (INET_ADDRSTRLEN)


#endif
