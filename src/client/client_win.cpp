#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>

#include <cstring>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>


#include "../libs/util.h"

#pragma comment(lib, "Ws2_32.lib")

static std::string g_input_buffer;
static std::string g_user_name;
static HANDLE g_hConsoleIn = INVALID_HANDLE_VALUE;
static HANDLE g_hConsoleOut = INVALID_HANDLE_VALUE;
static DWORD g_origInMode = 0;
static DWORD g_origOutMode = 0;
static int g_channel_id = 0;

typedef uint64_t msec64;

static void cleanup() {
    if (g_hConsoleIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(g_hConsoleIn, g_origInMode);
    }
    if (g_hConsoleOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(g_hConsoleOut, g_origOutMode);
    }
    WSACleanup();
}

static int connect_tcp(const char* host, const char* port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed");
        return -1;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int status = getaddrinfo(host, port, &hints, &res);
    if (status != 0) {
        printf("getaddrinfo failed: %d", status);
        WSACleanup();
        return -1;
    }

    SOCKET fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) {
        printf("socket() failed: %d", WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return -1;
    }

    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("connect() failed: %d", WSAGetLastError());
        closesocket(fd);
        freeaddrinfo(res);
        WSACleanup();
        return -1;
    }

    freeaddrinfo(res);
    return (int)fd;
}

static bool send_frame(int fd, const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    char header[5];
    snprintf(header, sizeof(header), "%04x", len);

    int n = send((SOCKET)fd, header, 4, 0);
    if (n != 4) return false;

    size_t sent = 0;
    while (sent < payload.size()) {
        n = send((SOCKET)fd, payload.data() + sent, (int)(payload.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static msec64 now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static void enableRawMode() {
    g_hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    g_hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (g_hConsoleIn == INVALID_HANDLE_VALUE || g_hConsoleOut == INVALID_HANDLE_VALUE) return;

    GetConsoleMode(g_hConsoleIn, &g_origInMode);
    GetConsoleMode(g_hConsoleOut, &g_origOutMode);

    // 입력 에코 및 라인 모드 비활성화
    DWORD newInMode = g_origInMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    SetConsoleMode(g_hConsoleIn, newInMode);

    // 가상 터미널 처리 활성화 (ANSI 색상 코드 지원)
    DWORD newOutMode = g_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(g_hConsoleOut, newOutMode);
    
    atexit(cleanup);
}

static int get_term_width() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hConsoleOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
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

// 간단한 JSON 문자열 추출 함수
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t start = json.find(search);
    if (start == std::string::npos) return "";
    start += search.length();
    
    // 공백 건너뛰기
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
    
    if (start >= json.length() || json[start] != '"') return ""; // 문자열이 아님
    start++; // 시작 따옴표
    
    std::string res;
    bool escaped = false;
    for (size_t i = start; i < json.length(); ++i) {
        char c = json[i];
        if (escaped) {
            res += c;
            escaped = false;
        } else {
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                return res;
            } else {
                res += c;
            }
        }
    }
    return res;
}

// 간단한 JSON 정수 추출 함수
static int extract_json_int(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t start = json.find(search);
    if (start == std::string::npos) return 0;
    start += search.length();
    
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
    
    size_t end = start;
    while (end < json.length() && ((json[end] >= '0' && json[end] <= '9') || json[end] == '-')) end++;
    
    if (start == end) return 0;
    return std::stoi(json.substr(start, end - start));
}

static void print_line(const std::string& type, const std::string& user, const std::string& event, const std::string& msg_text, int ch_id) {
    if (type.empty()) return;

    int width = get_term_width();
    if (type == "system") {
        if (!event.empty() && !user.empty()) {
            if (g_user_name == user && (event == "join" || event == "rejoin")) {
                if (ch_id > 0) g_channel_id = ch_id;
            }
            std::string msg = "[System] " + user + " " + event;
            int pad = (width - (int)msg.length()) / 2;
            if (pad < 0) pad = 0;
            std::cout << std::string(pad, ' ') << _CY_ << msg << _EC_ << "\r\n";
        }
    } else if (type == "user") {
        if (!user.empty() && !event.empty()) { // event 필드에 텍스트가 들어옴
            if (g_user_name == user) {
                std::string msg = event + " (Me)";
                int pad = width - (int)msg.length();
                if (pad < 0) pad = 0;
                std::cout << std::string(pad, ' ') << _CB_ << msg << _EC_ << "\r\n";
            } else {
                std::cout << user << ": " << event << "\r\n";
            }
        }
    } else if (type == "error") {
        if (!msg_text.empty()) {
            std::string msg = "[Error] " + msg_text;
            int pad = (width - (int)msg.length()) / 2;
            if (pad < 0) pad = 0;
            std::cout << std::string(pad, ' ') << _CR_ << msg << _EC_ << "\r\n";
        }
    }
}

static void parse_and_print(const std::string& json_str) {
    std::string type = extract_json_string(json_str, "type");
    std::string user = extract_json_string(json_str, "user_name");
    std::string event = extract_json_string(json_str, "event");
    std::string message = extract_json_string(json_str, "message");
    int ch_id = extract_json_int(json_str, "channel_id");
    print_line(type, user, event, message, ch_id);
}

static void display_message(const std::string& payload) {
    std::cout << "\r\033[K"; // Clear input line

    // 배열인지 확인 (단순 체크)
    bool is_array = false;
    size_t start = 0;
    while(start < payload.size() && isspace(payload[start])) start++;
    if (start < payload.size() && payload[start] == '[') is_array = true;

    if (is_array) {
        size_t pos = start + 1;
        while (pos < payload.size()) {
            size_t obj_start = payload.find('{', pos);
            if (obj_start == std::string::npos) break;
            size_t obj_end = payload.find('}', obj_start);
            if (obj_end == std::string::npos) break;
            
            std::string obj_str = payload.substr(obj_start, obj_end - obj_start + 1);
            parse_and_print(obj_str);
            
            pos = obj_end + 1;
        }
    } else {
        parse_and_print(payload);
    }

    refresh_line();
}

// JSON 이스케이프 처리
static std::string json_escape(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\b') res += "\\b";
        else if (c == '\f') res += "\\f";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

static bool recv_frames(int fd, std::string& acc) {
    char buf[4096];
    int n = recv((SOCKET)fd, buf, sizeof(buf), 0);
    if (n == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return true;
        return false;
    } else if (n == 0) return false; // Connection closed
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

            // getaddrinfo는 호스트명만 인식하므로 경로는 제거해야 함 (HTTP 요청 시에만 필요)
            p = url.find('/');
            if (p != std::string::npos) url = url.substr(0, p);

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
    std::string join_msg = "{\"type\":\"join\", \"channel_id\":1, \"user_name\":\"" + 
                           json_escape(g_user_name) + "\", \"timestamp\":" + 
                           std::to_string(now_ms()) + "}";
    send_frame(fd, join_msg);

    enableRawMode();
    cls();
    refresh_line();

    // 소켓을 논블로킹 모드로 설정 (Polling을 위해)
    u_long mode = 1;
    ioctlsocket((SOCKET)fd, FIONBIO, &mode);

    std::string acc;
    bool running = true;

    while (running) {
        // 1. 소켓 확인 (select 사용)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET((SOCKET)fd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms 타임아웃

        int ret = select(0, &readfds, nullptr, nullptr, &tv);
        if (ret == SOCKET_ERROR) {
            break;
        }
        if (ret > 0 && FD_ISSET((SOCKET)fd, &readfds)) {
            if (!recv_frames(fd, acc)) running = false;
        }

        // 2. 콘솔 입력 확인 (_kbhit 사용)
        if (_kbhit()) {
            int c = _getch();
            if (c == 0 || c == 0xE0) {
                _getch(); // 특수 키(화살표 등) 무시
            } else {
                if (c == '\r') c = '\n'; // Enter 키 변환

                if (c == '\n') {
                    if (!g_input_buffer.empty()) {
                        if (g_input_buffer.rfind("/join ", 0) == 0) {
                            try {
                                std::string ch_id_str = g_input_buffer.substr(6);
                                int channel_id = std::stoi(ch_id_str);
                                
                                std::string req = "{\"type\":\"join\", \"channel_id\":" + std::to_string(channel_id) + 
                                                  ", \"timestamp\":" + std::to_string(now_ms()) + "}";
                                send_frame(fd, req);
                            } catch (const std::exception&) {
                                // 잘못된 명령어 무시
                            }
                        } else {
                            std::string req = "{\"type\":\"message\", \"text\":\"" + json_escape(g_input_buffer) + 
                                              "\", \"timestamp\":" + std::to_string(now_ms()) + "}";
                            send_frame(fd, req);
                        }
                        g_input_buffer.clear();
                    }
                } else if (c == '\b' || c == 127) { // Backspace
                    if (!g_input_buffer.empty()) g_input_buffer.pop_back();
                } else if (c >= 32 && c <= 126) { // 출력 가능한 문자
                    g_input_buffer += (char)c;
                }
                refresh_line();
            }
        }
    }

    closesocket((SOCKET)fd);
    return 0;
}