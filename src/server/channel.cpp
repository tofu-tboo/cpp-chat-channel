#include "channel.h"
#include "channel_server.h"

Channel::Channel(NetworkService<User>* service, ChannelServer* srv, ch_id_t id, const int max_conn): ChatServer(service, max_conn, 100), channel_id(id), server(srv), empty_since(0) {}
Channel::~Channel() {}

bool Channel::init() {
	if (!ChatServer::init()) {
		return false;
	}
	task_runner.popb(TS_POLL);
	return true;
}

void Channel::proc() {
	task_runner.run();
}

void Channel::leave(typename NetworkService<User>::Session& ses, const MessageReqDto& msg) {
	cur_conn--;
	if (cur_conn == 0) {
		empty_since = now_ms();
	}

	// service->change_session_group(&ses, INT_MIN);

	std::unique_lock<std::shared_mutex> lock(mq_mtx);
	mq.push({&ses, msg});
	lock.unlock();

	LOG(_CR_ "[Leave] User %p left channel %u at %lu" _EC_, ses.user, channel_id, msg.timestamp);
}

void Channel::join(typename NetworkService<User>::Session& ses, const MessageReqDto& msg) {
	cur_conn++;
	if (empty_since > 0) {
		empty_since = 0;
	}

	service->change_session_group(&ses, channel_id);
	service->register_handler(&ses, this);

	std::unique_lock<std::shared_mutex> lock(mq_mtx);
	mq.push({&ses, msg});
	lock.unlock();

	LOG(_CB_ "[Join] User %p joined channel %u at %lu" _EC_, ses.user, channel_id, msg.timestamp);
}

void Channel::leave_and_logging(typename NetworkService<User>::Session& ses) {
	User* user = ses.user;
	MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = now_ms(), .channel_id = channel_id };


	if (user->name) sys_msg.user_name = user->name;
	else if (ses.group != INT_MIN) {
		resv_close(&ses);
		return;
	}

	leave(ses, sys_msg);
}

void Channel::join_and_logging(typename NetworkService<User>::Session& ses, bool re) {
	User* user = ses.user;
	MessageReqDto sys_msg = { .type = SYSTEM, .timestamp = now_ms(), .channel_id = channel_id };

	
	if (user->name) sys_msg.user_name = user->name;
	else {
		return;
	}

	sys_msg.text = re ? "rejoin" : "join";

	join(ses, sys_msg);
}

bool Channel::ping_pool() {
	return cur_conn < max_conn;
}

msec64 Channel::get_empty_since() const { return empty_since; }
// bool Channel::is_stopped() const { return stop_flag.load(); }

#pragma region PROTECTED_FUNC

void Channel::on_accept(typename NetworkService<User>::Session& ses) {}

void Channel::on_req(const typename NetworkService<User>::Session& ses, const char* target, Json& root) {
    switch (hash(target)) {
    case hash("message"):
    case hash("Message"):
    case hash("MESSAGE"):
		ChatServer::on_req(ses, target, root);
        break;
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
			ch_id_t ch_to;
			__UNPACK_JSON(root, "{s:i}", "channel_id", &ch_to) {
				if (ch_to == channel_id) return;
				
				server->switch_channel(const_cast<typename NetworkService<User>::Session&>(ses), channel_id, ch_to);
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}

void Channel::resolve_broadcast() {
    Json cur_window(json_array());
	std::shared_lock<std::shared_mutex> lock(cm_mtx);

    for (const auto& [timestamp, req_pair] : cur_msgs) {
		const auto& req = req_pair.second;
		std::string type;
		switch (req.type)
		{
		case USER:
		{
			type = "user";
			__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s}",
			"type", type.c_str(), "user_name", req.user_name.c_str(), "event", req.text.c_str()) {
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
			"type", type.c_str(), "user_name", req.user_name.c_str(), "event", req.text.c_str(), "channel_id", req.channel_id) {
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
	lock.unlock();
	
	if (json_array_size(cur_window.get()) == 0) return;

    CharDump dumped(json_dumps(cur_window.get(), 0));
    if (dumped) {
		service->broadcast_group_async(channel_id, std::string(dumped.get()));
    }
}

void Channel::free_user(typename NetworkService<User>::Session& ses) {
	User* user = ses.user;
	
	MessageReqDto msg = { .type = SYSTEM, .text = "leave", .timestamp = now_ms(), .user_name = user->name ? user->name : "unknown", .channel_id = channel_id };

	service->change_session_group(&ses, INT_MIN);
	service->register_handler(&ses, nullptr);

	std::unique_lock<std::shared_mutex> lock(mq_mtx);
	mq.push({&ses, msg});
	lock.unlock();

	if (user->name) free(user->name);
	user->name = nullptr;
}

#pragma endregion