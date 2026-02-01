#include "server_base.h"

template <typename U>
fd_t ServerBase<U>::fd = -1;

template <typename U>
ServerBase<U>::ServerBase(NetworkService<U>* di_service, const int max_fd, const msec to): con_tracker(nullptr), service(di_service), timeout(to), is_running(true) {
    try {
        branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

        // con_tracker = new ConnectionTracker(fd, max_fd);
        // if (!con_tracker)
        //     throw std::runtime_error("Failed to allocate Connection Tracker.");
        // con_tracker->init();

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
    if (con_tracker)
        delete con_tracker;
	
    if (fd != FD_ERR) { // close listening socket
        close(fd);
        fd = FD_ERR;
    }

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
	if (!con_tracker) return;
    for (const fd_t fd : next_deletion) {
		try {
        	con_tracker->delete_client(fd);
		} catch (...) {
			continue;
		}
        close(fd);

		LOG("Normally Disconnected: fd %d", fd);
    }
}

template <typename U>
void ServerBase<U>::on_accept(const U& user, const Connection& connection) {
    fd_t client = lws_get_socket_fd(connection.wsi);
	try {
        if (con_tracker) con_tracker->add_client(client);
		LOG("Accepted new connection: fd %d", client);

	} catch (const std::exception& e) {
		if (const auto* cre = try_get_coded_error(e)) {
			if (cre->code == POOL_FULL) {
				service->send(connection.wsi, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
        iERROR("%s", e.what());
        next_deletion.insert(client);
		return;
    }

}

// void ServerBase<U>::on_disconnect(const fd_t fd) {
// 	next_deletion.insert(fd);
// }

template <typename U>
void ServerBase<U>::on_recv(const U& user, const Connection& connection, const RecvStream& stream) {
    fd_t from = lws_get_socket_fd(connection.wsi);
	try {
        on_frame(user, std::string(reinterpret_cast<const char*>(stream.data), stream.len));
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
        next_deletion.insert(from);
    }
}

template <typename U>
void ServerBase<U>::on_send(const U& user, const Connection& connection) {}

// User ServerBase<U>::translate(LwsCallbackParam&& param) {
	
// }
#pragma endregion
