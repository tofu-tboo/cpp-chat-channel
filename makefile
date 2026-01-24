PACKAGES = -ljansson
# 출력 디렉토리 변수 설정
OUT_DIR = exe

.PHONY: all client server clean libs

all: $(OUT_DIR) libs client server

# 폴더가 없을 경우 생성
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

client: src/client/client.cpp | $(OUT_DIR)
	g++ -o $(OUT_DIR)/$@ $^ $(PACKAGES)

server: src/server/server.cpp src/server/server_base.cpp src/server/channel_server.cpp src/server/chat_server.cpp src/server/channel.cpp src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/communication.cpp | $(OUT_DIR)
	g++ -o $(OUT_DIR)/$@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/task_runner.tpp src/libs/communication.cpp
	g++ -c $< -o $@ $(PACKAGES)

clean:
	rm -f $(OUT_DIR)/client $(OUT_DIR)/server *.o