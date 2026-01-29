#include <cstring>
#include <csignal>

#include "../libs/util.h"
#include "channel_server.h"

ChannelServer* g_server = nullptr;

void signal_handler(int signum) {
    if (g_server) {
        LOG("Signal %d received. Stopping server...", signum);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // if one of argv's key is lobbyN or chN, parse the its value as max fd of ChannelServer
	int lobby_max_fd = 32, ch_max_fd = 32;
	char port[6] = "\0";
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "lobbyN=", 7) == 0) {
			lobby_max_fd = atoi(argv[i] + 7);
		} else if (strncmp(argv[i], "chN=", 4) == 0) {
			ch_max_fd = atoi(argv[i] + 4);
		} else if (strncmp(argv[i], "port=", 5) == 0) {
			strncpy(port, argv[i] + 5, 5);
			port[5] = 0;
		}
	}

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	ChannelServer server(port, lobby_max_fd, ch_max_fd);
    g_server = &server;

    server.proc();

    g_server = nullptr;
    return 0;
}