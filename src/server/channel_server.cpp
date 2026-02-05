#include "channel_server.h"
#include "../libs/util.h"
#include "user_manager.h"

ChannelServer::ChannelServer(NetworkService<User>* service, const int max, const int ch_max, const msec to): TypedJsonFrameServer(service, max, to), ch_max_conn(ch_max) {}

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

bool ChannelServer::init() {
	if (TypedJsonFrameServer::init()) {
		// Periodically process switch requests from channels
		task_runner.pushb(TS_PRE, [this]() {
			consume_report();
		});
		task_runner.pushf(TS_LOGIC, AsThrottle([this]() {
			check_lobby();
			check_channels();
			for (auto& [_, channel] : channels) {
				channel->process();
			}
		}, 1000));
		return true;
	}
	return false;	
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
    for (auto* session : next_deletion) {
		User* user = session->user;
		if (user->name) free(user->name);
		user->name = nullptr;
        service->close_async(session, "Server closed.");
        LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

void ChannelServer::on_accept(typename NetworkService<User>::Session& ses) {
    try {
		if (cur_conn >= max_conn)
			throw runtime_errorf(POOL_FULL);
		
		cur_conn++;

		last_act[&ses] = now_ms();
	} catch (const std::exception& e) {
		if (const auto* cre = try_get_coded_error(e)) {
			switch (cre->code)
			{
			case POOL_FULL:
				service->send_async(&ses, std::string(R"({"type":"error","message":"Server is full."})"));
				break;
			default:
				break;
			}
		}
        iERROR("%s", e.what());
        // next_deletion.insert(&ses);
		return;
    }
}

void ChannelServer::on_req(const typename NetworkService<User>::Session& ses, const char* target, Json& root) {
	User* from = ses.user;
    switch (hash(target))
    {
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
            ch_id_t channel_id;
			msec64 timestamp = now_ms();
			const char* user_name;
			__UNPACK_JSON(root, "{s:I,s:s}", "channel_id", &channel_id, "user_name", &user_name) {
				if (from->name) free(from->name);
				from->name = strdup(user_name);

				Channel* target_ch = find_or_create_channel(channel_id);

				// target_ch is locked here
				target_ch->join_and_logging(const_cast<typename NetworkService<User>::Session*>(&ses), timestamp, false);

				// con_tracker->delete_client(from);
				last_act.erase(const_cast<typename NetworkService<User>::Session*>(&ses));

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
					service->send_async(req.from, std::string(R"({"type":"error","message":"The channel is full."})"));
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
		Channel* channel = new Channel(service, this, channel_id, ch_max_conn);
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
	auto now = now_ms();
	std::unordered_map<typename NetworkService<User>::Session*, msec64> next;
	for (const auto& [session, t] : last_act) {
		auto elapsed = now - t;
		if (elapsed < 5000) {
			next[session] = t;
		} else {
			LOG("Lobby timeout: user %p", session->user);
			// next_deletion.insert(session);
			service->close_async(session, std::string("Lobby timeout."));
		}
	}
	last_act = std::move(next);
}

void ChannelServer::check_channels() {
	msec64 now = now_ms();
	for (auto it = channels.begin(); it != channels.end(); ) {
		Channel* ch = it->second;
		if (ch->get_empty_since() > 0) {
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