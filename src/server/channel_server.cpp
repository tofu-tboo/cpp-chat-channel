#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

#include "channel_server.h"
#include "../libs/util.h"

ChannelServer::ChannelServer() {
    task_runner.popf();
    task_runner.popf();
}
ChannelServer::~ChannelServer() {
    for (auto& [_, channel] : channels) {
        delete channel;
    }
}

#pragma region PROTECTED_FUNC
void ChannelServer::on_switch(const char* target, Json& root, const std::string& payload) {
    switch (hash(target))
    {
    // case hash("message"):
    // case hash("Message"):
    // case hash("MESSAGE"):
    //     break;
    case hash("join"):
    case hash("Join"):
    case hash("JOIN"):
        {
            ch_id_t channel_id;
            __UNPACK_JSON(root, "{s:I}", "channel_id", &channel_id) {
                if (channels.find(channel_id) == channels.end()) {
                    channels[channel_id] = new Channel();
                    LOG(_CG_ "Channel %u created." _EC_, channel_id);
                }
            } __UNPACK_FAIL {
                iERROR("Malformed JSON message, missing channel_id.");
            }
        }
        break;
    // case hash("leave"):
    // case hash("Leave"):
    // case hash("LEAVE"):
    //     {
    //         ch_id_t channel_id;
    //         __UNPACK_JSON(root, "{s:I}", "channel_id", &channel_id) {
    //             auto it = channels.find(channel_id);
    //             if (it != channels.end()) {
    //                 delete it->second;
    //                 channels.erase(it);
    //                 LOG(_CY_ "Channel %u deleted." _EC_, channel_id);
    //             }
    //         } __UNPACK_FAIL {
    //             iERROR("Malformed JSON message, missing channel_id.");
    //         }
    //     }
    //     break;
    // case hash("quit"):
    //     break;
    default:
        break;
    }
}
#pragma endregion