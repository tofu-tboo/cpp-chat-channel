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

void ChannelServer::report(const ChannelReport& req) {
    std::lock_guard<std::mutex> lock(report_mtx);
    reports.push(req);
}


#pragma region PROTECTED_FUNC
void ChannelServer::on_req(const fd_t from, const char* target, Json& root) {
    switch (hash(target))
    {
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
            ch_id_t channel_id;
			msec64 timestamp;
			const char* user_name;
			__UNPACK_JSON(root, "{s:I,s:I,s:s}", "channel_id", &channel_id, "timestamp", &timestamp, "user_id", &user_name) {
				JoinReqDto req = { .channel_id = channel_id, .timestamp = timestamp, .user_id = std::string(user_name) };
                get_channel(channel_id)->join_and_logging(from, req, false);

                con_tracker->delete_client(from);
				name_map[from] = std::string(user_name); // user_%d -> real user_name
            } __UNPACK_FAIL {
                iERROR("Malformed JSON message, missing channel_id or timestamp or user_id.");
            }
        }
        break;
    default:
        break;
    }
}

void ChannelServer::consume_report() {
	std::lock_guard<std::mutex> lock(report_mtx);
    while (!reports.empty()) {
        ChannelReport req = reports.front();
        reports.pop();
		switch (hash(req.type)) {
		case hash("join"):
		case hash("Join"):
		case hash("JOIN"):
			{
				ch_id_t channel_id = req.dto.rejoin->channel_id;
				msec64 timestamp = req.dto.rejoin->timestamp;
				JoinReqDto join_req = { .channel_id = channel_id, .timestamp = timestamp };
				// TODO?: make union join & rejoin
				get_channel(channel_id)->join_and_logging(req.from, join_req, true);
				delete req.dto.rejoin;
			}
			break;
		}
    }
}
#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_channel(const ch_id_t channel_id) {
	Channel* channel;
	if (channels.find(channel_id) == channels.end()) {
		channel = new Channel(this);
		channels[channel_id] = channel;
		LOG(_CG_ "Channel %u created." _EC_, channel_id);
	}
	return channel;
}
#pragma endregion