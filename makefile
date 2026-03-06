# =============================================================================
#  Configuration
# =============================================================================
# Compiler and tools
CXX      := g++
CXX_WIN  := x86_64-w64-mingw32-g++
PYTHON   := python3

# Directories
OUT_DIR := exe
OBJ_DIR := obj

# Flags
# -Isrc: Add src to include paths to allow #include "libs/..."
CXXFLAGS := -O2 -std=c++20 -Isrc
LDFLAGS  := -ljansson -lwebsockets

# =============================================================================
#  Source & Object File Definitions
# =============================================================================
# Automatically find all .cpp files
LIB_SRC           := $(wildcard src/libs/*.cpp)
SERVER_SRC        := $(wildcard src/server/*.cpp)
CLIENT_WIN_SRC    := src/client/client_win.cpp
CLIENT_LINUX_SRC  := src/client/client.cpp

# Separate main() files from common server sources
MAIN_SERVER_SRC     := src/server/server.cpp
SIMPLE_SERVER_SRC   := src/server/server_simple.cpp
COMMON_SERVER_SRC   := $(filter-out $(MAIN_SERVER_SRC) $(SIMPLE_SERVER_SRC), $(SERVER_SRC))

# Generate corresponding object file lists
LIB_OBJ             := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(LIB_SRC))
COMMON_SERVER_OBJ   := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(COMMON_SERVER_SRC))
MAIN_SERVER_OBJ     := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(MAIN_SERVER_SRC))
SIMPLE_SERVER_OBJ   := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SIMPLE_SERVER_SRC))
CLIENT_WIN_OBJ      := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(CLIENT_WIN_SRC))

# Define executables
SERVER_EXE        := $(OUT_DIR)/server

# =============================================================================
#  Build Rules
# =============================================================================
# Default goal: build the main server
.DEFAULT_GOAL := server

# Phony targets for commands that aren't files
.PHONY: all server server-simple client client-win debug debug-all debug-thread debug-address check clean ngrok dynamic_compile

# --- High-Level Targets ---
all: server server-simple client

server: dynamic_compile $(OUT_DIR) $(SERVER_EXE)

server-simple: dynamic_compile $(OUT_DIR) $(OUT_DIR)/server-simple

client: dynamic_compile $(OUT_DIR) $(OUT_DIR)/client

client-win: dynamic_compile $(OUT_DIR) $(OUT_DIR)/client.exe

# --- Linking Executables ---
# Main server executable
$(OUT_DIR)/server: $(MAIN_SERVER_OBJ) $(COMMON_SERVER_OBJ) $(LIB_OBJ) | dynamic_compile
	@echo "==> Linking $@..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Simple server executable
$(OUT_DIR)/server-simple: $(SIMPLE_SERVER_OBJ) $(COMMON_SERVER_OBJ) $(LIB_OBJ) | dynamic_compile
	@echo "==> Linking $@..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Linux client executable
$(OUT_DIR)/client: $(CLIENT_LINUX_SRC) | $(OUT_DIR)
	@echo "==> Building Linux client..."
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Windows client executable (cross-compile)
$(OUT_DIR)/client.exe: $(CLIENT_WIN_OBJ) $(OBJ_DIR)/libs/util.o
	@echo "==> Building Windows client $@..."
	$(CXX_WIN) $(CXXFLAGS) -o $@ $^ -lws2_32 -static

# --- Compilation & Utility Rules ---
# Pattern rule to compile any .cpp from src/ into an .o in obj/
$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(@D)
	@echo "CXX $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the dynamic compile script. This is a prerequisite for linking.
dynamic_compile:
	@echo "==> Running dynamic compile script"
	$(PYTHON) src/dynamic_compile/dynamic_compile.py

# Create output directory if it doesn't exist
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# --- Utility Commands ---
# Debug build targets
# These add debug flags and then depend on the 'all' or 'server' target.
debug: CXXFLAGS += -g -DDEBUG
debug: server

debug-all: CXXFLAGS += -g -DDEBUG
debug-all: all

debug-thread: CXXFLAGS += -g -DDEBUG -fsanitize=thread
debug-thread: LDFLAGS += -fsanitize=thread
debug-thread: server

debug-address: CXXFLAGS += -g -DDEBUG -fsanitize=address
debug-address: LDFLAGS += -fsanitize=address
debug-address: server

# Run valgrind for memory checking on the main server
check: debug
	@echo "==> Running valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all $(SERVER_EXE)

clean:
	@echo "==> Cleaning up..."
	rm -rf $(OUT_DIR) $(OBJ_DIR)

ngrok:
	ngrok tcp 4800