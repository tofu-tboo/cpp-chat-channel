#include "channel_server.h"
#include "../libs/json_translator.h"
#include "../libs/chat_res_dto.h"
#include "../libs/times.h"
#include "../libs/hash.h"

ChannelServer::ChannelServer(std::shared_ptr<NetworkService<User>> service, const int max, std::unique_ptr<ChannelFactory> factory)
	: super(std::move(service), std::make_unique<JsonTranslator>(), max), channel_factory(std::move(factory)), Loggable("ChannelServer", _L_BLUE, this) {}

ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }

    channels.clear();
}

bool ChannelServer::init() {
	if (super::init()) {
		// Periodically process switch requests from channels
		task_runner.pushf(TS_LOGIC, AsThrottle([this]() {
			check_lobby();
			check_channels();
		}, 1000));
		task_runner.pushb(TS_LOGIC, [this]() {
			for (auto& [_, channel] : channels) {
				channel->proc();
			}
		});
		return true;
	}
	return false;	
}

void ChannelServer::switch_channel(Session& ses, const ch_id_t from, const ch_id_t to) {
	msec64 timestamp = now_ms();
	Channel* ch_from = get_channel(from);
	Channel* ch_to = get_channel(to);
	
	if (!ch_to->ping_pool()) {
		elog(_L_CYAN "Channel %u" _L_DEFAULT " is full.", to);
		service->send(&ses, std::string(R"({"type":"error","message":"The channel is full."})"));
		return;
	}	
	ch_to->join_and_logging(ses, true);
	ch_from->leave_and_logging(ses);
}

#pragma region PROTECTED_FUNC
void ChannelServer::free_user(Session& ses) {
	User* user = ses.user;
	if (user->name) free(user->name);
	user->name = nullptr;

	std::unique_lock<std::shared_mutex> lock(la_mtx);
	last_act.erase(&ses);
	lock.unlock();
}

void ChannelServer::on_accept(Session& ses) {
	super::on_accept(ses);
	std::unique_lock<std::shared_mutex> lock(la_mtx);
	last_act[&ses] = now_ms();
}

void ChannelServer::handle_request(Session& ses, std::unique_ptr<Request> req) {
	JsonRequest* json_req = dynamic_cast<JsonRequest*>(req.get());
	if (!json_req) return;

	ChatReqDto dto(&json_req->root);

	switch_hash(dto.type.c_str()) {
		case_hash("join"):
		case_hash("Join"):
		case_hash("JOIN"):
			{
				User* from = ses.user;
				if (from->name) free(from->name);
				from->name = strdup(dto.user_name.c_str());

				Channel* target_ch = find_or_create_channel(dto.channel_id);
				target_ch->join_and_logging(const_cast<Session&>(ses), false);

				std::unique_lock<std::shared_mutex> lock(la_mtx);
				last_act.erase(const_cast<Session*>(&ses));
				lock.unlock();
				
				cur_conn--;
				break;
			}
		default:
			break;
	}
	
}

#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_channel(const ch_id_t channel_id) {
	std::shared_lock<std::shared_mutex> lock(chs_mtx);
	auto it = channels.find(channel_id);
	if (it != channels.end()) {
		return it->second;
	}
	lock.unlock();

	std::unique_lock<std::shared_mutex> lock2(chs_mtx);
	channels[channel_id] = channel_factory->create(this, channel_id);
	channels[channel_id]->init();
	log(_L_CYAN "Channel %u" _L_DEFAULT " created.", channel_id);
	return channels[channel_id];
}

Channel* ChannelServer::find_or_create_channel(ch_id_t preferred_id) {
    Channel* target_ch = get_channel(preferred_id);

    if (target_ch->ping_pool()) {
        return target_ch;
    }

    // Try to find an available channel
	std::unique_lock<std::shared_mutex> lock(chs_mtx);
    std::vector<ch_id_t> ids;
    for(auto& [id, _] : channels) ids.push_back(id);

    for (ch_id_t id : ids) {
        if (id == preferred_id) continue;
        Channel* candidate = get_channel(id);
        if (candidate->ping_pool()) {
            return candidate;
        }
    }

    // Create new channel
    ch_id_t new_id = 1;
    while (channels.find(new_id) != channels.end()) new_id++;
	lock.unlock();

    target_ch = get_channel(new_id);
    
    return target_ch;
}

void ChannelServer::check_lobby() {
	auto now = now_ms();
	std::unordered_map<Session*, msec64> next;

	std::unique_lock<std::shared_mutex> lock(la_mtx);
	for (const auto& [session, t] : last_act) {
		auto elapsed = now - t;
		if (elapsed < 5000) {
			next[session] = t;
		} else {
			log(_L_YELLOW "Lobby timeout: " _L_CYAN "user %p", session->user);
			resv_close(session);
			service->send(session, std::string("Lobby timeout."));
		}
	}

	last_act = std::move(next);
}

void ChannelServer::check_channels() {
	msec64 now = now_ms();
	std::unique_lock<std::shared_mutex> lock(chs_mtx);
	for (auto it = channels.begin(); it != channels.end(); ) {
		Channel* ch = it->second;
		if (ch->get_empty_since() > 0) {
			msec64 empty_time = ch->get_empty_since();
			if (empty_time > 0 && (now - empty_time) > 300000) { // 5 minutes
				log(_L_CYAN "Channel %u" _L_DEFAULT " destroyed due to inactivity.", it->first);
				delete ch;
				it = channels.erase(it);
				continue;
			}
		}
		++it;
	}
}
#pragma endregion