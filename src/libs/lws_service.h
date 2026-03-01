#ifndef __LWS_SERVICE_H__
#define __LWS_SERVICE_H__

#define __CALLBACK_SAFE__

// timeout event flag
#define TO_EV_NONE				(0)
#define TO_EV_PING_PONG			(1)
#define TO_EV_TOKEN_REFILL		(1 << 2)

#include <libwebsockets.h>
#include <string>
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
		USING_SESSION_TYPENAME(T);

		__USING_SUPER_MEM__
		using NetworkService<T>::SessionSecret;
		using NetworkService<T>::broadcast;
		using NetworkService<T>::broadcast_group;
		using NetworkService<T>::change_session_group;
		using NetworkService<T>::close;
		using NetworkService<T>::construct_session;
		using NetworkService<T>::del_resv;
		using NetworkService<T>::destruct_session;
		using NetworkService<T>::handler;
		using NetworkService<T>::ip_conn_map;
		using NetworkService<T>::register_handler;
		using NetworkService<T>::send;
		using NetworkService<T>::send_resv;
		using NetworkService<T>::session_group;

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
		LwsService(const int port, const int tcnt = 1);
		~LwsService();

		virtual void setup(SessionEvHandler<T>* i_handler) override final;

		virtual void serve() override final;

		virtual void send(Session* ses, const unsigned char* data, size_t len) override final;

		virtual void broadcast(const unsigned char* data, size_t len) override final;
		virtual void broadcast_group(int group, const unsigned char* data, size_t len) override final;

		virtual void close(Session* ses, const unsigned char* data, size_t len) override final;
	func_protected:
		virtual void accumulate(Session* ses, const unsigned char* data, size_t len) override final;
	func_private:
		void flush();

		__CALLBACK_SAFE__ void check_pong(Session* ses);
		__CALLBACK_SAFE__ void set_timeout(Session* ses, lws* wsi, int flag);
		__CALLBACK_SAFE__ std::string get_ip(lws* wsi);
};


#include "lws_service.tpp"

#endif