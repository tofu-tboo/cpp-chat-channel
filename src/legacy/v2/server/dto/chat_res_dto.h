#ifndef __CHAT_RES_DTO_H__
#define __CHAT_RES_DTO_H__

#include "../../libs/json.h"
#include "../../libs/msg_translator.h"
#include "../channel_server_types.h"


struct ChatResDto : public Response {
	std::string type;
	std::string event;
	std::string user_name;
	ch_id_t channel_id;

	virtual std::string to_frame() const override {
		Json res(json_pack("{s:s,s:s,s:s,s:i}",
			"type", type.c_str(),
			"event", event.c_str(),
			"user_name", user_name.c_str(),
			"channel_id", channel_id
		));
		CharDump dumped(json_dumps(res.get(), JSON_COMPACT));
		return dumped ? std::string(dumped.get()) : "";
	}
};

struct ChatResDtoArray: public Response {
	std::vector<ChatResDto> entries;

	virtual std::string to_frame() const override {
		Json arr(json_array());
		for (const auto& msg : entries) {
			json_t* res = json_pack("{s:s,s:s,s:s,s:i}",
				"type", msg.type.c_str(),
				"event", msg.event.c_str(),
				"user_name", msg.user_name.c_str(),
				"channel_id", msg.channel_id
			);
			json_array_append_new(arr.get(), res);
		}
		CharDump dumped(json_dumps(arr.get(), JSON_COMPACT));
		return dumped ? std::string(dumped.get()) : "";
	}
};

#endif