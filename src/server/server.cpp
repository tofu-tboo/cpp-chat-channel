#include <stdio.h>

#include "../libs/util.h"
#include "channel_server.h"

int main() {
    ChannelServer server(32);

    server.proc();

    return 0;
}