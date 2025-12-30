#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <algorithm>

#include "server_base.h"
#include "../libs/util.h"
#include "../libs/json.h"

fd_t ServerBase::fd = -1;

ServerBase::ServerBase(int max_fd) {
    branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    this->max_fd = max_fd;
    efd = FD_ERR;
        
    if (fd == FD_ERR && !set_network()) {
        iERROR("Failed to set network.");
        if (fd != FD_ERR)
            close(fd);
    }

    LOG(_CG_ "Server initialized on port 4800." _EC_);

    // init epoll
    if ((efd = epoll_create1(0)) == FD_ERR) {
        iERROR("Failed to create epoll instance.");
        return;
    }
    pollev ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (fd != FD_ERR && FAILED(epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev))) {
        iERROR("Failed to add listen fd to epoll.");
    }
}

ServerBase::~ServerBase() {
    if (efd != FD_ERR) { // cleanup epoll listeners
        for (fd_t client_fd : listeners) {
            epoll_ctl(efd, EPOLL_CTL_DEL, client_fd, nullptr);
            if (client_fd != FD_ERR) {
                close(client_fd);
            }
        }
        close(efd);
        efd = FD_ERR;
    } else { // cleanup only listeners
        for (fd_t client_fd : listeners) {
            if (client_fd != FD_ERR) {
                close(client_fd);
            }
        }
    }
    listeners.clear();

    if (fd != FD_ERR) { // close listening socket
        close(fd);
        fd = FD_ERR;
    }

    recv_buf.clear();
    mq.clear();
    next_deletion.clear();
}

void ServerBase::proc(const msec to) {
    pollev events[max_fd];

    while (1) {
        next_deletion.clear();
        cur_msgs.clear();

        int n;
        if (FAILED(n = epoll_wait(efd, events, max_fd, to))) {
            iERROR("Failed to epoll_wait in proc.");
            break;
        }

        for (int i = 0; i < n; i++)
            handle_events(events[i]);

        resolve_timestamps();

        // TOOD: smart pointer
        json cur_window = json_array();
        char* dumped;
        for (const auto& [timestamp, msg] : cur_msgs)
        {
            json_array_append_new(cur_window, json_string(msg.c_str()));
        }
        dumped = json_dumps(cur_window, 0);
        if (dumped) {
            broadcast(dumped);
            free(dumped);   
        }
        free_json(cur_window);

        for (fd_t fd : next_deletion)
            delete_listener(fd);
    }
}

#pragma region PRIVATE_FUNC
bool ServerBase::set_network() {
    if (fd != FD_ERR) {
        iERROR("Sever descriptor is already assigned.");
        return false;
    }

    sAddrInfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "4800", &hints, &res);
    if (status != 0) {
        iERROR("The getaddrinfo() is not resolved.");
        return false;
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == FD_ERR) {
        iERROR("Failed to get socket fd.");
        return false;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(fd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        iERROR("Failed to bind server.");
        return false;
    }

    status = listen(fd, 5);
    if (status == -1) {
        iERROR("Failed to set listen().");
        return false;
    }

    freeaddrinfo(res);

    return true;
}
#pragma endregion

#pragma region PROTECTED_FUNC
void ServerBase::handle_events(const pollev event) {
    fd_t fd = event.data.fd;
    uint32_t evs = event.events;

    if (fd == ServerBase::fd) {
        fd_t client = accept(fd, nullptr, nullptr);
        if (client == FD_ERR) {
            iERROR("Failed to accept new connection.");
        } else {
            LOG("Accepted new connection: fd %d", client);
            if (!add_listener(client)) {
                close(client);
            }
        }
    } else if (evs & (EPOLLHUP | EPOLLERR)) {
        next_deletion.push_back(fd);
    } else if (evs & EPOLLIN) {
        if (!recv_frame(fd)) {
            next_deletion.push_back(fd);
        }
    }
    // 필요하면 EPOLLOUT도 처리
}

bool ServerBase::add_listener(const int fd) {
    if (fd == FD_ERR) {
        iERROR("Invalid listener fd.");
        return false;
    } else if (efd == FD_ERR) {
        iERROR("Epoll instance is not initialized.");
        return false;
    } else if (fd == ServerBase::fd) {
        iERROR("Listening socket cannot be re-added as a client listener.");
        return false;
    } else if (listeners.find(fd) != listeners.end()) {
        return true; // already tracked
    }

    pollev ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (FAILED(epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev))) {
        iERROR("Failed to add fd %d to epoll.", fd);
        return false;
    }

    listeners.insert(fd);
    return true;
}
bool ServerBase::delete_listener(const int fd) {
    if (fd == FD_ERR) {
        iERROR("Invalid listener fd.");
        return false;
    } else if (efd == FD_ERR) {
        iERROR("Epoll instance is not initialized.");
        return false;
    } else if (fd == ServerBase::fd) {
        iERROR("Listening socket cannot be deleted.");
        return false;
    } else if (listeners.find(fd) == listeners.end()) {
        return false; // not tracked
    }

    if (FAILED(epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr))) {
        iERROR("Failed to remove fd %d from epoll.", fd);
    }

    if (fd != FD_ERR) {
        close(fd);
    }

    listeners.erase(fd);
    recv_buf.erase(fd);
    return true;
}

bool ServerBase::recv_frame(const int fd) {
    // NEEDS: attach 4-byte length header to each frame
    //{"type":"message","channel_id":"c1","user_id":"u1","data":{"text":"message"},"timestamp":1234567890}

    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        return false; // closed or error
    }

    std::string& acc = recv_buf[fd];
    acc.append(buf, n);

    while (acc.size() >= 4) {
        uint32_t len = ((uint8_t)acc[0] << 24) | ((uint8_t)acc[1] << 16) | ((uint8_t)acc[2] << 8) | (uint8_t)acc[3];
        if (len > MAX_FRAME_SIZE) {
            iERROR("Frame too large from fd %d", fd);
            return false;
        } else if (len == 0) {
            iERROR("Empty frame from fd %d", fd);
            acc.erase(0, 4);
            continue;
        } else if (acc.size() < 4 + len) {
            break; // wait for full frame
        }
        
        std::string payload = acc.substr(4, len);

        resolve_payload(payload);
        
        acc.erase(0, 4 + len);
    }

    return true;
}

bool ServerBase::send_frame(const int fd, const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    if (len == 0) {
        return true;
    }

    unsigned char header[4];
    header[0] = (len >> 24) & 0xFF;
    header[1] = (len >> 16) & 0xFF;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;

    ssize_t sent = send(fd, header, 4, 0);
    if (sent != 4) {
        return false;
    }

    size_t total = 0;
    while (total < payload.size()) {
        ssize_t n = send(fd, payload.data() + total, payload.size() - total, 0);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

void ServerBase::broadcast(const std::string& payload) {
    for (const fd_t& fd : listeners) {
        if (!send_frame(fd, payload)) {
            next_deletion.push_back(fd);
        }
    }
}

void ServerBase::resolve_timestamps() {
    for (const std::string& msg : mq) {
        json_error_t err;
        json root;
        if ((root = json_loads(msg.c_str(), 0, &err)) == nullptr) {
            iERROR("Failed to parse JSON: %s", err.text);
            continue;
        }
        
        msec64 timestamp;
        __UNPACK_JSON(root, "{s:I}", "timestamp", &timestamp) {
            cur_msgs.emplace(timestamp, msg);
        } __UNPACK_FAIL {
            iERROR("Malformed JSON message, missing timestamp.");
        }
        __FREE_JSON(root);
    }
    mq.clear();
}

void ServerBase::resolve_payload(const std::string& payload) {
    json_error_t err;
    json root;
    if ((root = json_loads(payload.c_str(), 0, &err)) == nullptr) {
        iERROR("Failed to parse JSON: %s", err.text);
        return;
    }
    
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        switch (hash(type))
        {
        case hash("message"):
            mq.push_back(payload);
            break;
        
        default:
            break;
        }
    } __UNPACK_FAIL {
        iERROR("Malformed JSON message, missing type.");
    }
    __FREE_JSON(root);
}
#pragma endregion
