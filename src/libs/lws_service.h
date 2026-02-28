#ifndef __LWS_SERVICE_H__
#define __LWS_SERVICE_H__

#define __CALLBACK_SAFE__

// timeout event flag
#define TO_EV_PING_PONG			(1)
#define TO_EV_TOKEN_REFILL		(1 << 2)

#include "network_service.h"

typedef struct lws_context ctx;
typedef struct lws_context_creation_info ctx_creation_info;
typedef struct lws_protocols protocols_t;
typedef struct lws lws;
typedef enum lws_callback_reasons callback_reason;
typedef lws_sorted_usec_list_t utimer_list_t;


template <typename T>
class LwsService: public NetworkService<T> {
	protected:
		using Session = typename NetworkService<T>::Session;

		using NetworkService<T>::log;
		using NetworkService<T>::elog;
		using NetworkService<T>::accumulate;
		using NetworkService<T>::session_group;
		using NetworkService<T>::del_resv;
		using NetworkService<T>::send_resv;

		using SA = typename NetworkService<T>::SessionAccessor;

		struct LwsSession {
			int to_flag; // bit mask
			lws* wsi;
		}
	private:
		static protocols_t protocols[];
	private:
		static int lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
	private:
		ctx* context;
		ctx_creation_info info;

		bool fl_resv;

		AutoLockContainer<std::map<Session*, lws*>> wsi_map;

		utimer_list_t tlist;
	public:
		LwsService(const int port);
		~LwsService();

		virtual void setup(SessionEvHandler<T>* i_handler);

		virtual void serve();

		virtual void send(Session* ses, const std::string& msg);
		virtual void send(Session* ses, const unsigned char* data, size_t len);

		virtual void broadcast(const std::string& msg);
		virtual void broadcast(const unsigned char* data, size_t len);
		virtual void broadcast_group(int group, const std::string& msg);
		virtual void broadcast_group(int group, const unsigned char* data, size_t len);

		virtual void change_session_group(Session* ses, int new_group);
		virtual void register_handler(Session* ses, SessionEvHandler<T>* handler);

		virtual void close(Session* ses, const std::string& msg);
		virtual void close(Session* ses, const unsigned char* data, size_t len);
	private:
		void flush();
		__CALLBACK_SAFE__ void check_pong(Session* ses);

		__CALLBACK_SAFE__ void set_timeout(Session* ses, lws* wsi, int flag);
};


#include "lws_service.tpp"

#endif