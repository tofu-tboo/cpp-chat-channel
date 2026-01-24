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
	task_runner.pushf(2, [this]() {
		check_lobby();
	});
}

ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }

	std::queue<ChannelReport> local_q = reports.pop_all();	
    while (!local_q.empty()) {
        ChannelReport req = local_q.front();
        local_q.pop();
        if (req.type == ChannelReport::JOIN) {
			if (req.dto.rejoin)
            	delete req.dto.rejoin;
			else if (req.dto.join)
				delete req.dto.join;
		}
    }
    channels.clear();
}

void ChannelServer::report(const ChannelReport& req) {
    reports.push(req);
}


#pragma region PROTECTED_FUNC
void ChannelServer::on_accept() {
	if (!con_tracker) return;
    fd_t client = accept(ServerBase::fd, nullptr, nullptr);
    if (client == FD_ERR) {
        iERROR("Failed to accept new connection.");
    } else {
        LOG("Accepted new connection: fd %d", client);
        try {
            con_tracker->add_client(client);
			set_user_name(client, "user_" + std::to_string(client)); // temporary username assignment

			last_act[client] = clock();
		} catch (const std::exception& e) {
            iERROR("%s", e.what());
            con_tracker->delete_client(client);
            close(client);
        }
    }
}

void ChannelServer::on_recv(const fd_t from) {
	try {
		if (!comm) return;
		std::vector<std::string> frames = comm->recv_frame(from);
		for (const std::string& frame : frames) {
            json_error_t err;
            Json root(json_loads(frame.c_str(), 0, &err));
            if (root.get() == nullptr) {
                iERROR("Failed to parse JSON: %s", err.text);
                continue;
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
        next_deletion.push_back(from);
    }
}

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
			__UNPACK_JSON(root, "{s:I,s:I,s:s}", "channel_id", &channel_id, "timestamp", &timestamp, "user_name", &user_name) {
				JoinReqDto req = { .channel_id = channel_id, .timestamp = timestamp, .user_name = std::string(user_name) };
				set_user_name(from, std::string(user_name)); // user_%d -> real user_name

                get_channel(channel_id)->join_and_logging(from, timestamp, false);

                con_tracker->delete_client(from);
            } __UNPACK_FAIL {
                iERROR("Malformed JSON message, missing channel_id or timestamp or user_name.");
            }
        }
        break;
    default:
        break;
    }
}

void ChannelServer::consume_report() {
	std::queue<ChannelReport> local_q = reports.pop_all();	
	while (!local_q.empty()) {
		ChannelReport req = local_q.front();
		local_q.pop();
		switch (req.type) {
		case ChannelReport::JOIN:
			{
				ch_id_t channel_id = req.dto.rejoin->channel_id;
				msec64 timestamp = req.dto.rejoin->timestamp;
				get_channel(channel_id)->join_and_logging(req.from, timestamp, true);
                delete req.dto.rejoin; // Consumer takes responsibility for deletion
			}
			break;
		}
    }
}
#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_channel(const ch_id_t channel_id) {
	if (channels.find(channel_id) == channels.end()) {
		Channel* channel = new Channel(this);
		channels[channel_id] = channel;
		LOG(_CG_ "Channel %u created." _EC_, channel_id);
	}
	return channels[channel_id];
}

void ChannelServer::check_lobby() {
	clock_t now = clock();
	std::unordered_map<fd_t, clock_t> next;
	for (const auto& [fd, t] : last_act) {
		double elapsed_secs = now - t;
		if (elapsed_secs < 5000) {
			next[fd] = t;
		} else {
			LOG("Lobby timeout: fd %d", fd);
			next_deletion.push_back(fd);
		}
	}
	last_act = std::move(next);
}
#pragma endregion