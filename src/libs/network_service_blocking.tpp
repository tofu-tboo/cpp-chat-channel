#include "session_event_handler.h"
#include "network_service_blocking.h"

template <typename T>
NetworkServiceBlocking<T>::NetworkServiceBlocking(const int port, const NOTF_MODE m, const msec to): NetworkService<T>(port), mode(m), timeout(to) {}

template <typename T>
void NetworkServiceBlocking<T>::serve() {
	LOG(_CG_ "NetworkServiceBlocking [%14p]" _EC_ " / " _CG_ "Serve on." _EC_, (void*)this);
	{	
		std::unique_lock<std::mutex> lock(service_mtx);
		NetworkService<T>::serve();
		service_cv.wait_for(lock, std::chrono::milliseconds(timeout));
	}
	LOG(_CG_ "NetworkServiceBlocking [%14p]" _EC_ " / " _CG_ "Serve off." _EC_, (void*)this);
}

template <typename T>
void NetworkServiceBlocking<T>::post_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	if (mode == NOTF_MODE::ENTIRE)
		service_cv.notify_all();
	else if (mode == NOTF_MODE::ONCE)
		service_cv.notify_one();
}

template <typename T>
void NetworkServiceBlocking<T>::set_mode(const NOTF_MODE m) {
	mode = m;
}

template <typename T>
void NetworkServiceBlocking<T>::set_timeout(const msec to) {
	if (to < 0) return;
	timeout = to;
}

