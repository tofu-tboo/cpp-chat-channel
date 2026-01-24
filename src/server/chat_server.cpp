

#include "chat_server.h"
#include "user_manager.h"


ChatServer::ChatServer(const int max_fd, const msec to): TypedFrameServer(max_fd, to) {
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
	if (!con_tracker) return;
    for (const fd_t fd : next_deletion) {
		try {
        	con_tracker->delete_client(fd);
		} catch (...) {
			continue;
		}
		UserManager::remove_user_name(fd);
        close(fd);
		comm->clear_buffer(fd);
        LOG("Normally Disconnected: fd %d", fd);
    }
}

void ChatServer::resolve_timestamps() {
    std::queue<std::pair<fd_t, MessageReqDto>> local_q = mq.pop_all();
	while (!local_q.empty()) {
        std::pair<fd_t, MessageReqDto> item = std::move(local_q.front());
        local_q.pop();

		cur_msgs.emplace(item.second.timestamp, std::pair<fd_t, MessageReqDto>(item.first, item.second));
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
		if (!comm || !con_tracker) return;
		std::vector<fd_t> failed_fds = comm->broadcast(con_tracker->get_clients(), dumped.get());
		for (const fd_t& fd : failed_fds) {
			next_deletion.insert(fd);
		}
    }
}

void ChatServer::on_req(const fd_t from, const char* target, Json& root) {
	switch (hash(target))
	{
	case hash("message"):
	case hash("Message"):
	case hash("MESSAGE"):
		const char* text;
		msec64 timestamp;
		__UNPACK_JSON(root, "{s:s,s:I}", "text", &text, "timestamp", &timestamp) {
			std::string user_name;
			if (!UserManager::get_user_name(from, user_name)) {
				return;
			}
			MessageReqDto msg_req = { .type = USER, .text = std::string(text), .timestamp = timestamp, .user_name = user_name };
			mq.push({from, msg_req});
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