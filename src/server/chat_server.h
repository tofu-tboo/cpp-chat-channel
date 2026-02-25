#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__

#include "server_base.h"
#include "../libs/json_translator.h"
#include "../libs/chat_req_dto.h"
#include "../libs/dto.h"
#include "../libs/producer_consumer.h"

/* Requirement of ChatServer 
- Payload Resolution: process received payloads from clients. The format is JSON strings.
- Timestamp Handling: extract timestamps from messages and order them.
- Broadcast Handling: periodically broadcast messages to all connected clients.
*/

class ChatServer : public ServerBase<User> {
	protected:
		std::multimap<msec64, std::pair<typename NetworkService<User>::Session*, MessageReqDto>> cur_msgs; // timestamped messages
		ProducerConsumerQueue<std::pair<typename NetworkService<User>::Session*, MessageReqDto>> mq; // message queue (raw JSON strings)

		std::shared_mutex mq_mtx;
		std::shared_mutex cm_mtx;
	public:
		ChatServer(std::shared_ptr<NetworkService<User>> service, const int max_fd);
		~ChatServer();
		virtual bool init() override;
	protected:

		virtual void resolve_timestamps();
        virtual void resolve_broadcast();

		// Hooks
		virtual void on_accept(typename NetworkService<User>::Session& ses) override;
		virtual void handle_request(typename NetworkService<User>::Session& ses, std::unique_ptr<Request> req) override;
};

#endif
