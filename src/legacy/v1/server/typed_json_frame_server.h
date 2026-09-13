#ifndef __TYPED_JSON_FRAME_SERVER_H__
#define __TYPED_JSON_FRAME_SERVER_H__

#include "json_frame_server.h"

template <typename U>
class TypedJsonFrameServer : public JsonFrameServer<U> {
public:
	using JsonFrameServer<U>::JsonFrameServer;
protected:
    virtual void on_json(const typename NetworkService<U>::Session& ses, Json& root) override;

    virtual void on_req(const typename NetworkService<U>::Session& ses, const char* type, Json& root) = 0;
};

#include "typed_json_frame_server.tpp"

#endif