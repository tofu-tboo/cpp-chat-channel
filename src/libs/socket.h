#ifndef __SOCKET_H__
#define __SOCKET_H__


#define FAILED(mth)         ((mth) == -1)
#define FD_ERR              -1

typedef struct addrinfo sAddrInfo;
typedef struct epoll_event pollev;
typedef int fd_t;

#endif