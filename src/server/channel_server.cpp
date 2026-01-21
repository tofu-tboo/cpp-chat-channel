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
					channels[req.target]->join(req.fd);
					user_map[req.fd] = req.target;
					LOG(_CG_ "User %d moved to channel %u." _EC_, req.fd, req.target);

                }
                channels[req.target]->join(req.fd);
                user_map[req.fd] = req.target;
                LOG(_CG_ "User %d moved to channel %u." _EC_, req.fd, req.target);
            }
        }
    });
}

ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }

    channels.clear();
    user_map.clear();
}

void ChannelServer::report(const ChannelReq& req) {
    std::lock_guard<std::mutex> lock(req_mtx);
    req_queue.push(req);
}


#pragma region PROTECTED_FUNC
void ChannelServer::on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) {
    switch (hash(target))
    {
    // case hash("message"):
    // case hash("Message"):
    // case hash("MESSAGE"):
    //     break;
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
            ch_id_t channel_id;
            __UNPACK_JSON(root, "{s:I}", "channel_id", &channel_id) {
                if (channels.find(channel_id) == channels.end()) {
                    channels[channel_id] = new Channel(this);
                    LOG(_CG_ "Channel %u created." _EC_, channel_id);
                }

                if (user_map.find(fd) != user_map.end()) {
                    channels[channel_id]->leave(fd);
                }
                con_tracker->delete_client(fd);
                channels[channel_id]->join(fd);
            } __UNPACK_FAIL {
                iERROR("Malformed JSON message, missing channel_id.");
            }
        }
        break;
    // case hash("leave"):
    // case hash("Leave"):
    // case hash("LEAVE"):
    //     {
    //         ch_id_t channel_id;
    //         __UNPACK_JSON(root, "{s:I}", "channel_id", &channel_id) {
    //             auto it = channels.find(channel_id);
    //             if (it != channels.end()) {
    //                 delete it->second;
    //                 channels.erase(it);
    //                 LOG(_CY_ "Channel %u deleted." _EC_, channel_id);
    //             }
    //         } __UNPACK_FAIL {
    //             iERROR("Malformed JSON message, missing channel_id.");
    //         }
    //     }
    //     break;
    // case hash("quit"):
    //     break;
    default:
        break;
    }
}
#pragma endregion