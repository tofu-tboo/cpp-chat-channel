#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

#include <thread>
#include <atomic>

#include "dto/chat_req_dto.h"
#include "../libs/set_super.h"
#include "../libs/times.h"
#include "../libs/dto.h"
#include "chat_server.h"

class ChannelServer; // Forward declaration

class Channel: public ChatServer {
	SET_SUPER(ChatServer);
    var_private:
        const ch_id_t channel_id;
        ChannelServer* server; // upward link

		std::atomic<bool> freed_rsv;
    func_public:
        Channel(std::shared_ptr<NetworkService<User>> service, ChannelServer* srv, ch_id_t id, const int max = 256);
        ~Channel();

		virtual bool init() override;
		virtual void proc() override;

		// Can be polluted by other threads but protecting by ConnectionTracker's mutex
        void leave(Session& user);
        void join(Session& user);
		void leave_and_logging(Session& ses);
		void join_and_logging(Session& ses, bool re = true);

		bool is_full();

		bool want_freed() const;
		void use(); // TODO?: exchange logic

    func_protected: // Sequencially called in proc() => no needed mutex

        virtual void on_accept(Session& client) override;
        virtual void handle_request(Session& ses, std::shared_ptr<Request> req) override;
        virtual void resolve_broadcast() override;

		virtual void free_user(Session& ses) override;
};

#endif
