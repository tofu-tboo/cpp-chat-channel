

#include "chat_server.h"
#include "user_manager.h"


ChatServer::ChatServer(NetworkService<User>* service, const int max_fd, const msec to): TypedJsonFrameServer(service, max_fd, to) {}

ChatServer::~ChatServer() {
	cur_msgs.clear();
}

bool ChatServer::init() {
	if (TypedJsonFrameServer::init()) {
		task_runner.pushb(TS_PRE, [this]() {
			cur_msgs.clear();
		});
		task_runner.pushf(TS_LOGIC, [this]() {
			resolve_timestamps();
			resolve_broadcast();
    	});
		return true;
	}
	return false;
}

#pragma region PROTECTED_FUNC
void ChatServer::resolve_deletion() {
    for (auto* session : next_deletion) {
		User* user = session->user;
		if (user->name) free(user->name);
		user->name = nullptr;
        service->close_async(session, "Server closed.");
        LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

void ChatServer::resolve_timestamps() {
    auto local_q = mq.pop_all();
	while (!local_q.empty()) {
        auto item = std::move(local_q.front());
        local_q.pop();

		cur_msgs.emplace(item.second.timestamp, item);
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
			__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s}",
			"type", type.c_str(), "user_name", req.second.user_name.c_str(), "event", req.second.text.c_str()) {
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
			__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s,s:I}",
			"type", type.c_str(), "user_name", req.second.user_name.c_str(), "event", req.second.text.c_str(), "channel_id", req.second.channel_id) {
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
	if (json_array_size(cur_window.get()) == 0) return;
	
    CharDump dumped(json_dumps(cur_window.get(), 0));
    if (dumped) {
		service->broadcast_async(std::string(dumped.get()));
    }
}

void ChatServer::on_req(const typename NetworkService<User>::Session& ses, const char* target, Json& root) {
	const User& from = *ses.user;
	switch (hash(target))
	{
	case hash("message"):
	case hash("Message"):
	case hash("MESSAGE"): 
	{
		const char* text;
		msec64 timestamp = now_ms();
		__UNPACK_JSON(root, "{s:s}", "text", &text) {
			std::string user_name;
			if (from.name) user_name = from.name;
			else user_name = "unknown";

			MessageReqDto msg_req = { .type = USER, .text = std::string(text), .timestamp = timestamp, .user_name = user_name };
			mq.push({const_cast<typename NetworkService<User>::Session*>(&ses), msg_req});
		} __UNPACK_FAIL {
			iERROR("Malformed JSON message, missing text field.");
			return;
		}
		break;
	}
	default:
		break;
	}
}

#pragma endregion