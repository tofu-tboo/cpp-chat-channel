

#include "chat_server.h"
#include "user_manager.h"


ChatServer::ChatServer(NetworkService<User>* service, const int max_fd, const msec to): TypedJsonFrameServer(service, max_fd, to) {
	task_runner.pushb(TS_PRE, [this]() {
		cur_msgs.clear();
	});
    // 매 틱마다 mq를 확인하고 브로드캐스트 수행 (이벤트가 없어도 실행됨)
    task_runner.pushf(TS_LOGIC, [this]() {
        resolve_timestamps();
        resolve_broadcast();
    });
}

ChatServer::~ChatServer() {
	cur_msgs.clear();
}

#pragma region PROTECTED_FUNC
void ChatServer::resolve_deletion() {
    for (User* user : next_deletion) {
		if (user->name) free(user->name);
		user->name = nullptr;
        service->close_async(user, "Server closed.");
        LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

void ChatServer::resolve_timestamps() {
    std::queue<std::pair<User*, MessageReqDto>> local_q = mq.pop_all();
	while (!local_q.empty()) {
        std::pair<User*, MessageReqDto> item = std::move(local_q.front());
        local_q.pop();

		cur_msgs.emplace(item.second.timestamp, std::pair<User*, MessageReqDto>(item.first, item.second));
	}
}

void ChatServer::resolve_broadcast() {
    Json cur_window(json_array());
    for (const auto& [timestamp, req] : cur_msgs) {
		std::string type;
		switch (req.second.type)
		{
		case USER:
		{
			type = "user";
			__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s,s:I}",
			"type", type.c_str(), "user_name", req.second.user_name.c_str(), "event", req.second.text.c_str(), "timestamp", timestamp) {
				json_array_append_new(cur_window.get(), payload);
			} __ALLOC_FAIL {
				iERROR("Failed to create broadcast JSON.");
				continue;
			}
		}
			break;
		case SYSTEM:
		{
			type = "system";
			__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s,s:I,s:I}",
			"type", type.c_str(), "user_name", req.second.user_name.c_str(), "event", req.second.text.c_str(), "timestamp", timestamp, "channel_id", req.second.channel_id) {
				json_array_append_new(cur_window.get(), payload);
			} __ALLOC_FAIL {
				iERROR("Failed to create broadcast JSON.");
				continue;
			}
		}
			break;
		default:
			break;
		}
		
    }
    CharDump dumped(json_dumps(cur_window.get(), 0));
    if (dumped) {
		// TODO
		if (!comm || !con_tracker) return;
		std::vector<fd_t> failed_fds = comm->broadcast(con_tracker->get_clients(), dumped.get());
		for (const fd_t& fd : failed_fds) {
			next_deletion.insert(fd);
		}
    }
}

void ChatServer::on_req(const User& from, const char* target, Json& root) {
	switch (hash(target))
	{
	case hash("message"):
	case hash("Message"):
	case hash("MESSAGE"):
		const char* text;
		msec64 timestamp;
		__UNPACK_JSON(root, "{s:s,s:I}", "text", &text, "timestamp", &timestamp) {
			std::string user_name;
			if (from.name) user_name = from.name;
			else user_name = "unknown";

			MessageReqDto msg_req = { .type = USER, .text = std::string(text), .timestamp = timestamp, .user_name = user_name };
			mq.push({const_cast<User*>(&from), msg_req});
		} __UNPACK_FAIL {
			iERROR("Malformed JSON message, missing timestamp or text.");
			return;
		}
		break;
	
	default:
		break;
	}
}

#pragma endregion