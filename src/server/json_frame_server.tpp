#include "json_frame_server.h"

template <typename U>
void JsonFrameServer<U>::on_frame(const U& user, const std::string& frame) {
    json_error_t err;
    Json root(json_loads(frame.c_str(), 0, &err));
    if (root.get() == nullptr) {
        iERROR("Failed to parse JSON: %s", err.text);
        return;
    }
    on_json(user, root);
}