#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

#include "channel_server.h"
#include "../libs/util.h"

ChannelServer::ChannelServer(const int max_fd, const msec to): ServerBase(max_fd, to) {
    // Periodically process switch requests from channels
    task_runner.pushb(0, [this]() {
        consume_report();
    });
}

ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }

    channels.clear();
}

void ChannelServer::report(const ChannelReq& req) {
    std::lock_guard<std::mutex> lock(req_mtx);
    req_queue.push(req);
}


#pragma region PROTECTED_FUNC
void ChannelServer::on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) {
    switch (hash(target))
    {
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
            ch_id_t channel_id;
			const char* user_name;
			__UNPACK_JSON(root, "{s:I,s:s}", "channel_id", &channel_id, "user_id", &user_name) {
                if (channels.find(channel_id) == channels.end()) {
                    channels[channel_id] = new Channel(this);
                    LOG(_CG_ "Channel %u created." _EC_, channel_id);
                }

                channels[channel_id]->join(from);

				// TODO: 다음 두 줄을 channel에서 upward 방향 처리에서 join/rejoin 구분하기
                con_tracker->delete_client(from);
				name_map[from] = std::string(user_name); // user_%d -> real user_name
            } __UNPACK_FAIL {
                iERROR("Malformed JSON message, missing channel_id.");
            }
        }
        break;
    default:
        break;
    }
}

// TODO: 삭제 고려
void ChannelServer::consume_report() {
	std::lock_guard<std::mutex> lock(req_mtx);
    while (!req_queue.empty()) {
        ChannelReq req = req_queue.front();
        req_queue.pop();
    	if (req.type == ChannelReq::SWITCH) {
			if (req.is_leave) {
				// Move to Lobby
				try {
					con_tracker->add_client(req.fd);
					user_map.erase(req.fd);
					LOG(_CG_ "User %d returned to lobby." _EC_, req.fd);
				} catch (...) {}
			} else {
				// Move to Target Channel
				if (channels.find(req.target) == channels.end()) {
					channels[req.target] = new Channel(this);
					LOG(_CG_ "Channel %u created." _EC_, req.target);
				}
				channels[req.fd]->leave(req.fd);
				channels[req.target]->join(req.fd);
				user_map[req.fd] = req.target;
            }
        }
    }
}
#pragma endregion