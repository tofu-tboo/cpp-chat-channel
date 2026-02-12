#ifndef CHANNEL_FACTORY_H
#define CHANNEL_FACTORY_H

#include "channel.h"
#include "user_manager.h"

class ChannelServer;

class ChannelFactory {
public:
    ChannelFactory(NetworkService<User>* service, int max_conn) 
        : service_(service), max_conn_(max_conn) {}

    Channel* create(ChannelServer* server, ch_id_t id) const {
        return new Channel(service_, server, id, max_conn_);
    }

private:
    NetworkService<User>* service_;
    int max_conn_;
};

#endif