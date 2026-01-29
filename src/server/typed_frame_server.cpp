#include "typed_frame_server.h"

TypedFrameServer::TypedFrameServer(const int max_fd, const msec to) : ServerBase(max_fd, to) {
	#ifdef DEBUG
	task_runner.pushf(TS_LOGIC, AsThrottle([this]() {
		auto clients = con_tracker->get_clients();
		for (const fd_t fd : clients) {
			char buf[4096];
			recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
			LOG("[PEEK] %s from fd %d", buf, fd);
		}
	}, 2000));
	#endif
}

void TypedFrameServer::on_frame(const fd_t from, const std::string& frame) {
    json_error_t err;
    Json root(json_loads(frame.c_str(), 0, &err));
    if (root.get() == nullptr) {
        iERROR("Failed to parse JSON: %s", err.text);
        return;
    }
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        on_req(from, type, root);
    } __UNPACK_FAIL {
        iERROR("Malformed JSON message, missing type.");
    }
}