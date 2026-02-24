#ifndef __MSG_PARSER_H__
#define __MSG_PARSER_H__

#include <string>
#include <memory>

typedef struct _Req {
	virtual ~_Req() = default;
}Request; // Specialized server class needs to use this struct as dto type for request.

class IMsgParser {
public:
    virtual ~IMsgParser() = default;
    virtual std::unique_ptr<Request> process(const std::string& frame) = 0; // return type as decltype(auto)?
};

#endif