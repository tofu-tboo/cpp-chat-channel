#include "typed_json_frame_server.h"

template <typename U>
void TypedJsonFrameServer<U>::on_json(const U& user, Json& root) {
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        on_req(user, type, root);
    } __UNPACK_FAIL {
        iERROR("Malformed JSON message, missing type.");
    }
}