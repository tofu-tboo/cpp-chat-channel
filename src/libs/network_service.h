#ifndef __SOCKET_EVENTER_H__
#define __SOCKET_EVENTER_H__

#define MAX_FRAME_SIZE			(2048)

// timeout event flag
#define TO_EV_PING_PONG			(1)
#define TO_EV_TOKEN_REFILL		(1 << 2)

// Rate Limit Config
#define RL_BURST_MAX            (20u)      // 초기/최대 버킷 크기 (패킷 수)
#define RL_REFILL_SEC           (10)      // 리필 주기 (초)


#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

#include "socket.h"
#include "session_event_handler.h"
#include "dto.h"
#include "util.h"

// NetworkService allows the packets to be limited as certain size.

template <typename T>
class NetworkService {
	public:
		typedef struct {
			private:
				SessionEvHandler<T>* handler;
				lws* wsi;
				friend class NetworkService<T>;
				msec64 last_act;
				// lws timer flag
				int to_flag;
				// Rate Limiting (Token Bucket)
				unsigned int tokens;
			public:
				T* user;
				protocol_id prot_id;
				int group;

				
		} Session;
	private:
		static protocols_t protocols[];
	private:
		static int lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
	private:
		ctx* context;
		ctx_creation_info info;

		bool fl_resv;

		// Queue
		std::map<int, std::set<Session*>> session_group;
		std::map<lws*, std::string> del_resv; // save msg as string since easier auto free
		std::map<lws*, std::queue<std::vector<unsigned char>>> send_resv;

		std::shared_mutex sg_mtx;
		std::shared_mutex dr_mtx;
		std::shared_mutex sr_mtx;

		SessionEvHandler<T>* handler; // initial client-handler
	public:
		NetworkService(const int port);
		~NetworkService();

		virtual void setup(SessionEvHandler<T>* i_handler);

		virtual void serve();

		void send_async(Session* ses, const std::string& msg);
		void send_async(Session* ses, const unsigned char* data, size_t len);

		void broadcast_async(const std::string& msg);
		void broadcast_async(const unsigned char* data, size_t len);
		void broadcast_group_async(int group, const std::string& msg);
		void broadcast_group_async(int group, const unsigned char* data, size_t len);

		void change_session_group(Session* ses, int new_group);
		void register_handler(Session* ses, SessionEvHandler<T>* handler);

		void close_async(Session* ses, const std::string& msg);
		void close_async(Session* ses, const unsigned char* data, size_t len);

		void flush();

		// ctx* get_ctx() const;
	private:
		void accumulate(Session* ses, const unsigned char* data, size_t len);
		void check_pong(Session* ses);

		void set_timeout(Session* ses, int flag);
	// protected:
		// virtual void pre_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
		// virtual void post_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
};

#include "network_service.tpp"

#endif 