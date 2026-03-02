#ifndef __LWS_SERVICE_H__
#define __LWS_SERVICE_H__

#define __CALLBACK_SAFE__

// timeout event flag
#define TO_EV_NONE				(0)
#define TO_EV_PING_PONG			(1)
#define TO_EV_TOKEN_REFILL		(1 << 2)

#include <libwebsockets.h>
#include <string>
#include <vector>
#include "network_service.h"
#include "set_super.h"
#include "../dynamic_compile/dynamic_compile.h"

typedef struct lws_context ctx;
typedef struct lws_context_creation_info ctx_creation_info;
typedef struct lws_protocols protocols_t;
typedef struct lws lws;
typedef enum lws_callback_reasons callback_reason;
typedef lws_sorted_usec_list_t utimer_list_t;


template <typename T>
class LwsService: public NetworkService<T>, virtual public Loggable {
	SET_SUPER(NetworkService<T>);
	type_protected:
		USING_TYPENAME(Session, NetworkService<T>);

		__USING_SUPER_MEM__
		using NetworkService<T>::accumulate;
		using NetworkService<T>::broadcast;
		using NetworkService<T>::broadcast_group;
		using NetworkService<T>::close;
		using NetworkService<T>::del_resv;
		using NetworkService<T>::handler;
		using NetworkService<T>::send;
		using NetworkService<T>::send_resv;
		using NetworkService<T>::serve;
		using NetworkService<T>::session_group;
		using NetworkService<T>::setup;
		using NetworkService<T>::stop;
		using NetworkService<T>::thread_pool;

		struct LwsSession {
			msec64 last_act;
			int to_flag; // bit mask
			lws* wsi;
			LwsSession(): last_act(0), to_flag(TO_EV_NONE), wsi(nullptr) {}
		};

		struct SulWrapper {
			utimer_list_t sul;
			LwsService<T>* service;
			int tsi;
		};
	static_var_private:
		static protocols_t protocols[];
	static_func_private:
		static int lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
		__CALLBACK_SAFE__ static void quit_service(utimer_list_t* sul);
		__CALLBACK_SAFE__ static void reserve_quit(LwsService<T>* service);
	var_private:
		ctx* context;
		ctx_creation_info info;

		bool fl_resv;

		SulWrapper tlist_wrapper;
	func_public:
		LwsService(const port_t port, const size_t tcnt = 1);
		~LwsService();

		virtual void setup(SessionEvHandler<T>* i_handler) override final;
		virtual void stop() override final;

		virtual void serve() override final;

		virtual void send(Session* ses, const unsigned char* data, size_t len) override final;

		virtual void broadcast(const unsigned char* data, size_t len) override final;
		virtual void broadcast_group(int group, const unsigned char* data, size_t len) override final;

		virtual void close(Session* ses, const unsigned char* data, size_t len) override final;
	func_protected:
		virtual void accumulate(Session* ses, const unsigned char* data, size_t len) override final;
	func_private:
		void flush();
		friend void ThreadPool<T, "lws">::stop();

		__CALLBACK_SAFE__ void check_pong(Session* ses);
		__CALLBACK_SAFE__ void set_timeout(Session* ses, lws* wsi, int flag);
		__CALLBACK_SAFE__ std::string get_ip(lws* wsi);
};

template <typename T>
class ThreadPool<T, "lws">: public IThreadPool<T> {
	SET_SUPER(IThreadPool<T>);
	type_protected:
		__USING_SUPER_MEM__
		using IThreadPool<T>::is_running_flag;
		using IThreadPool<T>::join;
		using IThreadPool<T>::service;
		using IThreadPool<T>::size;
		using IThreadPool<T>::start;
		using IThreadPool<T>::stop;
	static_var_protected:
		static thread_local int tsi;
	var_private:
		std::vector<std::thread> threads;
	func_public:
		ThreadPool(NetworkService<T>* service, const size_t s): IThreadPool<T>(service, s) {}
		virtual ~ThreadPool() {
			if (is_running_flag) {
				stop();
			}
			join();
		}

		virtual void start() override final {
			is_running_flag = true;
			for (size_t i = 0; i < size; i++) {
				threads.emplace_back([this, i]() {
					tsi = (int)i;
					while (is_running_flag) {
						service->serve();
					}
				});
			}
		}
		virtual void stop() override final {
			super::stop();

			auto service = dynamic_cast<LwsService<T>*>(this->service);
			service->flush();
		}
		virtual void join() override final {
			for (auto& thread : threads) {
				if (thread.joinable())
					thread.join();
			}
		}

		int get_tsi() const {
			return tsi;
		}
};

template <typename T>
thread_local int ThreadPool<T, "lws">::tsi = 0;

#include "lws_service.tpp"

#endif