#ifndef __TYPED_JSON_FRAME_SERVER_H__
#define __TYPED_JSON_FRAME_SERVER_H__

#include "json_frame_server.h"

template <typename U>
class TypedJsonFrameServer : public JsonFrameServer<U> {
protected:
    virtual void on_json(const U& user, Json& root) override;

    virtual void on_req(const U& user, const char* type, Json& root) = 0;
};

#include "typed_json_frame_server.tpp"

#endif