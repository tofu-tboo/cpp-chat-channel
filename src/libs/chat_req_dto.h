#ifndef __CHAT_REQ_DTO_H__
#define __CHAT_REQ_DTO_H__

#include "types.h"
#include "json.h"
#include "json_parser.h"

typedef struct _TypeJson: public Request {
	std::string type;
	std::string text;
	msec64 timestamp;
	std::string user_name;
	ch_id_t channel_id;

	_TypeJson(Json* root) {
		const char* type;
		const char* text;
		const char* user_name;
		ch_id_t channel_id;

		__UNPACK_JSON(*root, "{s:s,s?s,s?s,s?i}", "type", &type, "text", &text, "user_name", &user_name, "channel_id", &channel_id) {
			this->type = type;
			this->text = text;
			this->timestamp = now_ms();
			this->user_name = user_name;
			this->channel_id = channel_id;
		} __UNPACK_FAIL {
			throw runtime_errorf("Malformed JSON message.");
		}
	}
	_TypeJson(JsonRequest* req) : _TypeJson(&req->root) {}
} ChatReqDto;

#endif