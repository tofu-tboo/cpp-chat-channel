PACKAGES = -ljansson
OUT_DIR = exe
# 기본 컴파일 옵션 (여기에 -g를 넣다 뺐다 할 수 있음)
CXXFLAGS = -O2 

.PHONY: all client server clean libs debug

# debug 타겟을 실행하면 CXXFLAGS에 -g를 추가하고 all을 실행
debug: CXXFLAGS = -g -DDEBUG
debug: all

all: $(OUT_DIR) libs client server

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

client: src/client/client.cpp | $(OUT_DIR)
	g++ $(CXXFLAGS) -o $(OUT_DIR)/$@ $^ $(PACKAGES)

server: src/server/server.cpp src/server/server_base.cpp src/server/channel_server.cpp src/server/chat_server.cpp src/server/channel.cpp src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/communication.cpp | $(OUT_DIR)
	g++ $(CXXFLAGS) -o $(OUT_DIR)/$@ $^ $(PACKAGES)

libs: src/libs/util.cpp src/libs/json.cpp src/libs/connection_tracker.cpp src/libs/task_runner.tpp src/libs/communication.cpp
	g++ -c $< -o $@ $(PACKAGES)

clean:
	rm -f $(OUT_DIR)/client $(OUT_DIR)/server *.o

check: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(OUT_DIR)/server