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

fd_t ServerBase::fd = -1;

ServerBase::ServerBase(const int max_fd): con_tracker(nullptr) {
    try {
        branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

        if (fd == FD_ERR)
            set_network();

        LOG(_CG_ "Server initialized on port 4800." _EC_);

        con_tracker = new ConnectionTracker(fd, max_fd);
        if (!con_tracker)
            throw std::runtime_error("Failed to allocate Connection Tracker.");
        con_tracker->init();

        task_runner.pushb([this]() {
            resolve_timestamps();
        });
        task_runner.pushb([this]() {
            resolve_broadcast();
        });
        task_runner.pushb([this]() {
            resolve_deletion();
        });
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

ServerBase::~ServerBase() {
    if (con_tracker)
        delete con_tracker;

    if (fd != FD_ERR) { // close listening socket
        close(fd);
        fd = FD_ERR;
    }

    rbuf.clear();
    mq.clear();
    next_deletion.clear();
}

void ServerBase::proc(const msec to) {
    try {
        while (1) {
            next_deletion.clear();
            cur_msgs.clear();

            con_tracker->polling(to);

            task_runner.push_oncef([this]() {
                auto events = con_tracker->get_ev();
                for (int i = 0; i < con_tracker->get_evcnt(); i++)
                    handle_events(events[i]);
            });

            task_runner.run();
        }
    } catch(const std::exception& e) {
        iERROR("%s", e.what());
    }
}

#pragma region PRIVATE_FUNC
void ServerBase::set_network() {
    if (fd != FD_ERR) {
        throw std::runtime_error("Sever descriptor is already assigned.");
    }

    sAddrInfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "4800", &hints, &res);
    if (status != 0) {
        throw std::runtime_error("The getaddrinfo() is not resolved.");
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == FD_ERR) {
        throw std::runtime_error("Failed to get socket fd.");
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(fd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        throw std::runtime_error("Failed to bind server.");
    }

    status = listen(fd, 5);
    if (status == -1) {
        throw std::runtime_error("Failed to set listen().");
    }

    freeaddrinfo(res);
}

void ServerBase::handle_events(const pollev event) {
    fd_t fd = event.data.fd;
    uint32_t evs = event.events;

    if (fd == ServerBase::fd) {
        on_accept();
    } else if (evs & (EPOLLHUP | EPOLLERR)) {
        next_deletion.push_back(fd);
    } else if (evs & EPOLLIN) {
        try {
            recv_frame(fd);
        } catch (const std::exception& e) {
            iERROR("%s", e.what());
            next_deletion.push_back(fd);
        }
    }
    // 필요하면 EPOLLOUT도 처리
}
#pragma endregion

#pragma region PROTECTED_FUNC
void ServerBase::recv_frame(const int fd) {
    // NEEDS: attach 4-byte length header to each frame
    //{"type":"message","channel_id":"c1","user_id":"u1","data":{"text":"message"},"timestamp":1234567890}

    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        throw std::runtime_error("Minus frame.");
    }

    std::string& acc = rbuf[fd];
    acc.append(buf, n);

    while (acc.size() >= 4) {
        uint32_t len = ((uint8_t)acc[0] << 24) | ((uint8_t)acc[1] << 16) | ((uint8_t)acc[2] << 8) | (uint8_t)acc[3];
        if (len > MAX_FRAME_SIZE) {
            throw runtime_errorf("Frame too large from fd %d", fd);
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
}

void ServerBase::send_frame(const int fd, const std::string& payload) {
    uint32_t len = static_cast<uint32_t>(payload.size());
    if (len == 0) return;

    unsigned char header[4];
    header[0] = (len >> 24) & 0xFF;
    header[1] = (len >> 16) & 0xFF;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;

    ssize_t sent = send(fd, header, 4, 0);
    if (sent != 4) {
        throw std::runtime_error("Invalid header sent.");
    }

    size_t total = 0;
    while (total < payload.size()) {
        ssize_t n = send(fd, payload.data() + total, payload.size() - total, 0);
        if (n <= 0) {
            throw std::runtime_error("Minus frame.");
        }
        total += static_cast<size_t>(n);
    }
}

void ServerBase::broadcast(const std::string& payload) {
    for (const fd_t& fd : con_tracker->get_clients()) {
        try {
            send_frame(fd, payload);
        } catch(const std::exception&) {
            next_deletion.push_back(fd);
        }
    }
}

void ServerBase::resolve_timestamps() {
    for (const std::string& msg : mq) {
        json_error_t err;
        Json root(json_loads(msg.c_str(), 0, &err));
        if (root.get() == nullptr) {
            iERROR("Failed to parse JSON: %s", err.text);
            continue;
        }
        
        msec64 timestamp;
        __UNPACK_JSON(root, "{s:I}", "timestamp", &timestamp) {
            cur_msgs.emplace(timestamp, msg);
        } __UNPACK_FAIL {
            iERROR("Malformed JSON message, missing timestamp.");
        }
    }
    mq.clear();
}

void ServerBase::resolve_payload(const std::string& payload) {
    json_error_t err;
    Json root(json_loads(payload.c_str(), 0, &err));
    if (root.get() == nullptr) {
        iERROR("Failed to parse JSON: %s", err.text);
        return;
    }
    
    const char* type;
    __UNPACK_JSON(root, "{s:s}", "type", &type) {
        on_switch(type, root, payload);
    } __UNPACK_FAIL {
        iERROR("Malformed JSON message, missing type.");
    }
}

void ServerBase::resolve_broadcast() {
    Json cur_window(json_array());
    for (const auto& [timestamp, msg] : cur_msgs) {
        json_array_append_new(cur_window.get(), json_string(msg.c_str()));
    }
    CharDump dumped(json_dumps(cur_window.get(), 0));
    if (dumped) {
        broadcast(dumped.get());
    }
}

void ServerBase::resolve_deletion() {
    for (const fd_t fd : next_deletion) {
        con_tracker->delete_client(fd);
        close(fd);
        rbuf.erase(fd);
    }
}

void ServerBase::on_switch(const char* target, Json& root, const std::string& payload) {
    switch (hash(target))
    {
    case hash("message"):
        mq.push_back(payload);
        break;
    
    default:
        break;
    }
}

void ServerBase::on_accept() {
    fd_t client = accept(ServerBase::fd, nullptr, nullptr);
    if (client == FD_ERR) {
        iERROR("Failed to accept new connection.");
    } else {
        LOG("Accepted new connection: fd %d", client);
        try {
            con_tracker->add_client(client);
        } catch (const std::exception& e) {
            iERROR("%s", e.what());
            con_tracker->delete_client(client);
            close(client);
        }
    }
}

#pragma endregion
