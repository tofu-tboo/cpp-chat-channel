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
        std::string uid;
		std::string event;

        if (re && req.rejoin) {
            ts = req.rejoin->timestamp;
            // Rejoin의 경우 DTO에 user_name가 없으므로 서버의 name_map에서 조회하거나 알 수 없음 처리
			if (!get_user_name(fd, uid)) {
				next_deletion.push_back(fd); // 이름을 알 수 없으면 강제 퇴장
				return;
			}

			event = "rejoin";
        } else if (!re && req.join) {
            ts = req.join->timestamp;
            uid = req.join->user_name;
			event = "join";
        }


		MessageReqDto sys_msg = { .type = SYSTEM, .text = event, .timestamp = ts };

		mq.push_back({fd, sys_msg});

		join(fd);
        LOG(_CB_ "[Join] User %s (fd: %d) joined channel %u at %lu" _EC_, uid.c_str(), fd, channel_id, ts);
	} catch (const std::exception& e) {
        iERROR("Logging failed: %s", e.what());
    }
}

#pragma region PROTECTED_FUNC

void Channel::on_accept() {} // accept only occured in lobby(ChannelServer)
void Channel::on_req(const fd_t from, const char* target, Json& root) {
    switch (hash(target)) {
    case hash("message"):
		ChatServer::on_req(from, target, root);
        break;
	// TODO: leave === disconnection
    case hash("join"):
        {
			ch_id_t channel_id;
			msec64 timestamp;
			__UNPACK_JSON(root, "{s:I,s:I,s:s}", "channel_id", &channel_id, "timestamp", &timestamp, "user_name", nullptr) {
				UReportDto dto;
				dto.rejoin = new RejoinReqDto{ .channel_id = channel_id, .timestamp = timestamp };

				MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp };
				mq.push_back({from, sys_msg});

				leave(from);

				server->report({"join", from, dto}); // Request switch channel
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}
#pragma endregion
