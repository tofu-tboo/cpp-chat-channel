#ifndef __SOCKET_EVENTER_H__
#define __SOCKET_EVENTER_H__

#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

#include "socket.h"
#include "session_event_handler.h"
#include "dto.h"

// // 해당 wsi에 대해 30초의 타임아웃을 설정
// lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE, 30);

/* TODO
- TCP ping-pong
- close & send queue: flush() to cancel_service
- broadcast needed to be based on channel: middleware?, grouping

*/

// template<typename T, typename = void>
// struct has_wsi : std::false_type {};

// template<typename T>
// struct has_wsi<T, std::void_t<decltype(std::declval<T>().wsi)>> : std::true_type {};

template <typename T>
class NetworkService {
	// static_assert(has_wsi<T>::value, "No wsi field found.");
	public:
		typedef struct {
			private:
				SessionEvHandler<T>* handler;
				lws* wsi;
				friend class NetworkService<T>;
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

		// Queue
		std::map<int, std::set<Session*>> session_group;
		std::map<lws*, std::string> del_resv; // save msg as string since easier auto free
		std::map<lws*, std::queue<std::vector<unsigned char>>> send_resv;

		std::shared_mutex sg_mtx;
		std::shared_mutex dr_mtx;
		std::shared_mutex sr_mtx;
	public:
		NetworkService(const int port);
		~NetworkService();

		void setup(SessionEvHandler<T>* i_handler);

		void serve(const msec to);

		void send_async(Session* ses, const std::string& msg);
		void send_async(Session* ses, const unsigned char* data, size_t len);

		void broadcast_async(std::string& msg);
		void broadcast_async(const unsigned char* data, size_t len);
		void broadcast_group_async(int group, std::string& msg);
		void broadcast_group_async(int group, const unsigned char* data, size_t len);

		void close_async(Session* ses, const std::string& msg);
		void close_async(Session* ses, const unsigned char* data, size_t len);

		void flush();
		// ctx* get_ctx() const;
	private:
		void accumulate(lws* wsi, const unsigned char* data, size_t len);
};

#include "network_service.tpp"

#endif 