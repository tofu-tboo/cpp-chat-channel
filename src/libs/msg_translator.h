#ifndef __MSG_TRANSLATOR_H__
#define __MSG_TRANSLATOR_H__

#include <string>
#include <memory>

struct Request {
	virtual Request* to_dto() = 0;
	virtual ~Request() = default;
};

struct Response {
	virtual std::string to_frame() const = 0;
	virtual ~Response() = default;
};

class IMsgTranslator {
public:
    virtual ~IMsgTranslator() = default;
    virtual std::shared_ptr<Request> decode(const std::string& frame) = 0; // return type as decltype(auto)?
	virtual std::string encode(const Response& res) = 0;
};

#endif