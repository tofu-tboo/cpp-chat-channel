#include <cstring>
#include <csignal>
#include <cstdlib>

#include "../libs/util.h"
#include "server_factory.h"
#include "chat_server.h"
#include "../libs/lws_service.h"
#include "channel_factory.h"
#include "../libs/json_translator.h"
#include "../libs/signal_handler_thread.h"

int main(int argc, char* argv[]) {
	SignalHandler sig_handler;
	sig_handler.setup_mask();

    // if one of argv's key is lobbyN or chN, parse the its value as max fd of ChannelServer
	int lobby_max_fd = 32;
	const char* env_p = std::getenv("PORT");
	int port = env_p != nullptr ? atoi(env_p) : 4800;
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "lobbyN=", 7) == 0) {
			lobby_max_fd = atoi(argv[i] + 7);
		} else if (strncmp(argv[i], "port=", 5) == 0) {
			port = atoi(argv[i] + 5);
		}
	}

	auto service = std::make_shared<LwsService<User>>(port, 8);
	auto server = ServerFactory::create<User, ChatServer>(service, lobby_max_fd);
	printf("LWS Version: %s\n", lws_get_library_version());

	sig_handler.start([server](int sig) {
        LOG("\n[Signal] Received signal %d. Shutting down...\n", sig);
        server->stop();
    });

    server->proc();
    sig_handler.stop();
    
    delete server;
    server = nullptr;

    return 0;
}