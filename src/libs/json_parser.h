#ifndef __JSON_MESSAGE_PROCESSOR_H__
#define __JSON_MESSAGE_PROCESSOR_H__

#include "msg_parser.h"
#include "json.h"


typedef struct _JsonReq : public Request {
	Json root;
	_JsonReq(Json r) : root(std::move(r)) {}
}JsonRequest;

class JsonParser : public IMsgParser<JsonRequest> {
	public:
		 virtual std::unique_ptr<JsonRequest> process(const std::string& frame) override;
};

#endif