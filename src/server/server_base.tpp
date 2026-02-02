#include "server_base.h"

template <typename U>
ServerBase<U>::ServerBase(NetworkService<U>* di_service, const int max_fd, const msec to): service(di_service), timeout(to), is_running(true) {
    try {
        branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

		if (!di_service)
			throw std::runtime_error("Network Service NullPtr.");
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

template <typename U>
void ServerBase<U>::init() {
	if (service) {
		service->setup(this);

		task_runner.new_session(TS_COUNT);
		// Cleanup Qs
        task_runner.pushb(TS_PRE, [this]() {
            next_deletion.clear();
        });
		// Polling
        task_runner.pushb(TS_POLL, [this]() {
            // con_tracker->polling(timeout);
			service->serve(timeout);
        });
		// Deletion fds
        task_runner.pushb(TS_LOGIC, [this]() {
            resolve_deletion();
        });
	} 
}

template <typename U>
ServerBase<U>::~ServerBase() {
    next_deletion.clear();
}

template <typename U>
void ServerBase<U>::proc() {
    while (is_running) {
        try {
            task_runner.run();
            // frame();
        } catch(const std::exception& e) {
            iERROR("%s", e.what());
        }
    }
}

template <typename U>
void ServerBase<U>::stop() {
    is_running = false;
}

#pragma region PRIVATE_FUNC

#pragma endregion

#pragma region PROTECTED_FUNC

template <typename U>
void ServerBase<U>::resolve_deletion() {
    for (U* user : next_deletion) {
        service->close_async(user, "Server closed connection.");
		LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

template <typename U>
void ServerBase<U>::on_accept(U& user) {
	try {
		LOG("Accepted new connection: user %p", &user);

	} catch (const std::exception& e) {
		if (const auto* cre = try_get_coded_error(e)) {
			if (cre->code == POOL_FULL) {
				service->send_async(&user, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
        iERROR("%s", e.what());
        next_deletion.insert(&user);
		return;
    }

}

// void ServerBase<U>::on_disconnect(const fd_t fd) {
// 	next_deletion.insert(fd);
// }

template <typename U>
void ServerBase<U>::on_recv(U& user, const RecvStream& stream) {
	try {
        on_frame(user, std::string(reinterpret_cast<const char*>(stream.data), stream.len));
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
        next_deletion.insert(&user);
    }
}

template <typename U>
void ServerBase<U>::on_send(U& user) {}

// User ServerBase<U>::translate(LwsCallbackParam&& param) {
	
// }
#pragma endregion
