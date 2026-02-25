#include "channel.h"
#include "channel_server.h"
#include "../libs/json_translator.h"
#include "../libs/chat_res_dto.h"

Channel::Channel(std::shared_ptr<NetworkService<User>> service, ChannelServer* srv, ch_id_t id, const int max_conn): ChatServer(std::move(service), max_conn), channel_id(id), server(srv), empty_since(0) {}
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

	LOG(_CR_ "[Leave] User %p left channel %u at %lu" _EC_, ses.user, channel_id, sys_msg.timestamp);
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

	LOG(_CB_ "[Join] User %p joined channel %u at %lu" _EC_, ses.user, channel_id, sys_msg.timestamp);
}

bool Channel::ping_pool() {
	return cur_conn < max_conn;
}

msec64 Channel::get_empty_since() const { return empty_since; }
// bool Channel::is_stopped() const { return stop_flag.load(); }

#pragma region PROTECTED_FUNC

void Channel::on_accept(typename NetworkService<User>::Session& ses) {}

void Channel::handle_request(typename NetworkService<User>::Session& ses, std::unique_ptr<Request> req) {
	JsonRequest* json_req = dynamic_cast<JsonRequest*>(req.get());
	if (!json_req) return;

	ChatReqDto dto(&json_req->root);

	switch_hash(dto.type.c_str()) {
		case_hash("message"):
		case_hash("Message"):
		case_hash("MESSAGE"):
				// Delegate to ChatServer for messages
				ChatServer::handle_request(ses, std::move(req));
			break;
		case_hash("join"):
		case_hash("Join"):
		case_hash("JOIN"):
			{
				if (dto.channel_id == channel_id) return;
				server->switch_channel(const_cast<typename NetworkService<User>::Session&>(ses), channel_id, dto.channel_id);
			}
		default:
			break;
    }
}

void Channel::resolve_broadcast() {
	ChatResDtoArray dtos;
	std::shared_lock<std::shared_mutex> lock(cm_mtx);

    for (const auto& [timestamp, req_pair] : cur_msgs) {
		const auto& req = req_pair.second;
		switch (req.type)
		{
			case USER:
			{
				ChatResDto dto;
				dto.type = "user";
				dto.event = req.text;
				dto.user_name = req.user_name;
				dtos.entries.push_back(std::move(dto));
				break;
			}
			case SYSTEM:
			{
				ChatResDto dto;
				dto.type = "system";
				dto.event = req.text;
				dto.user_name = req.user_name;
				dto.channel_id = req.channel_id;
				dtos.entries.push_back(std::move(dto));
				break;
			}
		default:
			break;
		}
		
    }
	lock.unlock();
	
	if (dtos.entries.empty()) return;

    std::string frame = dtos.to_frame();
    if (!frame.empty()) {
		service->broadcast_group_async(channel_id, frame);
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