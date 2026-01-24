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
static int g_channel_id = 0;

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

static void gotoxy(int x, int y) {
    std::cout << "\033[" << y << ";" << x << "H" << std::flush;
}

static void cls() {
    std::cout << "\033[2J\033[1;1H" << std::flush;
}

static void refresh_line() {
    std::cout << "\r\033[K" << _EC_ << "[Ch " << g_channel_id << "] > " << g_input_buffer << std::flush;
}

static void print_line(json_t* obj) {
    const char* type = json_string_value(json_object_get(obj, "type"));
    if (!type) return;

    int width = get_term_width();
    if (strcmp(type, "system") == 0) {
        const char* event = json_string_value(json_object_get(obj, "event"));
        const char* user = json_string_value(json_object_get(obj, "user_name"));
        if (event && user) {
            if (g_user_name == user && (strcmp(event, "join") == 0 || strcmp(event, "rejoin") == 0)) {
                json_t* ch_id_json = json_object_get(obj, "channel_id");
                if (ch_id_json && json_is_integer(ch_id_json))
                    g_channel_id = (int)json_integer_value(ch_id_json);
            }
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
}

static void display_message(const std::string& payload) {
    json_error_t err;
    json_t* obj = json_loadb(payload.data(), payload.size(), 0, &err);
    if (!obj) return;

    std::cout << "\r\033[K"; // Clear input line

    if (json_is_array(obj)) {
        size_t index;
        json_t *value;
        json_array_foreach(obj, index, value) {
            if (json_is_object(value)) {
                print_line(value);
            }
        }
    } else if (json_is_object(obj)) {
        print_line(obj);
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
        if (strncmp(argv[i], "host=", 5) == 0) {
            std::string url = argv[i] + 5;
            std::string default_port = "4800";

            size_t p = url.find("://");
            if (p != std::string::npos) {
                std::string scheme = url.substr(0, p);
                if (scheme == "https") default_port = "443";
                else if (scheme == "http") default_port = "80";
                url = url.substr(p + 3);
            }

            p = url.find(':');
            if (p != std::string::npos) {
                host = url.substr(0, p);
                port = url.substr(p + 1);
            } else {
                host = url;
                port = default_port;
            }
        }
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
	std::cout << "You can change the channel by a command \"/join <number>\"" << std::endl;

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

    std::cout << "Waiting for server response..." << std::endl;

    std::string acc;
    bool joined = false;
    char buf[4096];

    // Join 응답 대기 루프 (Blocking)
    while (!joined) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::cerr << "Connection closed or failed during handshake." << std::endl;
            close(fd);
            return 1;
        }
        acc.append(buf, n);

        while (acc.size() >= 4) {
            uint32_t len = 0;
            try {
                len = std::stoul(acc.substr(0, 4), nullptr, 16);
            } catch (...) { return 1; }

            if (acc.size() < 4 + len) break;
            std::string payload = acc.substr(4, len);
            
            json_error_t err;
            json_t* obj = json_loadb(payload.data(), payload.size(), 0, &err);
            if (obj) {
                if (json_is_array(obj)) {
                    size_t index;
                    json_t *value;
                    json_array_foreach(obj, index, value) {
                        const char* type = json_string_value(json_object_get(value, "type"));
                        if (type && strcmp(type, "system") == 0) {
                            const char* event = json_string_value(json_object_get(value, "event"));
                            if (event && (strcmp(event, "join") == 0 || strcmp(event, "rejoin") == 0)) {
                                json_t* ch_id_json = json_object_get(value, "channel_id");
                                if (ch_id_json && json_is_integer(ch_id_json))
                                    g_channel_id = (int)json_integer_value(ch_id_json);
                                joined = true;
                            }
                        } else if (type && strcmp(type, "error") == 0) {
                            std::cerr << "Join failed: " << json_string_value(json_object_get(value, "message")) << std::endl;
                            return 1;
                        }
                    }
                } else {
                    const char* type = json_string_value(json_object_get(obj, "type"));
                    if (type && strcmp(type, "system") == 0) {
                        const char* event = json_string_value(json_object_get(obj, "event"));
                        if (event && (strcmp(event, "join") == 0 || strcmp(event, "rejoin") == 0)) {
                            json_t* ch_id_json = json_object_get(obj, "channel_id");
                            if (ch_id_json && json_is_integer(ch_id_json))
                                g_channel_id = (int)json_integer_value(ch_id_json);
                            joined = true;
                        }
                    } else if (type && strcmp(type, "error") == 0) {
                        std::cerr << "Join failed: " << json_string_value(json_object_get(obj, "message")) << std::endl;
                        return 1;
                    }
                }
                json_decref(obj);
            }
            acc.erase(0, 4 + len);
        }
    }

    enableRawMode();
    cls();
    refresh_line();

    struct pollfd fds[2];
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

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
                        if (g_input_buffer.rfind("/join ", 0) == 0) {
                            try {
                                std::string ch_id_str = g_input_buffer.substr(6);
                                int channel_id = std::stoi(ch_id_str);
                                
                                json_t* join_obj = json_pack("{s:s, s:I, s:I}", 
                                    "type", "join", 
                                    "channel_id", (json_int_t)channel_id, 
                                    "timestamp", (json_int_t)now_ms());
                                char* join_dump = json_dumps(join_obj, JSON_COMPACT);
                                send_frame(fd, std::string(join_dump));
                                free(join_dump);
                                json_decref(join_obj);
                            } catch (const std::exception&) {
                                // Invalid command, do nothing
                            }
                        } else {
                            json_t* msg_obj = json_pack("{s:s, s:s, s:I}", "type", "message", "text", g_input_buffer.c_str(), "timestamp", (json_int_t)now_ms());
                            char* msg_dump = json_dumps(msg_obj, JSON_COMPACT);
                            send_frame(fd, std::string(msg_dump));
                            free(msg_dump);
                            json_decref(msg_obj);
                        }
                        g_input_buffer.clear();
                    }
                } else if (c == 127 || c == '\b') { // Backspace
                    if (!g_input_buffer.empty()) g_input_buffer.pop_back();
                } else if (c >= 32 && c <= 126) { // Printable ASCII
                    g_input_buffer += c;
                }
                refresh_line();
            }
        }
    }

    if (fd != -1) close(fd);
    return 0;
}
