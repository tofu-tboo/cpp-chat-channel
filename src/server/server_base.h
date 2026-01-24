#ifndef __SERVER_BASE_H__
#define __SERVER_BASE_H__

#define iERROR(...)         LOG2(_CR_ "[%x] " _EC_, branch_id); ERROR(__VA_ARGS__)

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <algorithm>

#include "../libs/json.h"
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
		static std::unordered_map<fd_t, std::string> name_map; // username mapping
		static std::mutex name_map_mtx;
    protected:
        int branch_id; // manager branch's id
        ConnectionTracker* con_tracker;
		Communication* comm;

        msec timeout;

        std::vector<fd_t> next_deletion;

        TaskRunner<void()> task_runner;
    public:
        ServerBase(const int max_fd = 256, const msec to = 0);
        ~ServerBase();

        virtual void proc(); // 외부에서의 서버 진입점

		bool get_user_name(const fd_t fd, std::string& out_user_name) const;
		void set_user_name(const fd_t fd, const std::string& user_name);
		void remove_user_name(const fd_t fd);
		

    private:
        void set_network();
        void handle_events(const pollev event);
    protected:
        // Tasks
        // virtual void frame();
        virtual void resolve_deletion();

        // Hooks
        virtual void on_req(const fd_t from, const char* target, Json& root); // handle both pure json & payload
        virtual void on_accept();
		virtual void on_disconnect(const fd_t fd);
		virtual void on_recv(const fd_t from);
};

#endif
