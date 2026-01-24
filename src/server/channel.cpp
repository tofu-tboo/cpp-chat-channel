#include "channel.h"
#include "channel_server.h"

Channel::Channel(ChannelServer* srv, ch_id_t id, const int max_fd): ChatServer(max_fd, 100), channel_id(id), server(srv) {
    stop_flag.store(false);
    worker = std::thread(&Channel::proc, this);

	task_runner.pushf(2, [this]() {
		resolve_pool();
	});
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

void Channel::leave(const fd_t fd, const MessageReqDto& msg) {
	leave_pool.emplace(fd, msg);
    // try {
    //     con_tracker->delete_client(fd);
    // } catch (const std::exception& e) {
    //     iERROR("%s", e.what());
    // }
}

void Channel::join(const fd_t fd, const MessageReqDto& msg) {
	join_pool.emplace(fd, msg);
    // try {
    //     con_tracker->add_client(fd);
    // } catch (const std::exception& e) {
    //     iERROR("%s", e.what());
    // }
}

void Channel::leave_and_logging(const fd_t fd, msec64 timestamp) {
	try {		
		MessageReqDto sys_msg = { .type = SYSTEM, .text = "leave", .timestamp = timestamp };
		if (!get_user_name(fd, sys_msg.user_name)) {
			next_deletion.insert(fd); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

	    mq.push({fd, sys_msg});

		leave(fd, sys_msg);
	} catch (const std::exception& e) {
		iERROR("Logging failed: %s", e.what());
	}
}

void Channel::join_and_logging(const fd_t fd, msec64 timestamp, bool re) {
	try {		
		MessageReqDto sys_msg = { .type = SYSTEM, .timestamp = timestamp};

		if (!get_user_name(fd, sys_msg.user_name)) {
			next_deletion.insert(fd); // 이름을 알 수 없으면 강제 퇴장
			return;
		}

		sys_msg.text = re ? "rejoin" : "join";

	    mq.push({fd, sys_msg});

		join(fd, sys_msg);
	} catch (const std::exception& e) {
        iERROR("Logging failed: %s", e.what());
    }
}

bool Channel::ping_pool() {
	std::lock_guard<std::mutex> lock(pool_mtx);
	return !con_tracker->is_full();
}

#pragma region PROTECTED_FUNC

void Channel::on_accept() {} // accept only occured in lobby(ChannelServer)
void Channel::resolve_deletion() {
	if (!con_tracker) return;
    for (const fd_t fd : next_deletion) {
		leave_and_logging(fd, std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
		remove_user_name(fd);
        close(fd);
		comm->clear_buffer(fd);
        LOG("Normally Disconnected: fd %d", fd);
    }
}

void Channel::resolve_pool() {
	std::lock_guard<std::mutex> lock(pool_mtx);
	if (!con_tracker) return;
	std::unordered_map<fd_t, MessageReqDto> local_q = std::move(join_pool);
	for (const auto& [fd, msg] : local_q) {
		try {
			con_tracker->add_client(fd);
        	LOG(_CB_ "[Join] User (fd: %d) joined channel %u at %lu" _EC_, fd, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			if (dynamic_cast<const coded_runtime_error*>(&e) != nullptr) {
				const coded_runtime_error& cre = static_cast<const coded_runtime_error&>(e);
				if (cre.code == POOL_FULL) {
					UReportDto dto;
					dto.join_block = new JoinBlockReqDto{ .channel_id = channel_id, .timestamp = msg.timestamp };
					
					server->report({ChannelServer::ChannelReport::JOIN_BLOCK, fd, dto});
					iERROR("Channel %u is full.", channel_id);
					continue;
				}
			}
			iERROR("%s", e.what());
		}
	}
	local_q = std::move(leave_pool);
	for (const auto& [fd, msg] : local_q) {
		try {
			con_tracker->delete_client(fd);
			LOG(_CR_ "[Leave] User (fd: %d) left channel %u at %lu" _EC_, fd, channel_id, msg.timestamp);
		} catch (const std::exception& e) {
			iERROR("%s", e.what());
		}
	}
}

void Channel::on_req(const fd_t from, const char* target, Json& root) {
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
			ch_id_t channel_id;
			msec64 timestamp;
			__UNPACK_JSON(root, "{s:I,s:I}", "channel_id", &channel_id, "timestamp", &timestamp) {
				UReportDto dto;
				dto.rejoin = new RejoinReqDto{ .channel_id = channel_id, .timestamp = timestamp };

				try {
					server->report({ChannelServer::ChannelReport::JOIN, from, dto});
					leave_and_logging(from, timestamp);
				} catch (const std::exception& e) {
					if (dynamic_cast<const coded_runtime_error*>(&e) != nullptr) {
						const coded_runtime_error& cre = static_cast<const coded_runtime_error&>(e);
						if (cre.code == POOL_FULL) {
							comm->send_frame(from, std::string(R"({"type":"error","message":"The channel is full."})"));
						}
					}
				}
			} __UNPACK_FAIL {
				iERROR("Malformed JSON message, missing channel_id.");
			}
		}
    default:
        break;
    }
}

void Channel::on_recv(const fd_t from) {
	try {
		if (!comm) return;
		std::vector<std::string> frames = comm->recv_frame(from);
		for (const std::string& frame : frames) {
			json_error_t err;
			Json root(json_loads(frame.c_str(), 0, &err));
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
	} catch (const std::exception& e) {
		iERROR("%s", e.what());
        next_deletion.insert(from);
    }
}

#pragma endregion