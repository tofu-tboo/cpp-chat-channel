#ifndef __CONNECTION_TRACKER_H__
#define __CONNECTION_TRACKER_H__

#define MAX_PEV        1024
#define POOL_FULL      601

#include <unordered_set>
#include <sys/epoll.h>
#include <mutex>
// #include <libwebsockets.h>

#include "util.h"
#include "socket.h"


class ConnectionTracker {
	private:
        int max_fd;
        fd_t& listener_fd;
        fd_t efd;
        std::unordered_set<fd_t> clients;
        pollev events[MAX_PEV];
        int evcnt;
        mutable std::mutex mtx;

		// ctx* context = nullptr;
		// ctx_creation_info info;
		// protocols_t protocols[];
    public:
        ConnectionTracker(fd_t& fd, const int max_fd = 256);
        ~ConnectionTracker();

        void init();
        // void init(const int port, const size_t s_user_sz);

        void polling(const msec to);

        void add_client(const fd_t fd);
        void delete_client(const fd_t fd);

        const pollev* get_ev() const;
        const int get_evcnt() const;
        std::unordered_set<fd_t> get_clients() const;
		bool is_full() const;
		int get_max_fd() const;
		size_t get_client_count() const;

		// int callback_logic(lws* wsi, reason reason, void* user, void* in, size_t len);

		// ctx* get_context();
};

#endif
