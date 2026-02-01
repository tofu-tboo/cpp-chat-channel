#ifndef __TYPED_FRAME_SERVER_H__
#define __TYPED_FRAME_SERVER_H__

#include "json_frame_server.h"

template <typename U>
class TypedFrameServer : public JsonFrameServer<U> {
public:
    TypedFrameServer(NetworkService<U>* service, const int max_fd = 256, const msec to = 0);
    virtual ~TypedFrameServer() = default;

protected:
    virtual void on_json(const U& user, Json& root) override;

    virtual void on_req(const char* target, Json& root) = 0;
};

#endif