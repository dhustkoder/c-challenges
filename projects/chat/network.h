#ifndef CHAT_NETWORK_H_
#define CHAT_NETWORK_H_


enum ConnectionMode {
	CONMODE_HOST,
	CONMODE_CLIENT
};


extern bool initialize_connection(enum ConnectionMode mode);
extern void terminate_connection(void);



#endif
