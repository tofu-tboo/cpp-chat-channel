PACKAGES = -ljansson
OUT_DIR = exe
CXXFLAGS = -O2 
# 윈도우 크로스 컴파일러 (Linux/WSL에서 Windows용 빌드 시 필요. 예: sudo apt install mingw-w64)
CXX_WIN = x86_64-w64-mingw32-g++

.PHONY: all client server clean libs debug

debug: CXXFLAGS = -g -DDEBUG
debug: all

all: $(OUT_DIR) libs client server

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

client: src/client/client.cpp | $(OUT_DIR)
	g++ $(CXXFLAGS) -o $(OUT_DIR)/$@ $^ $(PACKAGES)

# 윈도우용 클라이언트 빌드 (Linux/WSL에서 크로스 컴파일)
client_win: src/client/client_win.cpp src/libs/util.cpp | $(OUT_DIR)
	$(CXX_WIN) $(CXXFLAGS) -o $(OUT_DIR)/client.exe $^ -lws2_32 -static

server: src/server/server.cpp src/server/server_base.cpp src/server/typed_frame_server.cpp src/server/channel_server.cpp src/server/chat_server.cpp src/server/channel.cpp src/server/user_manager.cpp src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/communication.cpp | $(OUT_DIR)
	g++ $(CXXFLAGS) -o $(OUT_DIR)/$@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/task_runner.tpp src/libs/communication.cpp
	g++ -c $< -o $@ $(PACKAGES)

clean:
	rm -f $(OUT_DIR)/client $(OUT_DIR)/server *.o

check: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(OUT_DIR)/server

ngrok:
	ngrok tcp 4800