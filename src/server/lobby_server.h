#ifndef __LOBBY_SERVER_H__
#define __LOBBY_SERVER_H__

#include "server_base.h"

class LobbyServer: public ServerBase {
    private:

    public:
        LobbyServer();
        ~LobbyServer();
        // virtual void proc(const msec to = 0);

    protected:
        // virtual void handle_events(const pollev event);
        // virtual bool add_listener(const fd_t fd);
        // virtual bool delete_listener(const fd_t fd);
        // virtual bool recv_frame(const fd_t fd);
        // virtual bool send_frame(const fd_t fd, const std::string& payload);
        // virtual void broadcast(const std::string& payload);
        // virtual void resolve_timestamps();
        // virtual void resolve_payload(const std::string& payload);
};


#endif