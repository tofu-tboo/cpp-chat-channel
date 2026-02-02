#ifndef __SOCKET_EVENTER_H__
#define __SOCKET_EVENTER_H__

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

#include "socket.h"
#include "session_event_handler.h"
#include "dto.h"

// // 해당 wsi에 대해 30초의 타임아웃을 설정
// lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE, 30);

// TODO: TCP ping-pong

// template<typename T, typename = void>
// struct has_wsi : std::false_type {};

// template<typename T>
// struct has_wsi<T, std::void_t<decltype(std::declval<T>().wsi)>> : std::true_type {};

template <typename T>
class NetworkService {
	// static_assert(has_wsi<T>::value, "No wsi field found.");
	public:
		typedef struct {
			SessionEvHandler<T>* handler;
			T user;

			std::queue<std::vector<unsigned char>>* buf;
        	std::mutex* mtx;
			
			protocol_id prot_id;
		} Session;
	private:
		static protocols_t protocols[];
	private:
		static int lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len);
	private:
		ctx* context;
		ctx_creation_info info;

		
		std::unordered_set<lws*> session_inst;
		std::unordered_map<T*, lws*> user_map;
		std::unordered_set<std::pair<lws*, std::string>> del_resv; // save msg as string since easier auto free
		
		std::shared_mutex ses_inst_mtx;
		std::shared_mutex umap_mtx;
		std::shared_mutex del_resv_mtx;
	public:
		NetworkService(const int port);
		~NetworkService();

		void setup(SessionEvHandler<T>* i_handler);

		void serve(const msec to);

		lws* get_wsi(T* user);

		void send_async(lws* wsi, const std::string& msg);
		void send_async(lws* wsi, const unsigned char* data, size_t len);
		void send_async(T* user, const std::string& msg);
		void send_async(T* user, const unsigned char* data, size_t len);
		void broadcast_async(std::string& msg);
		void broadcast_async(const unsigned char* data, size_t len);
		
		void close_async(lws* wsi, const std::string& msg);
		void close_async(lws* wsi, const unsigned char* data, size_t len);
		void close_async(T* user, const std::string& msg);
		void close_async(T* user, const unsigned char* data, size_t len);
		// ctx* get_ctx() const;
	private:
		void accumulate(lws* wsi, const unsigned char* data, size_t len);
};

#include "network_service.tpp"

#endif 