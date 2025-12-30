#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

#include "channel_server.h"
#include "../libs/util.h"

ChannelServer::ChannelServer() {
}
ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }
}

void ChannelServer::proc(const msec to) {
}

#pragma region PROTECTED_FUNC

#pragma endregion