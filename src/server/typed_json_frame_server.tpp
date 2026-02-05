#include "typed_json_frame_server.h"

template <typename U>
void TypedJsonFrameServer<U>::on_json(const typename NetworkService<U>::Session& ses, Json& root) {
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        on_req(ses, type, root);
    } __UNPACK_FAIL {
        throw runtime_errorf("Malformed JSON message, missing type.");
    }
}