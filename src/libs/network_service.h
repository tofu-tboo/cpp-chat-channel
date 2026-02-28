#ifndef __SOCKET_EVENTER_H__
#define __SOCKET_EVENTER_H__

#define MAX_FRAME_SIZE			(2048)

// Define protocol num
#define TCP			(0)
#define WS			(1)
#define WS_NAME		("ws")
#define TCP_NAME	("tcp")

// Rate Limit Config
#define RL_BURST_MAX            (20u)      // 초기/최대 버킷 크기 (패킷 수)
#define RL_REFILL_SEC           (10)      // 리필 주기 (초)


#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <libwebsockets.h>

#include "dto.h"
#include "loggable.h"
#include "auto_lock_container.h"
#include "times.h"
#include "class.h"

typedef short protocol_id;

enum NS_EV { NONE, ACPT, RECV, SEND, CLOSE, RL_DROP };

template <typename T>
class SessionEvHandler;

// NetworkService allows the packets to be limited as certain size.
template <typename T>
class NetworkService: virtual public Loggable {
	forward_protected:
		struct SessionAccessor;
	type_public:
		struct Session {
			private:
				friend NetworkService<T>;
				friend SessionAccessor;
				SessionEvHandler<T>* handler;
				protocol_id prot_id;
				void* extra;
				msec64 last_act;
				int to_flag; // TODO: last_act, lws service의 wsi와 더불어 void*로 관리 고려.
				// Rate Limiting (Token Bucket)
				unsigned int tokens;
			public:
				T* user;
				int group;
		};

		struct CallbackParam {
			Session* ses;
			NS_EV event;
			unsigned char* in;
			size_t len;
		};
	type_protected:
		struct SessionAccessor {
			static auto& handler(Session* s) { return s->handler; }
			static auto& prot_id(Session* s) { return s->prot_id; }
			static auto& last_act(Session* s) { return s->last_act; }
			static auto& to_flag(Session* s) { return s->to_flag; }
			static auto& tokens(Session* s) { return s->tokens; }
		};
	var_protected:
		// Queue
		AutoLockContainer<std::map<int, std::set<Session*>>> session_group; // TODO?: AutoLockContainer<std::map<int, AutoLockContainer<std::set<Session*>>>>
		AutoLockContainer<std::map<Session*, std::string>> del_resv; // save msg as string since easier auto free
		AutoLockContainer<std::map<Session*, std::queue<std::vector<unsigned char>>>> send_resv;

		SessionEvHandler<T>* handler; // initial client-handler
	func_public:
		NetworkService();
		~NetworkService();

		virtual void setup(SessionEvHandler<T>* i_handler);

		virtual void serve() = 0;

		virtual void send(Session* ses, const std::string& msg);
		virtual void send(Session* ses, const unsigned char* data, size_t len) = 0;

		virtual void broadcast(const std::string& msg);
		virtual void broadcast(const unsigned char* data, size_t len) = 0;
		virtual void broadcast_group(int group, const std::string& msg);
		virtual void broadcast_group(int group, const unsigned char* data, size_t len) = 0;

		virtual void change_session_group(Session* ses, int new_group);
		virtual void register_handler(Session* ses, SessionEvHandler<T>* handler);

		virtual void close(Session* ses, const std::string& msg);
		virtual void close(Session* ses, const unsigned char* data, size_t len) = 0;
	func_protected:
		void accumulate(Session* ses, const unsigned char* data, size_t len);
};

#include "network_service.tpp"

#endif 