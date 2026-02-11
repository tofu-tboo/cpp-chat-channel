#ifndef __SOCKET_H__
#define __SOCKET_H__

#define FAILED(mth)         ((mth) == -1)
#define FD_ERR              -1

// Define protocol num
#define TCP			0
#define WS			1
#define WS_NAME		("ws")
#define TCP_NAME	("tcp")

#define U2S			1000000
#define M2S			1000
#define S2U			0.000001
#define S2M			0.001

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