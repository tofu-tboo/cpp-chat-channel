PACKAGES = -ljansson

all: client server libs

client: src/client/client.cpp
	g++ -o $@ $^ $(PACKAGES)

server: src/server/server.cpp src/server/server_class.cpp src/server/channel_server.cpp
	g++ -o $@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp
	g++ -c $< -o $@ $(PACKAGES)