#ifndef __SOCKET_H__
#define __SOCKET_H__

#define FAILED(mth)         ((mth) == -1)
#define FD_ERR              -1

// Define protocol num
#define WS			0
#define TCP			1
#define WS_NAME		("ws")
#define TCP_NAME	("tcp")


#include <libwebsockets.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>

// v1
typedef struct addrinfo sAddrInfo;
typedef struct epoll_event pollev;
typedef int fd_t;

// v2
typedef struct lws_context ctx;
typedef struct lws_context_creation_info ctx_creation_info;
typedef struct lws_protocols protocols_t;
typedef struct lws lws;
typedef enum lws_callback_reasons callback_reason;
typedef short protocol_id;

typedef struct {
	lws* wsi;
	enum { NONE, ACPT, RECV, SEND, CLOSE } event;
	void* user;
	unsigned char* in;
	size_t len;
	protocol_id prot_id;
} LwsCallbackParam; // add prot_id??

typedef struct {
	lws* wsi;
	protocol_id prot_id;
} Connection;

#endif