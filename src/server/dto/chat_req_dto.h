#ifndef __CHAT_REQ_DTO_H__
#define __CHAT_REQ_DTO_H__

#include "../../libs/json.h"
#include "../../libs/json_translator.h"
#include "../../libs/exception.h"
#include "../../libs/times.h"
#include "../channel_server_types.h"

struct ChatReqDto: public JsonRequest {
	std::string type;
	std::string text;
	msec64 timestamp;
	std::string user_name;
	ch_id_t channel_id;

	ChatReqDto(Json p): JsonRequest(std::move(p)) {
		const char* type = nullptr;
		const char* text = nullptr;
		const char* user_name = nullptr;
		ch_id_t channel_id = 0;

		unpack_json(root, "{s:s,s?s,s?s,s?i}", "type", &type, "text", &text, "user_name", &user_name, "channel_id", &channel_id) {
			this->type = type;
			this->text = text ? text : ""; // nullptr to std::string is undefined behavior
			this->timestamp = now_ms();
			this->user_name = user_name ? user_name : "";
			this->channel_id = channel_id;
		} unpack_fail {
			throw runtime_errorf("Malformed JSON message.");
		}
	}
	ChatReqDto(JsonRequest* req) : JsonRequest(nullptr) {
		if (req && req->root) {
			json_incref(req->root.get());
			this->root.reset(req->root.get());
			*this = ChatReqDto(std::move(this->root));
		}
	}

	virtual Request* to_dto() override { return this; }
};

#endif