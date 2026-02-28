#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__

#include "server_base.h"
#include "../libs/json_translator.h"
#include "../libs/dto.h"
#include "../libs/producer_consumer.h"
#include "../libs/set_super.h"
#include "../libs/times.h"

/* Requirement of ChatServer 
- Payload Resolution: process received payloads from clients. The format is JSON strings.
- Timestamp Handling: extract timestamps from messages and order them.
- Broadcast Handling: periodically broadcast messages to all connected clients.
*/

class ChatServer : public ServerBase<User> {
	SET_SUPER(ServerBase<User>);
	var_protected:
		std::multimap<msec64, std::pair<Session*, MessageReqDto>> cur_msgs; // timestamped messages
		ProducerConsumerQueue<std::pair<Session*, MessageReqDto>> mq; // message queue (raw JSON strings)

		std::shared_mutex mq_mtx;
		std::shared_mutex cm_mtx;
	func_public:
		ChatServer(std::shared_ptr<NetworkService<User>> service, const int max_fd);
		~ChatServer();
		virtual bool init() override;
	func_protected:

		virtual void resolve_timestamps();
        virtual void resolve_broadcast();

		// Hooks
		virtual void on_accept(Session& ses) override;
		virtual void handle_request(Session& ses, std::unique_ptr<Request> req) override;
};

#endif
