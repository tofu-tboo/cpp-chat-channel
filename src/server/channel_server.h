#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <unordered_map>

#include "server_base.h"
#include "lobby_server.h"
#include "channel.h"

class ChannelServer: public ServerBase {
    private:
        std::unordered_map<ch_id_t, Channel*> channels;
    public:
        ChannelServer();
        ~ChannelServer();
        virtual void proc(const msec to = 0);

    protected:
        virtual void handle_events(const pollev event);
        virtual bool add_listener(const fd_t fd);
        virtual bool delete_listener(const fd_t fd);
        virtual bool recv_frame(const fd_t fd);
        virtual bool send_frame(const fd_t fd, const std::string& payload);
        virtual void broadcast(const std::string& payload);
        virtual void resolve_timestamps();
        virtual void resolve_payload(const std::string& payload);
};


#endif