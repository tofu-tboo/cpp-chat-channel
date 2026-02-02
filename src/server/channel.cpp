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

void Channel::leave(User* user, const MessageReqDto& msg) {
	leave_pool.emplace(user, msg);
}

void Channel::join(User* user, const MessageReqDto& msg) {
	if (users.empty()) {
		empty_since.store(0);
	}
	join_pool.emplace(user, msg);
}

void Channel::leave_and_logging(User* user, msec64 timestamp) {
	try {		
		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp, .channel_id = channel_id };
		if (user->name) sys_msg.user_name = user->name;
		else {
			next_deletion.insert(user); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

		leave(user, sys_msg);
	} catch (const std::exception& e) {
		iERROR("Logging failed: %s", e.what());
	}
}

void Channel::join_and_logging(User* user, msec64 timestamp, bool re) {
	try {		
		MessageReqDto sys_msg = { .type = SYSTEM, .timestamp = timestamp, .channel_id = channel_id };

		if (user->name) sys_msg.user_name = user->name;
		else {
			return;
		}

		sys_msg.text = re ? "rejoin" : "join";

		join(user, sys_msg);
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

void Channel::on_accept(User& client) {} // accept only occured in lobby(ChannelServer)
void Channel::resolve_deletion() {
    for (User* user : next_deletion) {
		msec64 timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		
		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp, .channel_id = channel_id };
		if (user->name) sys_msg.user_name = user->name;
		else {
			sys_msg.user_name = "unknown";
		}

		mq.push({user, sys_msg});

		// try {
		// 	con_tracker->delete_client(fd);
		// } catch (...) {}

		// UserManager::remove_user_name(fd);
        service->close_async(user, "Bye");
        LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

void Channel::resolve_pool() {
	std::lock_guard<std::mutex> lock(pool_mtx);
	std::unordered_map<User*, MessageReqDto> local_q = std::move(join_pool);
	for (const auto& [user, msg] : local_q) {
		try {
			users.insert(user);
		    mq.push({user, msg});
        	LOG(_CB_ "[Join] User %p joined channel %u at %lu" _EC_, user, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			iERROR("%s", e.what());
		}
	}
	local_q = std::move(leave_pool);
	for (const auto& [user, msg] : local_q) {
		try {
			users.erase(user);
		    mq.push({user, msg});
			LOG(_CR_ "[Leave] User %p left channel %u at %lu" _EC_, user, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			iERROR("%s", e.what());
		}
	}

	if (users.empty()) {
		empty_since.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	}
}

void Channel::on_req(const User& from, const char* target, Json& root) {
    switch (hash(target)) {
    case hash("message"):
    case hash("Message"):
    case hash("MESSAGE"):
		ChatServer::on_req(from, target, root);
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

				server->report({ChannelServer::ChannelReport::JOIN, const_cast<User*>(&from), dto});
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}

#pragma endregion