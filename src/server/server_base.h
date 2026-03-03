#ifndef __SERVER_BASE_H__
#define __SERVER_BASE_H__

#define POOL_FULL      601

#define iERROR(...)         LOG2(_CR_ "[%x] " _EC_, branch_id); ERROR(__VA_ARGS__)

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <deque>
#include <queue>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <memory>

#include "../libs/loggable.h"
#include "../libs/class.h"
#include "../libs/network_service.h"
#include "../libs/session_event_handler.h"
#include "../libs/cron_worker.h"

/*
All servers have only one shared file descriptor listening on a port.
ServerBase assumed that it has one channel.
*/

/* Requirement of ServerBase 
- fd Management: Manage clinets connected to the listening fd. Entrusted to ConnectionTracker class.
- Frame Handling: receive and send frames from clients with customizable frame format. But, default is 4-byte length header + payload. Entrusted to Communication class.
- Separate Tasks: Use TaskRunner to separate tasks like polling, deletion resolution, payload resolution. But, ServerBase only does polling and deletion resolution. The payload resolution is left to derived classes. 
*/

struct Request;
class ServerFactory;
class IMsgTranslator;

template <typename U>
class ServerBase: public SessionEvHandler<U>, virtual public Loggable {
    type_protected:
		USING_TYPENAME(Session, SessionEvHandler<U>);

		enum TaskSession {
            TS_PRE = 0,   	// 전처리: 큐 소비, 버퍼 정리
            TS_POLL,  		// I/O: 폴링, 이벤트 처리
            TS_LOGIC, 		// 로직: 메시지 처리, 브로드캐스트, 삭제
            TS_COUNT
        };
	var_protected:
        const int branch_id; // branch's id
		std::shared_ptr<NetworkService<U>> service;
		std::unique_ptr<IMsgTranslator> msg_translator;

        std::unordered_set<Session*> nxt_close;

		std::shared_mutex nc_mtx;

		const unsigned int max_conn;
		std::atomic<unsigned int> cur_conn;

		CronWorker cron_worker;
    func_public:
        ServerBase(std::shared_ptr<NetworkService<U>> di_service, std::unique_ptr<IMsgTranslator> processor, const int max_fd = 256);
        virtual ~ServerBase();

		virtual bool init();
        virtual void proc(); // 외부에서의 서버 진입점
        void stop();

    func_protected:
        void resolve_close();

        virtual void on_accept(Session& ses);
		virtual void on_close(Session& ses) final;
		virtual void on_recv(Session& ses, const RecvStream& stream) final;
		virtual void on_send(Session& ses);
		virtual void on_rate_limit_packet_drop(Session& ses);

		virtual void handle_request(Session& ses, std::unique_ptr<Request> req) = 0;

		virtual void free_user(Session& ses);
		void rsv_close(Session* ses);
};

#include "server_base.tpp"

#endif
