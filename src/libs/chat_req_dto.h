#ifndef __CHAT_REQ_DTO_H__
#define __CHAT_REQ_DTO_H__

#include "types.h"
#include "json.h"
#include "json_translator.h"

struct ChatReqDto: public JsonRequest {
	std::string type;
	std::string text;
	msec64 timestamp;
	std::string user_name;
	ch_id_t channel_id;

	ChatReqDto(Json* root) {
		const char* type = nullptr;
		const char* text = nullptr;
		const char* user_name = nullptr;
		ch_id_t channel_id = 0;

		__UNPACK_JSON(*root, "{s:s,s?s,s?s,s?i}", "type", &type, "text", &text, "user_name", &user_name, "channel_id", &channel_id) {
			this->type = type;
			this->text = text ? text : ""; // nullptr to std::string is undefined behavior
			this->timestamp = now_ms();
			this->user_name = user_name ? user_name : "";
			this->channel_id = channel_id;
		} __UNPACK_FAIL {
			throw runtime_errorf("Malformed JSON message.");
		}
	}
	ChatReqDto(JsonRequest* req) : ChatReqDto(&req->root) {}

	virtual Request* to_dto() override { return this; }
};

#endif