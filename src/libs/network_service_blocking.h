#ifndef __NETWORK_SERVICE_BLOCKING_H__
#define __NETWORK_SERVICE_BLOCKING_H__

#include <mutex>
#include <condition_variable>

#include "util.h"

template <typename T>
class NetworkServiceBlocking: public NetworkService<T> {
	public:
		enum NOTF_MODE { ONCE, ENTIRE };
	private:
		std::condition_variable service_cv;
		std::mutex service_mtx;

		NOTF_MODE mode;
		msec timeout;
	public:
		NetworkServiceBlocking(const int port, const NOTF_MODE m = NOTF_MODE::ENTIRE, const msec to = 0);
		
		void set_mode(const NOTF_MODE m);
		void set_timeout(const msec to);

		virtual void serve() override;
	
	protected:
		virtual void post_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len) override;
};

#include "network_service_blocking.tpp"

#endif