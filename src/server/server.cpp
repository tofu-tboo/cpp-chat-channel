#include <cstring>

#include "../libs/util.h"
#include "channel_server.h"

int main(int argc, char* argv[]) {
    // if one of argv's key is lobbyN or chN, parse the its value as max fd of ChannelServer
	int lobby_max_fd = 32, ch_max_fd = 32;
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "lobbyN=", 7) == 0) {
			lobby_max_fd = atoi(argv[i] + 7);
		} else if (strncmp(argv[i], "chN=", 4) == 0) {
			ch_max_fd = atoi(argv[i] + 4);
		}
	}

	ChannelServer server(lobby_max_fd, ch_max_fd);

    server.proc();

    return 0;
}