#include "server_base.h"
#include "../libs/msg_translator.h"
#include "../libs/times.h"
#include "../libs/exception.h"
#include "../libs/network_service.h"
#include <chrono>

template <typename U>
ServerBase<U>::ServerBase(std::shared_ptr<NetworkService<U>> di_service, std::unique_ptr<IMsgTranslator> processor, const int max): service(std::move(di_service)), msg_translator(std::move(processor)), branch_id(now_ms()), max_conn(max), cur_conn(0), Loggable("ServerBase", _L_BLUE, this) {
	if (!service)
		throw std::runtime_error("Network Service NullPtr.");
	else if (!msg_translator)
		throw std::runtime_error("Message Parser NullPtr.");
}

template <typename U>
bool ServerBase<U>::init() {
	if (service) {
		service->setup(this);

		cron_worker.schedule_every("session deletion", 1000, [this]() {
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
   	service->start();
	std::thread background([this]() {
		while (service->is_running()) {
			cron_worker.run_pending();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});
	service->join(); // Wait for I/O threads to finish
	if (background.joinable()) {
		background.join(); // Then, wait for the cron worker thread to finish
	}
}

template <typename U>
void ServerBase<U>::stop() {
    if (service) service->stop();
}

#pragma region PRIVATE_FUNC
#pragma endregion

#pragma region PROTECTED_FUNC
template <typename U>
void ServerBase<U>::resolve_close() {
	std::shared_lock lock(nc_mtx);
    for (auto [ses, msg] : nxt_close) {
		service->close(ses, msg);
    }
	nxt_close.clear();
}

template <typename U>
void ServerBase<U>::on_accept(Session& ses) {
	try {
		if (cur_conn >= max_conn)
			throw;
		
		cur_conn++;
		log("Accepted new connection: " _L_CYAN "user %p", ses.user);
	} catch (const std::exception& e) {
		service->send(&ses, std::string(R"({"type":"error","message":"Server is full."})"));
		throw e;
    }

}

template <typename U>
void ServerBase<U>::on_close(Session& ses) {
	cur_conn--;
	free_user(ses);
	log("Closed connection: " _L_CYAN "user %p", ses.user);
}

template <typename U>
void ServerBase<U>::on_recv(Session& ses, const RecvStream& stream) {
    if (msg_translator) {
		auto req = msg_translator->decode(std::string(reinterpret_cast<const char*>(stream.data), stream.len));
		handle_request(ses, std::move(req));
	}
}

template <typename U>
void ServerBase<U>::on_send(Session& ses) {}

template <typename U>
void ServerBase<U>::on_rate_limit_packet_drop(Session& ses) {}

template <typename U>
void ServerBase<U>::free_user(Session& ses) {}

template <typename U>
void ServerBase<U>::rsv_close(Session* ses, const std::string& msg) {
	std::unique_lock lock(nc_mtx);
	nxt_close.insert({ses, msg});
}
#pragma endregion
