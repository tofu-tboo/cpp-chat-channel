#include "channel.h"
#include "channel_server.h"
#include "user_manager.h"

Channel::Channel(NetworkService<User>* service, ChannelServer* srv, ch_id_t id, const int max_fd): ChatServer(service, max_fd, 100), channel_id(id), server(srv), paused(false) {
}
Channel::~Channel() {
}

void Channel::process() {
    try {
        resolve_pool();
        resolve_timestamps();
        resolve_broadcast();
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

void Channel::leave(typename NetworkService<User>::Session* ses, const MessageReqDto& msg) {
	leave_pool.emplace(ses, msg);
}

void Channel::join(typename NetworkService<User>::Session* ses, const MessageReqDto& msg) {
	if (users.empty()) {
		empty_since.store(0);
	}
	join_pool.emplace(ses, msg);
}

void Channel::leave_and_logging(typename NetworkService<User>::Session* ses, msec64 timestamp) {
	try {		
		User* user = ses->user;
		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp, .channel_id = channel_id };
		if (user->name) sys_msg.user_name = user->name;
		else {
			next_deletion.insert(ses); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

		leave(ses, sys_msg);
	} catch (const std::exception& e) {
		iERROR("Logging failed: %s", e.what());
	}
}

void Channel::join_and_logging(typename NetworkService<User>::Session* ses, msec64 timestamp, bool re) {
	try {		
		User* user = ses->user;
		MessageReqDto sys_msg = { .type = SYSTEM, .timestamp = timestamp, .channel_id = channel_id };

		if (user->name) sys_msg.user_name = user->name;
		else {
			return;
		}

		sys_msg.text = re ? "rejoin" : "join";

		join(ses, sys_msg);
	} catch (const std::exception& e) {
        iERROR("Logging failed: %s", e.what());
    }
}

bool Channel::ping_pool() {
	return (users.size() + join_pool.size()) < 256; // Hardcoded max_fd for now or pass via constructor
}

void Channel::wait_stop_pooling() {
	pool_mtx.lock();
}

void Channel::start_pooling() {
	pool_mtx.unlock();
}

msec64 Channel::get_empty_since() const { return empty_since.load(); }
bool Channel::is_stopped() const { return stop_flag.load(); }

#pragma region PROTECTED_FUNC
void Channel::set_network(const char* port) {}

void Channel::on_accept(typename NetworkService<User>::Session& client) {} // accept only occured in lobby(ChannelServer)
void Channel::resolve_deletion() {
    for (auto* session : next_deletion) {
		User* user = session->user;
		msec64 timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		
		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp, .channel_id = channel_id };
		if (user->name) sys_msg.user_name = user->name;
		else {
			sys_msg.user_name = "unknown";
		}

		mq.push({session, sys_msg});

		// try {
		// 	con_tracker->delete_client(fd);
		// } catch (...) {}

		// UserManager::remove_user_name(fd);
        service->close_async(session, "Bye");
        LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

void Channel::resolve_pool() {
	std::lock_guard<std::mutex> lock(pool_mtx);
	auto local_join_q = std::move(join_pool);
	for (const auto& [ses, msg] : local_join_q) {
		try {
			users.insert(ses);
		    mq.push({ses, msg});
        	LOG(_CB_ "[Join] User %p joined channel %u at %lu" _EC_, ses->user, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			iERROR("%s", e.what());
		}
	}
	auto local_leave_q = std::move(leave_pool);
	for (const auto& [ses, msg] : local_leave_q) {
		try {
			users.erase(ses);
		    mq.push({ses, msg});
			LOG(_CR_ "[Leave] User %p left channel %u at %lu" _EC_, ses->user, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			iERROR("%s", e.what());
		}
	}

	if (users.empty()) {
		empty_since.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	}
}

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
			msec64 timestamp;
			__UNPACK_JSON(root, "{s:I,s:I}", "channel_id", &ch_to, "timestamp", &timestamp) {
				if (ch_to == channel_id) return;
				
				UReportDto dto;
				dto.join = new JoinReqDto{ .ch_from = channel_id, .ch_to = ch_to, .timestamp = timestamp };

				server->report({ChannelServer::ChannelReport::JOIN, const_cast<typename NetworkService<User>::Session*>(&ses), dto});
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}

#pragma endregion