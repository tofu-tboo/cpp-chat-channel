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
    try {
        while (!stop_flag.load(std::memory_order_relaxed)) {
			task_runner.run();
        }
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
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

void Channel::join_and_logging(const fd_t fd, UJoinDto req, bool re) {
	try {		
        msec64 ts = 0;
        std::string uname;
		std::string event;

        if (re && req.rejoin) {
            ts = req.rejoin->timestamp;
			if (!get_user_name(fd, uname)) {
				next_deletion.push_back(fd); // 이름을 알 수 없으면 강제 퇴장
				return;
			}

			event = "rejoin";
        } else if (!re && req.join) {
            ts = req.join->timestamp;
            uname = req.join->user_name;
			event = "join";
        }


		MessageReqDto sys_msg = { .type = SYSTEM, .text = event, .timestamp = ts };
	    mq.push({fd, sys_msg});

		join(fd);
        LOG(_CB_ "[Join] User (fd: %d) joined channel %u at %lu" _EC_, fd, channel_id, ts);
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

				MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp };
	    		mq.push({fd, sys_msg});

				leave(from);
		
				LOG(_CB_ "[Leave] User (fd: %d) left channel %u at %lu" _EC_, from, channel_id, timestamp);

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
