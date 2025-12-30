#ifndef __SERVER_BASE_H__
#define __SERVER_BASE_H__

#define FD_ERR              -1
#define MAX_FRAME_SIZE      16 * 1024
#define FAILED(mth)         ((mth) == -1)
#define iERROR(...)         LOG(_CR_ "[%x] " _EC_, branch_id); ERROR(__VA_ARGS__)

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <cstdint>

typedef struct addrinfo sAddrInfo;
typedef struct epoll_event pollev;
typedef int fd_t; 
typedef int msec;
typedef uint64_t msec64;
/*
All servers have only one shared file descriptor listening on a port.
ServerBase assumed that it has one channel.
*/

class ServerBase {
    protected:
        static fd_t fd;
    private:
        fd_t efd;
        int branch_id; // manager branch's id
        int max_fd;
        std::unordered_set<fd_t> listeners;

        std::unordered_map<fd_t, std::string> recv_buf; // per-connection accumulation buffer
        std::vector<std::string> mq; // parsed json frames
        std::multimap<msec64, std::string> cur_msgs; // sorted by timestamp
        std::vector<fd_t> next_deletion;

    public:
        ServerBase(int max_fd = 256);
        ~ServerBase();

        virtual void proc(const msec to = 0);

    private:
        bool set_network();
    protected:
        virtual void handle_events(const pollev event);
        virtual bool add_listener(const fd_t fd);
        virtual bool delete_listener(const fd_t fd);
        virtual bool recv_frame(const fd_t fd);
        virtual bool send_frame(const fd_t fd, const std::string& payload);
        virtual void broadcast(const std::string& payload);
        virtual void resolve_timestamps();
        virtual void resolve_payload(const std::string& payload);
};

#endif
