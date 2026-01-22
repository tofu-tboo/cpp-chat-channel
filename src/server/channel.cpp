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

void Channel::join_and_logging(const fd_t fd, JoinReqDto req, bool re) {
	try {
		join(fd);
		// TODO: logging using req (req.user_id, req.timestamp, etc.)
	} catch (...) {}
}

#pragma region PROTECTED_FUNC
void Channel::resolve_timestamps() {
    for (const auto& [from, msg_req] : mq) {        
        cur_msgs.emplace(msg_req.timestamp, std::pair<fd_t, std::string>{from, msg_req.text});
    }
    mq.clear();
}

void Channel::resolve_broadcast() {
    Json cur_window(json_array());
    for (const auto& [timestamp, msg] : cur_msgs) {
		__ALLOC_JSON_NEW(payload, "{s:s,s:s,s:s,s:I}",
			"type", "user", "user_id", name_map[msg.first].c_str(), "event", msg.second.c_str(), "timestamp", timestamp) {
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
			__UNPACK_JSON(root, "{s:I,s:I,s:s}", "channel_id", &channel_id, "timestamp", &timestamp, "user_id", nullptr) {
				UReportDto dto;
				dto.rejoin = new RejoinReqDto{ .channel_id = channel_id, .timestamp = timestamp };
				leave(from);
				// TODO: push mq sys msg
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
