#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

#include <thread>
#include <atomic>

#include "chat_server.h"

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

		std::atomic<bool> paused;
		std::atomic<msec64> empty_since{0};
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

		void wait_stop_pooling();
		void start_pooling();

		msec64 get_empty_since() const;
		bool is_stopped() const;

    protected: // Sequencially called in proc() => no needed mutex
		virtual void resolve_deletion() override;
		virtual void resolve_pool();

        virtual void on_accept(const fd_t client) override;
        virtual void on_req(const fd_t from, const char* target, Json& root) override;
};

#endif
