#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <unordered_map>

#include "server_base.h"
#include "channel.h"
#include "../libs/json.h"
#include "../libs/socket.h"

class ChannelServer: public ServerBase {
    private:
        std::unordered_map<ch_id_t, Channel*> channels;
        std::unordered_map<fd_t, ch_id_t> user_map;
    public:
        ChannelServer(const int max_fd = 256, const msec to = 0);
        ~ChannelServer();
    protected:
        virtual void on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) override;
    };


#endif