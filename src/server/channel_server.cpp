#include "channel_server.h"
#include "../libs/util.h"
#include "user_manager.h"

ChannelServer::ChannelServer(const int max_fd, const int ch_max_fd, const msec to): TypedFrameServer(max_fd, to), ch_max_fd(ch_max_fd) {
    // Periodically process switch requests from channels
    task_runner.pushb(TS_PRE, [this]() {
        consume_report();
    });
	task_runner.pushf(TS_LOGIC, AsThrottle([this]() {
		check_lobby();
		check_channels();
	}, 1000));
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
			if (req.dto.join)
				delete req.dto.join;
		}
    }
    channels.clear();
}

void ChannelServer::report(const ChannelReport& req) {
	// switch (req.type) {
	// case ChannelReport::JOIN:
	// 	if (!get_channel(req.dto.rejoin->channel_id)->ping_pool()) {
	// 		iERROR("Channel %u is full.", req.dto.rejoin->channel_id);
	// 		throw runtime_errorf(POOL_FULL);
	// 	}
	// 	break;
	// }
    reports.push(req);
}


#pragma region PROTECTED_FUNC
void ChannelServer::resolve_deletion() {
	if (!con_tracker) return;
    for (const fd_t fd : next_deletion) {
		try {
        	con_tracker->delete_client(fd);
		} catch (...) {
			continue;
		}
		UserManager::remove_user_name(fd);
        close(fd);
		comm->clear_buffer(fd);
        LOG("Normally Disconnected: fd %d", fd);
    }
}

void ChannelServer::on_accept(const fd_t client) {
    try {
        con_tracker->add_client(client);
		UserManager::set_user_name(client, "user_" + std::to_string(client)); // temporary username assignment

		last_act[client] = std::chrono::steady_clock::now();
	} catch (const std::exception& e) {
		if (dynamic_cast<const coded_runtime_error*>(&e) != nullptr) {
			const coded_runtime_error& cre = static_cast<const coded_runtime_error&>(e);
			if (cre.code == POOL_FULL) {
				comm->send_frame(client, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
        iERROR("%s", e.what());
        next_deletion.insert(client);
		return;
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
				UserManager::set_user_name(from, std::string(user_name)); // user_%d -> real user_name

				Channel* target_ch = find_or_create_channel(channel_id);

				// target_ch is locked here
				target_ch->join_and_logging(from, timestamp, false);

				con_tracker->delete_client(from);
				last_act.erase(from);

				target_ch->start_pooling();
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
				msec64 timestamp = req.dto.join->timestamp;
				Channel* ch_from = get_channel(req.dto.join->ch_from);
				Channel* ch_to = get_channel(req.dto.join->ch_to);
				ch_to->wait_stop_pooling();
				
				if (!ch_to->ping_pool()) {
					iERROR("Channel %u is full.", req.dto.join->ch_to);
					comm->send_frame(req.from, std::string(R"({"type":"error","message":"The channel is full."})"));
					ch_to->start_pooling();
					continue;
				}

				ch_to->join_and_logging(req.from, timestamp, true);
				ch_from->leave_and_logging(req.from, timestamp);

				ch_to->start_pooling();
                delete req.dto.join; // Consumer takes responsibility for deletion
			}
			break;
		}
    }
}
#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_channel(const ch_id_t channel_id) {
	if (channels.find(channel_id) == channels.end()) {
		Channel* channel = new Channel(this, channel_id, ch_max_fd);
		channels[channel_id] = channel;
		LOG(_CG_ "Channel %u created." _EC_, channel_id);
	}
	return channels[channel_id];
}

Channel* ChannelServer::find_or_create_channel(ch_id_t preferred_id) {
    Channel* target_ch = get_channel(preferred_id);
    target_ch->wait_stop_pooling();

    if (target_ch->ping_pool()) {
        return target_ch;
    }

    target_ch->start_pooling(); // Unlock full channel
    target_ch = nullptr;

    // Try to find an available channel
    std::vector<ch_id_t> ids;
    for(auto& [id, _] : channels) ids.push_back(id);
    
    for (ch_id_t id : ids) {
        if (id == preferred_id) continue;
        Channel* candidate = get_channel(id);
        candidate->wait_stop_pooling();
        if (candidate->ping_pool()) {
            return candidate;
        }
        candidate->start_pooling();
    }

    // Create new channel
    ch_id_t new_id = 1;
    while (channels.find(new_id) != channels.end()) new_id++;
    target_ch = get_channel(new_id);
    target_ch->wait_stop_pooling();
    
    return target_ch;
}

void ChannelServer::check_lobby() {
	auto now = std::chrono::steady_clock::now();
	std::unordered_map<fd_t, std::chrono::steady_clock::time_point> next;
	for (const auto& [fd, t] : last_act) {
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count();
		if (elapsed < 5000) {
			next[fd] = t;
		} else {
			LOG("Lobby timeout: fd %d", fd);
			next_deletion.insert(fd);
		}
	}
	last_act = std::move(next);
}

void ChannelServer::check_channels() {
	msec64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	for (auto it = channels.begin(); it != channels.end(); ) {
		Channel* ch = it->second;
		if (ch->is_stopped()) {
			msec64 empty_time = ch->get_empty_since();
			if (empty_time > 0 && (now - empty_time) > 300000) { // 5 minutes
				LOG(_CG_ "Channel %u destroyed due to inactivity." _EC_, it->first);
				delete ch;
				it = channels.erase(it);
				continue;
			}
		}
		++it;
	}
}
#pragma endregion