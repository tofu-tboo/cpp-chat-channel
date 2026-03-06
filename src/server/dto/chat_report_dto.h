#ifndef __CHAT_REPORT_DTO_H__
#define __CHAT_REPORT_DTO_H__

#include "../../libs/network_service.h"
#include "../../libs/msg_translator.h"
#include "../../libs/exception.h"
#include "../../libs/times.h"
#include "../channel_server_types.h"
#include "chat_req_dto.h"

template <typename U>
struct ChatReportDto: public Request {
	protected:
		USING_TYPENAME(Session, NetworkService<U>);
	public:
		std::string type;
		ch_id_t channel_id1;
		ch_id_t channel_id2;
		Session* ses;
		
		ChatReportDto(Session* ses, const std::string& type, ch_id_t channel_id1, ch_id_t channel_id2): type(type), channel_id1(channel_id1), channel_id2(channel_id2), ses(ses) {}
		// ChatReportDto(Session* ses, const ChatReqDto& dto): type(dto.type), channel_id(dto.channel_id), ses(ses) {}

		virtual Request* to_dto() override { return this; }
};

#endif