#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <mutex>

#include "chat_server.h"
#include "../libs/util.h"

class ChannelServer; // Forward declaration

class Channel: public ChatServer {
    private:
        ch_id_t channel_id;
        std::thread worker;
        std::atomic<bool> stop_flag{false};
        ChannelServer* server; // upward link

		std::mutex pool_mtx;
		std::unordered_map<fd_t, MessageReqDto> join_pool;
		std::unordered_map<fd_t, MessageReqDto> leave_pool;
    public:
        Channel(ChannelServer* srv, ch_id_t id, const int max_fd = 256);
        ~Channel();

        virtual void proc() override;

		// Can be polluted by other threads but protecting by ConnectionTracker's mutex
        void leave(const fd_t fd, const MessageReqDto& msg);
        void join(const fd_t fd, const MessageReqDto& msg);
		void leave_and_logging(const fd_t fd, msec64 timestamp);
		void join_and_logging(const fd_t fd, msec64 timestamp, bool re = true);

		bool ping_pool();

    protected: // Sequencially called in proc() => no needed mutex
		virtual void resolve_deletion() override;
		virtual void resolve_pool();

        virtual void on_accept() override;
        virtual void on_req(const fd_t from, const char* target, Json& root) override;
		virtual void on_recv(const fd_t from) override;
};

#endif
