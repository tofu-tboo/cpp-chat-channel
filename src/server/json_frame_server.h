#ifndef __JSON_FRAME_SERVER_H__
#define __JSON_FRAME_SERVER_H__

#include "server_base.h"
#include "../libs/json.h"

template <typename U>
class JsonFrameServer: public ServerBase<U> {
public:
    using ServerBase<U>::ServerBase;
protected:
    virtual void on_frame(const U& user, const std::string& frame) override;
    virtual void on_json(const U& user, Json& root) = 0;
};

#include "json_frame_server.tpp"

#endif