#ifndef __CHAT_SERVER_H__
#define __CHAT_SERVER_H__


#include "server_base.h"
#include "../libs/socket.h"
#include "../libs/dto.h"
#include "../libs/producer_consumer.h"

/* Requirement of ChatServer 
- Payload Resolution: process received payloads from clients. The format is JSON strings.
- Timestamp Handling: extract timestamps from messages and order them.
- Broadcast Handling: periodically broadcast messages to all connected clients.
*/

class ChatServer : public ServerBase {
	protected:
		std::multimap<msec64, std::pair<fd_t, std::string>> cur_msgs; // timestamped messages
		ProducerConsumerQueue<std::pair<fd_t, MessageReqDto>> mq; // message queue (raw JSON strings)
	public:
		ChatServer(const int max_fd = 32, const msec to = 0);
		~ChatServer();
	protected:
		virtual void resolve_timestamps();
        virtual void resolve_broadcast();

		// Hooks
		virtual void on_req(const fd_t from, const char* target, Json& root) override; // handle both pure json & payload
		virtual void on_recv(const fd_t from) override;
};

#endif
