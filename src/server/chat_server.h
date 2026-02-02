#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__

#include "typed_json_frame_server.h"
#include "../libs/json.h"
#include "../libs/dto.h"
#include "../libs/producer_consumer.h"

/* Requirement of ChatServer 
- Payload Resolution: process received payloads from clients. The format is JSON strings.
- Timestamp Handling: extract timestamps from messages and order them.
- Broadcast Handling: periodically broadcast messages to all connected clients.
*/

class ChatServer : public TypedJsonFrameServer<User> {
	protected:
		std::unordered_set<User*> users;
		std::multimap<msec64, std::pair<User*, MessageReqDto>> cur_msgs; // timestamped messages
		ProducerConsumerQueue<std::pair<User*, MessageReqDto>> mq; // message queue (raw JSON strings)
	public:
		ChatServer(NetworkService<User>* service, const int max_fd = 32, const msec to = 1000);
		~ChatServer();
	protected:
		virtual void resolve_deletion() override;
		virtual void resolve_timestamps();
        virtual void resolve_broadcast();

		// Hooks
		virtual void on_req(const User& from, const char* target, Json& root) override; // handle both pure json & payload
};

#endif
