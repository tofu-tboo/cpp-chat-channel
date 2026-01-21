#ifndef __SERVER_BASE_H__
#define __SERVER_BASE_H__

#define MAX_FRAME_SIZE      16 * 1024

#define iERROR(...)         LOG(_CR_ "[%x] " _EC_, branch_id); ERROR(__VA_ARGS__)

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <cstdint>
#include <stdexcept>

#include "../libs/json.h"
#include "../libs/socket.h"
#include "../libs/connection_tracker.h"
#include "../libs/task_runner.h"

/*
All servers have only one shared file descriptor listening on a port.
ServerBase assumed that it has one channel.
*/

class ServerBase {
    protected:
        static fd_t fd;
		static std::unordered_map<fd_t, std::string> name_map; // username mapping
    protected:
        int branch_id; // manager branch's id
        ConnectionTracker* con_tracker;

        msec timeout;

        std::unordered_map<fd_t, std::string> rbuf; // per-connection accumulation buffer
        std::vector<std::string> mq; // parsed json frames
        std::multimap<msec64, std::string> cur_msgs; // sorted by timestamp
        std::vector<fd_t> next_deletion;

        TaskRunner<void()> task_runner;
    public:
        ServerBase(const int max_fd = 256, const msec to = 0);
        ~ServerBase();

        virtual void proc();

    private:
        void set_network();
        void handle_events(const pollev event);
    protected:
        // Helpers
        virtual void recv_frame(const fd_t fd); // frame format can be overridden
        virtual void send_frame(const fd_t fd, const std::string& payload); // frame format can be overridden
        virtual void broadcast(const std::string& payload);

        // Tasks
        virtual void frame();
        virtual void resolve_timestamps();
        virtual void resolve_payload(const fd_t from, const std::string& payload);
        virtual void resolve_broadcast();
        virtual void resolve_deletion();

        // Hooks
        virtual void on_switch(const fd_t from, const char* target, Json& root, const std::string& payload); // handle both pure json & payload
        virtual void on_accept();
};

#endif
