#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <unordered_map>
#include <queue>
#include <mutex>

#include "server_base.h"
#include "channel.h"
#include "../libs/json.h"
#include "../libs/socket.h"

class ChannelServer: public ServerBase {
    public:
        struct ChannelReq {
			enum Type { SWITCH } type;
            fd_t fd;
            ch_id_t target;
            bool is_leave; // true: go to lobby, false: go to target channel
			std::string payload;
        };
    private:
        std::unordered_map<ch_id_t, Channel*> channels;
        std::queue<ChannelReq> req_queue;
        std::mutex req_mtx;
    public:
        ChannelServer(const int max_fd = 256, const msec to = 0);
        ~ChannelServer();
        void report(const ChannelReq& req);
    protected:
        virtual void on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) override;
		void consume_report();
    };


#endif