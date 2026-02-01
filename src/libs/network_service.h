#ifndef __SOCKET_EVENTER_H__
#define __SOCKET_EVENTER_H__

#include <type_traits>
#include <unordered_set>
#include <shared_mutex>

#include "socket.h"
#include "session_event_handler.h"
#include "dto.h"

// // 해당 wsi에 대해 30초의 타임아웃을 설정
// lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE, 30);

// // TCP 연결 후 웹소켓 핸드셰이크가 완료되어야 하는 제한 시간 (초)
// info.timeout_secs = 15;

//
//lws_cancel_service <- service blocking 무기 => awake

// 즉시 소켓을 닫고 wsi 메모리를 해제합니다. (lws 스레드 내부 권장)
// lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NORMAL);

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
		int ctx_port;
		std::unordered_set<lws*> session_inst;
		std::shared_mutex mtx;
	public:
		NetworkService(const int port);
		~NetworkService();

		void setup(SessionEvHandler<T>* i_handler);

		void serve(const msec to);

		void send(lws* wsi, const std::string& msg);
		void send(lws* wsi, const unsigned char* data, size_t len);
		void broadcast(std::string& msg);
		void broadcast(const unsigned char* data, size_t len);
		// ctx* get_ctx() const;
	private:
		void accumulate(lws* wsi, const unsigned char* data, size_t len);
};

#include "network_service.tpp"

#endif 