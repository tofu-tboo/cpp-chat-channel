#include <cstring>
#include <csignal>
#include <cstdlib>

#include "../libs/util.h"
#include "server_factory.h"
#include "channel_server.h"
#include "../libs/network_service.h"

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
	const char* env_p = std::getenv("PORT");
	int port = env_p != nullptr ? atoi(env_p) : 4800;
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "lobbyN=", 7) == 0) {
			lobby_max_fd = atoi(argv[i] + 7);
		} else if (strncmp(argv[i], "chN=", 4) == 0) {
			ch_max_fd = atoi(argv[i] + 4);
		} else if (strncmp(argv[i], "port=", 5) == 0) {
			port = atoi(argv[i] + 5);
		}
	}

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	NetworkService<User> service(port);
	g_server = ServerFactory::create<ChannelServer>(&service, lobby_max_fd, ch_max_fd);

    g_server->proc();

    g_server = nullptr;
    return 0;
}