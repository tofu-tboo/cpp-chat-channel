#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

#include <thread>
#include <atomic>

#include "../libs/chat_req_dto.h"
#include "../libs/set_super.h"
#include "../libs/times.h"
#include "../libs/dto.h"
#include "chat_server.h"

class ChannelServer; // Forward declaration

class Channel: public ChatServer {
	SET_SUPER(ChatServer);
    var_private:
        ch_id_t channel_id;
        ChannelServer* server; // upward link

		std::atomic<msec64> empty_since;
    func_public:
        Channel(std::shared_ptr<NetworkService<User>> service, ChannelServer* srv, ch_id_t id, const int max = 256);
        ~Channel();

		virtual bool init() override;
		virtual void proc() override;

		// Can be polluted by other threads but protecting by ConnectionTracker's mutex
        void leave(Session& user, const MessageReqDto& msg);
        void join(Session& user, const MessageReqDto& msg);
		void leave_and_logging(Session& ses);
		void join_and_logging(Session& ses, bool re = true);

		bool ping_pool();

		msec64 get_empty_since() const;

    func_protected: // Sequencially called in proc() => no needed mutex

        virtual void on_accept(Session& client) override;
        virtual void handle_request(Session& ses, std::unique_ptr<Request> req) override;
        virtual void resolve_broadcast() override;

		virtual void free_user(Session& ses) override;
};

#endif
