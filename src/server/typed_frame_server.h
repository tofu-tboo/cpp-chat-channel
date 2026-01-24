#ifndef __TYPED_FRAME_SERVER_H__
#define __TYPED_FRAME_SERVER_H__

#include "server_base.h"
#include "../libs/json.h"

class TypedFrameServer : public ServerBase {
public:
    TypedFrameServer(const int max_fd = 256, const msec to = 0);
    virtual ~TypedFrameServer() = default;

protected:
    virtual void on_frame(const fd_t from, const std::string& frame) override;
    virtual void on_req(const fd_t from, const char* target, Json& root) = 0;
};

#endif