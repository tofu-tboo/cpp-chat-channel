#include "json_frame_server.h"

template <typename U>
void JsonFrameServer<U>::on_frame(const typename NetworkService<U>::Session& ses, const std::string& frame) {
    json_error_t err;
    Json root(json_loads(frame.c_str(), 0, &err));
    if (root.get() == nullptr) {
        throw runtime_errorf("Failed to parse JSON: %s", err.text);
    }
    on_json(ses, root);
}