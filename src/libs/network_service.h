#ifndef __NETWORK_SERVICE_H__
#define __NETWORK_SERVICE_H__

#define MAX_FRAME_SIZE			(2048)
#define MAX_THREAD				(8)

// Define protocol num
#define PROT_NONE	(-1)
#define TCP			(0)
#define WS			(1)
#define WS_NAME		("ws")
#define TCP_NAME	("tcp")

// Rate Limit Config
#define RL_BURST_MAX            (20u)      // 초기/최대 버킷 크기 (패킷 수)
#define RL_REFILL_SEC           (10)      // 리필 주기 (초)
#define MAX_ALLOWED_WSI_PER_IP  (20)       // IP당 최대 연결 수


#include <queue>
#include <set>
#include <map>
#include <vector>
#include <unordered_map>
#include <climits>
#include <thread>
#include <functional>
#include <atomic>

#include "dto.h"
#include "loggable.h"
#include "auto_lock_container.h"
#include "times.h"
#include "class.h"

typedef short protocol_id;
typedef unsigned short port_t;

enum NS_EV { NONE, ACPT, RECV, SEND, CLOSE, RL_DROP };

template <typename T>
class SessionEvHandler;

template <typename T>
class IThreadPool;

template <typename T, FixedString Str>
class ThreadPool;

// NetworkService allows the packets to be limited as certain size.
template <typename T>
class NetworkService: virtual public Loggable {
	forward_protected:
		struct SessionSecret;
	type_public:
		struct Session {
			SessionSecret* secret;
			T* user;
			int group;
			Session(): secret(new SessionSecret()), user(new T()), group(INT_MIN) {}
			Session(NetworkService<T>* service): secret(new SessionSecret(service)), user(new T()), group(INT_MIN) {}
			~Session() {
				if (secret)
					delete secret;
				if (user)
					delete user;
			}
		};

		struct CallbackParam {
			Session* ses;
			NS_EV event;
			unsigned char* in;
			size_t len;
		};
	type_protected:
		struct SessionSecret {
			SessionEvHandler<T>* handler;
			protocol_id prot_id;
			void* extra;
			// Rate Limiting (Token Bucket)
			unsigned int tokens;
			std::string ip;
			SessionSecret(): handler(nullptr), prot_id(PROT_NONE), extra(nullptr), tokens(0) {}
			SessionSecret(NetworkService<T>* service): handler(service->handler), prot_id(PROT_NONE), extra(nullptr), tokens(RL_BURST_MAX) {}
		};
	var_protected:
		// Queue
		AutoLockContainer<std::map<int, std::set<Session*>>> session_group; // TODO?: AutoLockContainer<std::map<int, AutoLockContainer<std::set<Session*>>>>
		AutoLockContainer<std::map<Session*, std::string>> del_rsv; // save msg as string since easier auto free
		AutoLockContainer<std::map<Session*, std::queue<std::vector<unsigned char>>>> send_rsv;
		AutoLockContainer<std::unordered_map<std::string, int>> ip_conn_map;

		SessionEvHandler<T>* handler; // initial client-handler

		IThreadPool<T>* thread_pool;
	func_public:
		NetworkService();
		~NetworkService();

		virtual void setup(SessionEvHandler<T>* i_handler);
		virtual void start();
		virtual void stop();

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

		virtual void join();

		bool is_running() const;
	func_protected:
		virtual void accumulate(Session* ses, const unsigned char* data, size_t len) = 0;

		// for C-style libraries
		void construct_session(Session* ses);
		void destruct_session(Session* ses);
};

template <typename T>
class IThreadPool {
	var_protected:
		size_t size;
		NetworkService<T>* service;
		std::atomic<bool> is_running_flag;
		friend class NetworkService<T>;
	func_public:
		IThreadPool(NetworkService<T>* service, const size_t s): service(service), is_running_flag(false) {
			if (s > MAX_THREAD)
				size = MAX_THREAD;
			else
				size = s;
		};
		virtual ~IThreadPool() = default;

		virtual void start() = 0;
		virtual void stop() {
			is_running_flag = false;
		}
		virtual void join() = 0;
		
		size_t get_size() const {
			return size;
		};

		bool is_running() const {
			return is_running_flag;
		}
};

#include "network_service.tpp"

#endif