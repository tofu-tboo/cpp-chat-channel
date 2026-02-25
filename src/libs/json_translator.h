#ifndef __JSON_TRANSLATOR_H__
#define __JSON_TRANSLATOR_H__

#include "msg_translator.h"
#include "json.h"


struct JsonRequest : public Request {
	Json root;
	virtual Request* to_dto() override { return this; }
	JsonRequest() = default;
	JsonRequest(Json r) : root(std::move(r)) {}
};

struct JsonResponse : public Response {
	Json payload;
	mutable std::string cached_frame;
	mutable bool is_cached = false;

	virtual std::string to_frame() const override {
		if (!is_cached) {
			CharDump dumped(json_dumps(payload.get(), JSON_COMPACT));
			cached_frame = dumped ? std::string(dumped.get()) : "";
			is_cached = true;
		}
		return cached_frame;
	}

	JsonResponse() = default;
	JsonResponse(Json p) : payload(std::move(p)) {}
};


class JsonTranslator : public IMsgTranslator {
	public:
		 virtual std::unique_ptr<Request> decode(const std::string& frame) override;
		 virtual std::string encode(const Response& res) override;
};

#endif