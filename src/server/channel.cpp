#include "channel.h"

Channel::Channel(): ServerBase(256, 100) {
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
    // temporal
    ServerBase::on_switch(from, target, root, payload);
    // TODO: post join msg to main server
}
#pragma endregion
