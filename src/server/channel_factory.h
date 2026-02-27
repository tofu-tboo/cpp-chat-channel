#ifndef CHANNEL_FACTORY_H
#define CHANNEL_FACTORY_H

#include <memory>
#include "channel.h"

class ChannelServer;

class ChannelFactory {
public:
    ChannelFactory(std::shared_ptr<NetworkService<User>> service, int max_conn) 
        : service_(std::move(service)), max_conn_(max_conn) {}

    Channel* create(ChannelServer* server, ch_id_t id) const {
        return new Channel(service_, server, id, max_conn_);
    }

private:
    std::shared_ptr<NetworkService<User>> service_;
    int max_conn_;
};

#endif