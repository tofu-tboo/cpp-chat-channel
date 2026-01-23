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
#include <atomic>

#include "chat_server.h"
#include "../libs/util.h"

class ChannelServer; // Forward declaration

class Channel: public ChatServer {
    private:
        ch_id_t channel_id;
        std::thread worker;
        std::atomic<bool> stop_flag{false};
        ChannelServer* server; // upward link
    public:
        Channel(ChannelServer* srv);
        ~Channel();

        virtual void proc() override;

		// Can be polluted by other threads but protecting by ConnectionTracker's mutex
        void leave(const fd_t fd);
        void join(const fd_t fd);
		void join_and_logging(const fd_t fd, UJoinDto req, bool re = true);

    protected: // Sequencially called in proc() => no needed mutex
        virtual void on_accept() override;
        virtual void on_req(const fd_t from, const char* target, Json& root) override;
};

#endif
