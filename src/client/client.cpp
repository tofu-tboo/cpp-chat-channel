#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cerrno>

#include <cstring>
#include <chrono>
#include <iostream>
#include <string>

#include <jansson.h>

#include "../libs/util.h"

static std::string g_input_buffer;
static std::string g_user_name;
static struct termios g_orig_termios;

typedef uint64_t msec64;


static int connect_tcp(const char* host, const char* port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, port, &hints, &res);
    if (status != 0 || !res) {
        ERROR("getaddrinfo failed: %s", gai_strerror(status));
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        ERROR("socket() failed");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        ERROR("connect() failed");
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return fd;
}

static bool send_frame(int fd, const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    char header[5];
    snprintf(header, sizeof(header), "%04x", len);

    ssize_t n = send(fd, header, 4, 0);
    if (n != 4) return false;

    size_t sent = 0;
    while (sent < payload.size()) {
        n = send(fd, payload.data() + sent, payload.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static msec64 now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

static void enableRawMode() {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    atexit(disableRawMode);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;  // 최소 1바이트 입력 시 리턴
    raw.c_cc[VTIME] = 0; // 타임아웃 없음
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int get_term_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) return 80;
    return w.ws_col;
}

static void refresh_line() {
    std::cout << "\r\033[K" << "> " << g_input_buffer << std::flush;
}

static void display_message(const std::string& payload) {
    json_error_t err;
    json_t* obj = json_loadb(payload.data(), payload.size(), 0, &err);
    if (!obj || !json_is_object(obj)) {
        if (obj) json_decref(obj);
        return;
    }

    const char* type = json_string_value(json_object_get(obj, "type"));
    if (!type) {
        json_decref(obj);
        return;
    }

    int width = get_term_width();
    std::cout << "\r\033[K"; // Clear input line

    if (strcmp(type, "system") == 0) {
        const char* event = json_string_value(json_object_get(obj, "event"));
        const char* user = json_string_value(json_object_get(obj, "user_name"));
        if (event && user) {
            std::string msg = std::string("[System] ") + user + " " + event;
            int pad = (width - (int)msg.length()) / 2;
            if (pad < 0) pad = 0;
            std::cout << std::string(pad, ' ') << _CY_ << msg << _EC_ << "\r\n";
        }
    } else if (strcmp(type, "user") == 0) {
        const char* user = json_string_value(json_object_get(obj, "user_name"));
        const char* text = json_string_value(json_object_get(obj, "event"));
        if (user && text) {
            if (g_user_name == user) {
                std::string msg = std::string(text) + " (Me)";
                int pad = width - (int)msg.length();
                if (pad < 0) pad = 0;
                std::cout << std::string(pad, ' ') << _CB_ << msg << _EC_ << "\r\n";
            } else {
                std::cout << user << ": " << text << "\r\n";
            }
        }
    } else if (strcmp(type, "error") == 0) {
        const char* msg_text = json_string_value(json_object_get(obj, "message"));
        if (msg_text) {
            std::string msg = std::string("[Error] ") + msg_text;
            int pad = (width - (int)msg.length()) / 2;
            if (pad < 0) pad = 0;
            std::cout << std::string(pad, ' ') << _CR_ << msg << _EC_ << "\r\n";
        }
    }

    json_decref(obj);
    refresh_line();
}

static bool recv_frames(int fd, std::string& acc) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n < 0) {
        if (errno == EINTR) return true; // 시그널 인터럽트는 무시하고 계속 진행
        return false;
    } else if (n == 0) return false; // 연결 종료
    acc.append(buf, n);

    while (acc.size() >= 4) {
        uint32_t len = 0;
        try {
            len = std::stoul(acc.substr(0, 4), nullptr, 16);
        } catch (...) { return false; }

        if (acc.size() < 4 + len) break;
        std::string payload = acc.substr(4, len);
        display_message(payload);
        acc.erase(0, 4 + len);
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "4800";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "host=", 5) == 0) host = argv[i] + 5;
        else if (strncmp(argv[i], "port=", 5) == 0) port = argv[i] + 5;
    }

    std::cout << "Enter user_name: ";
    std::getline(std::cin, g_user_name);
    if (g_user_name.empty()) g_user_name = "guest";

    int fd = connect_tcp(host.c_str(), port.c_str());
    if (fd == -1) {
        return 1;
    }
    std::cout << _CG_ "Connected to " << host << ":" << port << _EC_ << std::endl;

    // Send Join
    json_t* join_obj = json_pack("{s:s, s:I, s:s, s:I}", 
        "type", "join", 
        "channel_id", (json_int_t)1, 
        "user_name", g_user_name.c_str(), 
        "timestamp", (json_int_t)now_ms());
    char* join_dump = json_dumps(join_obj, JSON_COMPACT);
    send_frame(fd, std::string(join_dump));
    free(join_dump);
    json_decref(join_obj);

    enableRawMode();
    refresh_line();

    struct pollfd fds[2];
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    std::string acc;
    bool running = true;

    while (running) {
        if (poll(fds, 2, -1) == -1) break;

        if (fds[0].revents & POLLIN) {
            if (!recv_frames(fd, acc)) running = false;
        }
        if (fds[1].revents & POLLIN) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == '\n' || c == '\r') {
                    if (!g_input_buffer.empty()) {
                        json_t* msg_obj = json_pack("{s:s, s:s, s:I}", "type", "message", "text", g_input_buffer.c_str(), "timestamp", (json_int_t)now_ms());
                        char* msg_dump = json_dumps(msg_obj, JSON_COMPACT);
                        send_frame(fd, std::string(msg_dump));
                        free(msg_dump);
                        json_decref(msg_obj);
                        g_input_buffer.clear();
                    }
                } else if (c == 127 || c == '\b') {
                    if (!g_input_buffer.empty()) g_input_buffer.pop_back();
                } else {
                    g_input_buffer += c;
                }
                refresh_line();
            }
        }
    }

    if (fd != -1) close(fd);
    return 0;
}
