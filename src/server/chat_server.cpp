#include "chat_server.h"
#include "../libs/json_translator.h"
#include "dto/chat_res_dto.h"
#include "dto/chat_req_dto.h"
#include "../libs/hash.h"

ChatServer::ChatServer(std::shared_ptr<NetworkService<User>> service, const int max_fd)
	: super(std::move(service), std::make_unique<JsonTranslator>(), max_fd), Loggable("ChatServer", _L_BLUE, this) {}

ChatServer::~ChatServer() {
	cur_msgs.clear();
}

bool ChatServer::init() {
	if (super::init()) {
		cron_worker.schedule_every("broadcast", 50, [this]() {
			resolve_timestamps();
			resolve_broadcast();
			std::lock_guard lock(cm_mtx);
			cur_msgs.clear();
		});
		return true;
	}
	return false;
}

#pragma region PROTECTED_FUNC


void ChatServer::resolve_timestamps() {
	std::unique_lock lock(mq_mtx);
	std::lock_guard lock2(cm_mtx);
    auto local_q = mq.pop_all();
	lock.unlock();

	while (!local_q.empty()) {
        auto item = std::move(local_q.front());
        local_q.pop_front();

		cur_msgs.emplace(item.second.timestamp, item);
	}
}

void ChatServer::resolve_broadcast() {
    ChatResDtoArray dtos;
	{
		std::shared_lock gather_msg(cm_mtx);
		for (const auto& [timestamp, req_pair] : cur_msgs) {
			const auto& req = req_pair.second;
			switch (req.type)
			{
				case USER:
				{
					ChatResDto dto;
					dto.type = "user";
					dto.event = req.text;
					dto.user_name = req.user_name;
					dtos.entries.push_back(std::move(dto));
					break;
				}
				case SYSTEM:
				{
					ChatResDto dto;
					dto.type = "system";
					dto.event = req.text;
					dto.user_name = req.user_name;
					dto.channel_id = req.channel_id;
					dtos.entries.push_back(std::move(dto));
					break;
				}
			default:
				break;
			}
			
		}
	}
	
	
	if (dtos.entries.empty()) return;

    std::string frame = dtos.to_frame();
    if (!frame.empty()) {
		service->broadcast(frame);
    }
}

void ChatServer::on_accept(Session& ses) {
	super::on_accept(ses);

	char user_name[20];
	snprintf(user_name, sizeof(user_name), "guest_%013llu", (unsigned long long)now_ms());

	ses.user->name = strdup(user_name);
}

void ChatServer::handle_request(Session& ses, std::shared_ptr<Request> req) {
	JsonRequest* json_req = dynamic_cast<JsonRequest*>(req.get());
	if (!json_req) return;

	ChatReqDto dto(json_req);

	switch_hash (dto.type.c_str()) {
		case_hash ("message"):
		case_hash ("Message"):
		case_hash ("MESSAGE"):
		{
			const User* from = ses.user;
			std::string user_name;
			if (from->name) user_name = from->name;
			else user_name = "unknown";

			MessageReqDto msg_req = { .type = USER, .text = dto.text, .timestamp = dto.timestamp, .user_name = user_name };

			std::lock_guard lock(mq_mtx);
			mq.push({const_cast<Session*>(&ses), msg_req});
			break;
		}
	default:
		break;
	}
}

#pragma endregion