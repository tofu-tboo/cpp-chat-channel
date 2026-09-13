#include "channel_server.h"
#include "../libs/json_translator.h"
#include "dto/chat_res_dto.h"
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
			scan_channels_to_freed();
		});
		cron_worker.schedule_every("channel proc", 50, [this]() {
			std::shared_lock lock(chs_mtx);
			for (auto& [_, channel] : channels) channel->proc();
		});
		return true;
	}
	return false;	
}

#pragma region PROTECTED_FUNC
void ChannelServer::switch_channel(Session& ses, const ch_id_t from, const ch_id_t to) {
	std::lock_guard lock(chs_mtx);
	msec64 timestamp = now_ms();
	Channel* ch_from = get_or_create_ch_unsafe(from);
	Channel* ch_to = get_or_create_ch_unsafe(to);
	
	if (ch_to->is_full()) {
		elog(_L_CYAN "Channel %u" _L_DEFAULT " is full.", to);
		service->send(&ses, std::string(R"({"type":"error","message":"The channel is full."})"));
		return;
	}
	
	ch_to->join_and_logging(ses, true);
	ch_from->leave_and_logging(ses);
}

void ChannelServer::free_user(Session& ses) {
	User* user = ses.user;
	if (user->name) free(user->name);
	user->name = nullptr;

	std::lock_guard erase(la_mtx);
	last_act.erase(&ses);
}

void ChannelServer::on_accept(Session& ses) {
	super::on_accept(ses);
	std::lock_guard lock(la_mtx);
	last_act[&ses] = now_ms();
}

void ChannelServer::handle_request(Session& ses, std::shared_ptr<Request> req) {
	JsonRequest* json_req = dynamic_cast<JsonRequest*>(req.get());
	if (!json_req) return;

	ChatReqDto dto(json_req);

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
					std::lock_guard uncheck_activity(la_mtx);
					std::shared_lock check_close_rsv(nc_mtx);
					last_act.erase(&ses);
					if (nxt_close.find(&ses) != nxt_close.end())
						break; // Session considered as inactive
				}

				// 3. Join ch
				// Channel* target_ch = find_pref_or_rand_ch(-1);
				// target_ch->join_and_logging(const_cast<Session&>(ses), false);
				ChatReportDto<User>* join_rep = new ChatReportDto<User>(&ses, "join", 0, dto.channel_id);
				report(join_rep);
				
				cur_conn--;

				
				break;
			}
		default:
			break;
	}
	
}

void ChannelServer::consume_report(std::shared_ptr<Request> req) {
	ChatReportDto<User>* rep = dynamic_cast<ChatReportDto<User>*>(req.get());
	if (!rep) return;

	switch_hash (rep->type.c_str()) {
		case_hash ("join"):
		case_hash ("Join"):
		case_hash ("JOIN"):
		{
			Channel* target_ch = find_pref_or_rand_ch(-1);
			target_ch->join_and_logging(*rep->ses, false);
			break;
		}
		case_hash ("switch"):
		case_hash ("Switch"):
		case_hash ("SWITCH"):
		{
			switch_channel(*rep->ses, rep->channel_id1, rep->channel_id2);
			break;
		}
		default:
			break;
	}
}

#pragma endregion

#pragma region PRIVATE_FUNC
Channel* ChannelServer::get_or_create_ch(const ch_id_t channel_id) {
	std::lock_guard lock(chs_mtx);
	return get_or_create_ch_unsafe(channel_id);
}

Channel* ChannelServer::get_or_create_ch_unsafe(const ch_id_t channel_id) {
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
	std::lock_guard lock(chs_mtx);
	if (preferred_id != -1) {
		Channel* target_ch = get_or_create_ch_unsafe(preferred_id);

		if (!target_ch->is_full()) {
			return target_ch;
		}
	}
    
	ch_id_t expected = 0;
	for (auto [id, _]: channels) {
		if (id != expected) {
			Channel* candidate = get_or_create_ch_unsafe(id);
			if (!candidate->is_full()) {
				return candidate;
			}
		}
		expected++;
	}

	return get_or_create_ch_unsafe(expected);
}

void ChannelServer::check_lobby() {
	auto now = now_ms();
	std::unordered_map<Session*, msec64> next;

	std::lock_guard lock(la_mtx);
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
	std::lock_guard lock(chs_mtx);
	for (std::map<ch_id_t, Channel*>::iterator it = channels.begin(); it != channels.end();) {
		Channel* ch = it->second;
		if (ch->want_freed()) {
			log(_L_CYAN "Channel %u" _L_DEFAULT " destroyed due to inactivity.", it->first);
			delete ch;
			it = channels.erase(it);
		} else {
			it++;
		}
	}
}
#pragma endregion