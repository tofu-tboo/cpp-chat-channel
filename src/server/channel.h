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
        ChannelServer* server; // upward link

		std::atomic<msec64> empty_since;
    public:
        Channel(NetworkService<User>* service, ChannelServer* srv, ch_id_t id, const int max = 256);
        ~Channel();

		virtual bool init() override;
		virtual void proc() override;

		// Can be polluted by other threads but protecting by ConnectionTracker's mutex
        void leave(typename NetworkService<User>::Session& user, const MessageReqDto& msg);
        void join(typename NetworkService<User>::Session& user, const MessageReqDto& msg);
		void leave_and_logging(typename NetworkService<User>::Session& ses);
		void join_and_logging(typename NetworkService<User>::Session& ses, bool re = true);

		bool ping_pool();

		msec64 get_empty_since() const;
		bool is_stopped() const;

    protected: // Sequencially called in proc() => no needed mutex

        virtual void on_accept(typename NetworkService<User>::Session& client) override;
        virtual void on_req(const typename NetworkService<User>::Session& from, const char* target, Json& root) override;
        virtual void resolve_broadcast() override;

		virtual void free_user(typename NetworkService<User>::Session& ses) override;
};

#endif
