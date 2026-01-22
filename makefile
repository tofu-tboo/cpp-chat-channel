PACKAGES = -ljansson

.PHONY: all client server clean

all: libs client server

client: src/client/client.cpp
	g++ -o $@ $^ $(PACKAGES)

server: src/server/server.cpp src/server/server_base.cpp src/server/channel_server.cpp src/server/chat_server.cpp src/server/channel.cpp src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/communication.cpp
	g++ -o $@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/task_runner.tpp src/libs/communication.cpp
	g++ -c $< -o $@ $(PACKAGES)

clean:
	rm -f client server libs *.o
