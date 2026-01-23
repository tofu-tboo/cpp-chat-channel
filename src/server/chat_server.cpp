

#include "chat_server.h"


ChatServer::ChatServer(const int max_fd, const msec to): ServerBase(max_fd, to) {
	task_runner.pushb(0, [this]() {
		cur_msgs.clear();
	});
}

ChatServer::~ChatServer() {
	cur_msgs.clear();
	mq.clear();
}

#pragma region PROTECTED_FUNC

void ChatServer::resolve_payload(const fd_t from, const std::string& payload) {
    json_error_t err;
    Json root(json_loads(payload.c_str(), 0, &err));
    if (root.get() == nullptr) {
        iERROR("Failed to parse JSON: %s", err.text);
        return;
    }
    
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        on_req(from, type, root);
    } __UNPACK_FAIL {
        iERROR("Malformed JSON message, missing type.");
    }
}

void ChatServer::resolve_timestamps() {
    for (const auto& [from, msg_req] : mq) {
		MsgType type = msg_req.type;
		if (type == USER) {
        	cur_msgs.emplace(msg_req.timestamp, std::pair<fd_t, std::string>{from, msg_req.text});
		} else if (type == SYSTEM) {
			cur_msgs.emplace(msg_req.timestamp, std::pair<fd_t, std::string>{ServerBase::fd, msg_req.text});
		}
    }	
    mq.clear();
}

void ChatServer::resolve_broadcast() {
    Json cur_window(json_array());
    for (const auto& [timestamp, msg] : cur_msgs) {
		const char* user_name = nullptr;
		if (!get_user_name(msg.first, user_name)) {
			continue;
		}
		__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s,s:I}",
			"type", msg.first == ServerBase::fd ? "system" : "user", "user_name", user_name, "event", msg.second.c_str(), "timestamp", timestamp) {
        	json_array_append_new(cur_window.get(), payload);
		} __ALLOC_FAIL {
			iERROR("Failed to create broadcast JSON.");
			continue;
		}
    }
    CharDump dumped(json_dumps(cur_window.get(), 0));
    if (dumped) {
		if (!comm || !con_tracker) return;
		std::vector<fd_t> failed_fds = comm->broadcast(con_tracker->get_clients(), dumped.get());
		for (const fd_t& fd : failed_fds) {
			next_deletion.push_back(fd);
		}
    }
}

void ChatServer::on_req(const fd_t from, const char* target, Json& root) {
	switch (hash(target))
	{
	case hash("message"):
		const char* text;
		msec64 timestamp;
		__UNPACK_JSON(root, "{s:s,s:s,s:I}", "type", nullptr, "text", &text, "timestamp", &timestamp) {
			MessageReqDto msg_req = { .type = USER, .text = std::string(text), .timestamp = timestamp };
			mq.push_back({from, msg_req});
		} __UNPACK_FAIL {
			iERROR("Malformed JSON message, missing timestamp or text.");
			return;
		}
		break;
	
	default:
		break;
	}
}

void ChatServer::on_recv(const fd_t from) {
	try {
		if (!comm) return;
		std::vector<std::string> frames = comm->recv_frame(from);
		for (const std::string& frame : frames) {
			resolve_payload(from, frame);
		}
	} catch (const std::exception& e) {
        iERROR("%s", e.what());
        next_deletion.push_back(from);
    }

	resolve_timestamps();
	resolve_broadcast();
}

#pragma endregion