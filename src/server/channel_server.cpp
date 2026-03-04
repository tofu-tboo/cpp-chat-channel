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
		cron_worker.schedule_every("check lobby", 100, [this]() {
			check_lobby();
		});
		cron_worker.schedule_every("cleanup", 1000, [this]() {
			scan_channels_to_freed(); // TODO?: make timing dependent on cur user cnt & channel cnt
		}); // 추후 채널 생성의 비용이 커질 수 있으므로 cron으로 효율화
		cron_worker.schedule_every("channel proc", 50, [this]() {
			std::shared_lock lock(chs_mtx);
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
	Channel* ch_from = get_or_create_ch(from);
	Channel* ch_to = get_or_create_ch(to);
	
	if (!ch_to->is_full()) {
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

	std::unique_lock erase(la_mtx);
	last_act.erase(&ses);
}

void ChannelServer::on_accept(Session& ses) {
	super::on_accept(ses);
	std::unique_lock lock(la_mtx);
	last_act[&ses] = now_ms();
}

void ChannelServer::handle_request(Session& ses, std::unique_ptr<Request> req) {
	JsonRequest* json_req = dynamic_cast<JsonRequest*>(req.get());
	if (!json_req) return;

	ChatReqDto dto(&json_req->root);

	switch_hash (dto.type.c_str()) {
		case_hash ("join"):
		case_hash ("Join"):
		case_hash ("JOIN"):
			{
				// 1. Set name
				User* from = ses.user;
				if (from->name) free(from->name);
				from->name = strdup(dto.user_name.c_str());

				// 2. Uncheck session activity
				{
					// ** DEPENDENT ON check_lobby() **
					std::unique_lock uncheck_activity(la_mtx);
					std::shared_lock check_close_rsv(nc_mtx);
					last_act.erase(&ses);
					if (nxt_close.find(&ses) != nxt_close.end())
						break; // Session considered as inactive
				}

				// 3. Join ch
				Channel* target_ch = find_pref_or_rand_ch(-1);
				target_ch->join_and_logging(const_cast<Session&>(ses), false);
				
				cur_conn--;
				break;
			}
		default:
			break;
	}
	
}

#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_or_create_ch(const ch_id_t channel_id) {
	std::unique_lock lock(chs_mtx);
	
	std::map<ch_id_t, Channel*>::iterator it = channels.find(channel_id);
	if (it != channels.end()) {
		Channel* ch = it->second;
		if (ch->want_freed())
			ch->use();
		return ch;
	}
	// Split lock as 2 (shared & unique) => there is likely to make same channel simutaneously. => Dangling ptr
	channels[channel_id] = channel_factory->create(this, channel_id);
	channels[channel_id]->init();
	log(_L_CYAN "Channel %u" _L_DEFAULT " created.", channel_id);
	return channels[channel_id];
}

Channel* ChannelServer::find_pref_or_rand_ch(ch_id_t preferred_id) {
	if (preferred_id != -1) {
		Channel* target_ch = get_or_create_ch(preferred_id);

		if (!target_ch->is_full()) {
			return target_ch;
		}
	}
    
	std::unique_lock lock(chs_mtx);
	ch_id_t expected = 0;
	for (auto [id, _]: channels) {
		if (id != expected) {
			Channel* candidate = get_or_create_ch(id);
			if (!candidate->is_full()) {
				return candidate;
			}
		}
		expected++;
	}
}

void ChannelServer::check_lobby() {
	auto now = now_ms();
	std::unordered_map<Session*, msec64> next;

	std::unique_lock lock(la_mtx);
	for (const auto& [session, t] : last_act) {
		auto elapsed = now - t;
		if (elapsed < 5000) {
			next[session] = t;
		} else {
			log(_L_YELLOW "Lobby timeout: " _L_CYAN "user %p", session->user);
			rsv_close(session, R"({"type":"error","message":"Lobby timeout."})");
			// service->close(session, R"({"type":"error","message":"Lobby timeout."})");
			// service->send(session, std::string("Lobby timeout."));
		}
	}

	last_act = std::move(next);
}

void ChannelServer::scan_channels_to_freed() {
	std::unique_lock lock(chs_mtx);
	for (std::map<ch_id_t, Channel*>::iterator it = channels.begin(); it != channels.end();) {
		Channel* ch = it->second;
		if (channel->want_freed()) {
			log(_L_CYAN "Channel %u" _L_DEFAULT " destroyed due to inactivity.", it->first);
			delete channel;
			channels.erase(id);
			continue;
		}
		it++
	}
}
#pragma endregion