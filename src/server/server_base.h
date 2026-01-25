#ifndef __SERVER_BASE_H__
#define __SERVER_BASE_H__

#define iERROR(...)         LOG2(_CR_ "[%x] " _EC_, branch_id); ERROR(__VA_ARGS__)

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <deque>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <atomic>

#include "../libs/util.h"
#include "../libs/socket.h"
#include "../libs/connection_tracker.h"
#include "../libs/task_runner.h"
#include "../libs/communication.h"

/*
All servers have only one shared file descriptor listening on a port.
ServerBase assumed that it has one channel.
*/

/* Requirement of ServerBase 
- fd Management: Manage clinets connected to the listening fd. Entrusted to ConnectionTracker class.
- Frame Handling: receive and send frames from clients with customizable frame format. But, default is 4-byte length header + payload. Entrusted to Communication class.
- Separate Tasks: Use TaskRunner to separate tasks like polling, deletion resolution, payload resolution. But, ServerBase only does polling and deletion resolution. The payload resolution is left to derived classes. 
*/

class ServerBase {
    protected:
        static fd_t fd;
    protected:
        int branch_id; // manager branch's id
        ConnectionTracker* con_tracker;
		Communication* comm;

        enum TaskSession {
            TS_PRE = 0,   // 전처리: 큐 소비, 버퍼 정리
            TS_POLL = 1,  // I/O: 폴링, 이벤트 처리
            TS_LOGIC = 2, // 로직: 메시지 처리, 브로드캐스트, 삭제
            TS_COUNT = 3
        };

        msec timeout;

        std::unordered_set<fd_t> next_deletion;

        TaskRunner<void()> task_runner;
        std::atomic<bool> is_running;
    public:
        ServerBase(const int max_fd = 256, const msec to = 0);
        ~ServerBase();

        virtual void proc(); // 외부에서의 서버 진입점
        void stop();


    private:
        void set_network();
        void handle_events(const pollev event);
    protected:
        // Tasks
        // virtual void frame();
        virtual void resolve_deletion();

        // Hooks
        virtual void on_frame(const fd_t from, const std::string& frame);
        virtual void on_accept(const fd_t client);
		virtual void on_disconnect(const fd_t fd);
		virtual void on_recv(const fd_t from);
};

#endif
