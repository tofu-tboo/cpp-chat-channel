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
		dlog("Cron worker thread started.");
		while (service->is_running()) {
			cron_worker.run_pending();
			cron_worker.wait_for_next_task();
		}
		dlog("Cron worker thread finished.");
	});
	dlog("Main proc loop started.");
	while (service->is_running()) {
		std::deque<std::shared_ptr<Request>> reports;
		if (report_q.wait_and_pop_all(reports)) {
			for (std::shared_ptr<Request>& rep: reports) {
				consume_report(rep);
			}
		} else {
			// when MPMC stopped
		}
	}
	dlog("Main proc loop finished. Joining threads...");
	service->join(); // Wait for I/O threads to finish
	dlog("Network service threads joined.");
	if (background.joinable()) {
		background.join(); // Then, wait for the cron worker thread to finish
	}
	dlog("Cron worker thread joined. Shutdown complete.");
}

template <typename U>
void ServerBase<U>::stop() {
	dlog("Stop requested.");
    if (service) {
		dlog("Stopping network service...");
		service->stop();
	}
	dlog("Stopping report queue...");
	report_q.stop();
	dlog("Notifying cron worker to stop...");
	cron_worker.schedule_in(0, [](){}); // Wake up cron worker thread
}

template <typename U>
void ServerBase<U>::report(std::shared_ptr<Request> req) {
	report_q.push(req);
}

template <typename U>
void ServerBase<U>::report(Request* req) {
	report_q.push(std::shared_ptr<Request>(req));
}


#pragma region PRIVATE_FUNC
#pragma endregion

#pragma region PROTECTED_FUNC
template <typename U>
void ServerBase<U>::resolve_close() {
	std::unique_lock lock(nc_mtx);
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
		handle_request(ses, req);
	}
}

template <typename U>
void ServerBase<U>::on_send(Session& ses) {}

template <typename U>
void ServerBase<U>::on_rate_limit_packet_drop(Session& ses) {}

template <typename U>
void ServerBase<U>::consume_report(std::shared_ptr<Request> req) {}

template <typename U>
void ServerBase<U>::free_user(Session& ses) {}

template <typename U>
void ServerBase<U>::rsv_close(Session* ses, const std::string& msg) {
	std::lock_guard lock(nc_mtx);
	nxt_close.insert({ses, msg});
}
#pragma endregion
