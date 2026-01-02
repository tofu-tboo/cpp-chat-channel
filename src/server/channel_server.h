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
    public:
        ChannelServer();
        ~ChannelServer();
    protected:
        virtual void on_switch(const char* target, Json& root, const std::string& payload);
    };


#endif