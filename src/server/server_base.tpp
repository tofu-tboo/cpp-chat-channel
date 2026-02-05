#include "server_base.h"
#include <sys/time.h>

template <typename U>
ServerBase<U>::ServerBase(NetworkService<U>* di_service, const int max, const msec to): service(di_service), max_conn(max), cur_conn(0), timeout(to), is_running(true) {
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
bool ServerBase<U>::init() {
	if (service) {
		service->setup(this);

		task_runner.new_session(TS_COUNT);
		// Cleanup Qs
        task_runner.pushb(TS_PRE, [this]() {
            // next_deletion.clear();
        });
		// Polling
        task_runner.pushb(TS_POLL, [this]() {
            // con_tracker->polling(timeout);
			service->serve(timeout); // TODO: api: ignored timeout
        });
		// Deletion fds
        task_runner.pushb(TS_LOGIC, [this]() {
            // resolve_deletion();
        });

		return true;
	} 
	return false;
}

template <typename U>
ServerBase<U>::~ServerBase() {
    // next_deletion.clear();
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
    for (typename NetworkService<U>::Session* user : next_deletion) {
        service->close_async(user, std::string("Server closed connection."));
		LOG("Normally Disconnected: user %p", user);
    }
    next_deletion.clear();
}

template <typename U>
void ServerBase<U>::on_accept(typename NetworkService<U>::Session& ses) {
	try {
		if (cur_conn >= max_conn)
			throw runtime_errorf(POOL_FULL);
		
		cur_conn++;
		LOG("Accepted new connection: user %p", ses.user);
	} catch (const std::exception& e) {
		if (const auto* cre = try_get_coded_error(e)) {
			if (cre->code == POOL_FULL) {
				service->close_async(&ses, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
        iERROR("%s", e.what());
        // next_deletion.insert(&ses);
		return;
    }

}

template <typename U>
void ServerBase<U>::on_close(typename NetworkService<U>::Session& ses) {
	cur_conn--;
	LOG("Closed connection: user %p", ses.user);
}

template <typename U>
void ServerBase<U>::on_recv(typename NetworkService<U>::Session& ses, const RecvStream& stream) {
	try {
        on_frame(ses, std::string(reinterpret_cast<const char*>(stream.data), stream.len));
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
		service->close_async(&ses, std::string(""));
        // next_deletion.insert(&ses);
    }
}

template <typename U>
void ServerBase<U>::on_send(typename NetworkService<U>::Session& ses) {}

// User ServerBase<U>::translate(LwsCallbackParam&& param) {
	
// }
#pragma endregion
