PACKAGES = -ljansson

.PHONY: all client server libs clean

all: libs client server

client: src/client/client.cpp
	g++ -o $@ $^ $(PACKAGES)

server: src/server/server.cpp src/server/server_base.cpp src/server/channel_server.cpp src/libs/json.cpp src/libs/util.cpp src/libs/connection_tracker.cpp
	g++ -o $@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/task_runner.tpp
	g++ -c $< -o $@ $(PACKAGES)

clean:
	rm -f client server libs *.o