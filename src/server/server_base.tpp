#include "server_base.h"
#include "../libs/msg_translator.h"

template <typename U>
ServerBase<U>::ServerBase(std::shared_ptr<NetworkService<U>> di_service, std::unique_ptr<IMsgTranslator> processor, const int max): service(std::move(di_service)), msg_translator(std::move(processor)), max_conn(max), cur_conn(0), is_running(true) {
    branch_id = now_ms();

	if (!service)
		throw std::runtime_error("Network Service NullPtr.");
	else if (!msg_translator)
		throw std::runtime_error("Message Parser NullPtr.");
}

template <typename U>
bool ServerBase<U>::init() {
	if (service) {
		service->setup(this);

		task_runner.new_session(TS_COUNT);
		// Cleanup Qs
        task_runner.pushb(TS_PRE, [this]() {
			std::unique_lock<std::shared_mutex> lock(nd_mtx);
			nxt_close.clear();
        });
		// Polling
        task_runner.pushb(TS_POLL, [this]() {
			service->serve();
        });
		// Deletion fds
        task_runner.pushb(TS_LOGIC, [this]() {
            resolve_close();
        });

		return true;
	} 
	return false;
}

template <typename U>
ServerBase<U>::~ServerBase() {}

template <typename U>
void ServerBase<U>::proc() {
    while (is_running) {
        try {
            task_runner.run();
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
// template <typename U>
// void ServerBase<U>::cl_session(typename NetworkService<U>::Session* ses) {
// 	service->close_async(ses, std::string("Server closed connection."));
// }
template <typename U>
void ServerBase<U>::resolve_close() {
	std::shared_lock<std::shared_mutex> lock(nd_mtx);
    for (typename NetworkService<U>::Session* ses : nxt_close) {
		service->close_async(ses, std::string("Server closed connection."));
        // cl_session(ses);
    }
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
				service->send_async(&ses, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
		throw e;
    }

}

template <typename U>
void ServerBase<U>::on_close(typename NetworkService<U>::Session& ses) {
	cur_conn--;
	free_user(ses);
	LOG("Closed connection: user %p", ses.user);
}

template <typename U>
void ServerBase<U>::on_recv(typename NetworkService<U>::Session& ses, const RecvStream& stream) {
    if (msg_translator) {
		auto req = msg_translator->decode(std::string(reinterpret_cast<const char*>(stream.data), stream.len));
		handle_request(ses, std::move(req));
	}
}

template <typename U>
void ServerBase<U>::on_send(typename NetworkService<U>::Session& ses) {}

template <typename U>
void ServerBase<U>::on_rate_limit_packet_drop(typename NetworkService<U>::Session& ses) {}


// User ServerBase<U>::translate(LwsCallbackParam&& param) {
	
// }
template <typename U>
void ServerBase<U>::free_user(typename NetworkService<U>::Session& ses) {}

template <typename U>
void ServerBase<U>::resv_close(typename NetworkService<U>::Session* ses) {
	std::unique_lock<std::shared_mutex> lock(nd_mtx);
	nxt_close.insert(ses);
}
#pragma endregion
