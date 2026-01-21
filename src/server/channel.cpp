#include "channel.h"
#include "channel_server.h"

Channel::Channel(ChannelServer* srv): ServerBase(256, 100), server(srv) {
    stop_flag.store(false);
    worker = std::thread(&Channel::proc, this);
}
Channel::~Channel() {
    stop_flag.store(true);
    if (worker.joinable()) {
        worker.join();
    }
}

void Channel::proc() {
    try {
        while (!stop_flag.load(std::memory_order_relaxed)) {
            frame();
        }
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

void Channel::leave(const fd_t fd) {
    try {
        con_tracker->delete_client(fd);
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}
void Channel::join(const fd_t fd) {
    try {
        con_tracker->add_client(fd);
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

#pragma region PROTECTED_FUNC
void Channel::on_accept() {}
void Channel::on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) {
    switch (hash(target)) {
    case hash("message"):
		ServerBase::on_switch(from, target, root, payload);
        break;
	// TODO: leave === disconnection
    case hash("join"):
        {
			leave(from);
			server->on_switch(from, "join", root, payload);
            // ch_id_t channel_id;
            // __UNPACK_JSON(root, "{s:I}", "channel_id", &channel_id) {
            //     leave(from);
            //     server->report({ChannelServer::ChannelReq::Type::SWITCH, from, channel_id, false}); // Request switch channel
            // } __UNPACK_FAIL {
            //     iERROR("Malformed JSON message, missing channel_id.");
            // }
        }
        break;
    default:
        break;
    }
}
#pragma endregion
