#include "typed_json_frame_server.h"

template <typename U>
void TypedJsonFrameServer<U>::on_json(const typename NetworkService<U>::Session& ses, Json& root) {
    const char* type;
    unpack_json(root, "{s:s}", "type", &type) {
        on_req(ses, type, root);
    } unpack_fail {
        throw runtime_errorf("Malformed JSON message, missing type.");
    }
}