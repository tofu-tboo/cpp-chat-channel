#include "channel.h"
#include "channel_server.h"

Channel::Channel(ChannelServer* srv): ChatServer(256, 100), server(srv) {
    stop_flag.store(false);
    worker = std::thread(&Channel::proc, this);
}
Channel::~Channel() {
    stop_flag.store(true);
    if (worker.joinable()) {
        worker.join();
    }
}

void Channel::proc() {
    while (!stop_flag.load(std::memory_order_relaxed)) {
        try {
			task_runner.run();
        } catch (const std::exception& e) {
            iERROR("%s", e.what());
        }
    }
}

void Channel::leave(const fd_t fd) {
    try {
        con_tracker->delete_client(fd);
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}
void Channel::join(const fd_t fd) {
    try {
        con_tracker->add_client(fd);
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

void Channel::leave_and_logging(const fd_t fd, msec64 timestamp) {
	try {		
		std::string user_name;
		if (!get_user_name(fd, user_name)) {
			next_deletion.push_back(fd); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp };
	    mq.push({fd, sys_msg});

		leave(fd);
		LOG(_CR_ "[Leave] User (fd: %d) left channel %u at %lu" _EC_, fd, channel_id, timestamp);
	} catch (const std::exception& e) {
		iERROR("Logging failed: %s", e.what());
	}
}

void Channel::join_and_logging(const fd_t fd, msec64 timestamp, bool re) {
	try {		
        std::string uname;
		std::string event;

		if (!get_user_name(fd, uname)) {
			next_deletion.push_back(fd); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

		event = re ? "rejoin" : "join";

		MessageReqDto sys_msg = { .type = SYSTEM, .text = event, .timestamp = timestamp };
	    mq.push({fd, sys_msg});

		join(fd);
        LOG(_CB_ "[Join] User (fd: %d) joined channel %u at %lu" _EC_, fd, channel_id, timestamp);
	} catch (const std::exception& e) {
        iERROR("Logging failed: %s", e.what());
    }
}

#pragma region PROTECTED_FUNC

void Channel::on_accept() {} // accept only occured in lobby(ChannelServer)
void Channel::on_req(const fd_t from, const char* target, Json& root) {
    switch (hash(target)) {
    case hash("message"):
    case hash("Message"):
    case hash("MESSAGE"):
		ChatServer::on_req(from, target, root);
        break;
	// TODO: leave === disconnection
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
			ch_id_t channel_id;
			msec64 timestamp;
			__UNPACK_JSON(root, "{s:I,s:I}", "channel_id", &channel_id, "timestamp", &timestamp) {
				UReportDto dto;
				dto.rejoin = new RejoinReqDto{ .channel_id = channel_id, .timestamp = timestamp };

				leave_and_logging(from, timestamp);

				server->report({ChannelServer::ChannelReport::JOIN, from, dto}); // Request switch channel
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}
#pragma endregion
