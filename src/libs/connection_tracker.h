#ifndef __CONNECTION_TRACKER_H__
#define __CONNECTION_TRACKER_H__

#include <unordered_set>
#include <unistd.h>
#include <sys/epoll.h>

#include "util.h"
#include "socket.h"

class ConnectionTracker {
    private:
        int max_fd;
        fd_t& listener_fd;
        fd_t efd;
        std::unordered_set<fd_t> clients;
        pollev* events;
        int evcnt;

    public:
        ConnectionTracker(fd_t& fd, const int max_fd = 256);
        ~ConnectionTracker();

        void init();

        void polling(const msec to);

        void add_client(const fd_t fd);
        void delete_client(const fd_t fd);

        const pollev* get_ev() const;
        const int get_evcnt() const;
        const std::unordered_set<fd_t>& get_clients() const;
};

#endif
