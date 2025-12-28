#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>
#include <cstdint>
#include <string>
#include <algorithm>

#include "server_class.h"
#include "../libs/util.h"

int Server::num_listeners = 0;

Server::Server() {
    fd = -1;
    listeners = nullptr;

    if (!set_network()) {
        ERROR("Failed to set network.");
        if (fd != -1)
            close(fd);
    }

    LOG(_CG_ "Server initialized on port 4800." _EC_);
    add_listener(fd);
}

Server::~Server() {
    if (listeners) {
        for (int i = 0; i < num_listeners; i++) {
            if (listeners[i].fd != -1) {
                close(listeners[i].fd);
            }
        }
        free(listeners);
        listeners = nullptr;
        num_listeners = 0;
        fd = -1;
    } else if (fd != -1) { // fallback if listeners array was never allocated
        close(fd);
        fd = -1;
    }

    recv_buffers.clear();
    message_queue.clear();
    next_deletion.clear();
}

void Server::lobby() {
    while (1) {
        next_deletion.clear();

        int ready = poll(listeners, num_listeners, 0);
        if (ready == -1) {
            ERROR("Failed to poll in lobby.");
            break;
        }

        // 1. Listen for new connections
        if (num_listeners > 0 && (listeners[0].revents & POLLIN)) { // server
            int client = accept(fd, NULL, NULL);
            if (client == -1) {
                ERROR("Failed to accept.");
                break;
            }

            add_listener(client);
        }

        // 2. Listen to clients
        for (int i = 1; i < num_listeners; i++) { // clients
            if (listeners[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                next_deletion.push_back(listeners[i].fd);
                continue;
            }
            if (listeners[i].revents & POLLIN) {
                if (!read_from_client(listeners[i].fd)) {
                    next_deletion.push_back(listeners[i].fd);
                }
            }
        }

        // 3. Consume message queue
        while (!message_queue.empty()) {
            std::string payload = std::move(message_queue.back());
            message_queue.pop_back();
            broadcast_message(payload);
            LOG("Broadcasted message: %s", payload.c_str());
        }

        // 4. Cleanup session
        if (!next_deletion.empty()) {
            std::sort(next_deletion.begin(), next_deletion.end());
            next_deletion.erase(std::unique(next_deletion.begin(), next_deletion.end()), next_deletion.end());
            for (int fd_to_delete : next_deletion) {
                delete_listener(fd_to_delete);
            }
        }
    }
}

#pragma region PRIVATE_FUNC
bool Server::set_network() {
    if (fd != -1) {
        ERROR("Sever descriptor is already assigned.");
        return false;
    }

    sAddrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "4800", &hints, &res);
    if (status != 0) {
        ERROR("The getaddrinfo() is not resolved.");
        return false;
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        ERROR("Failed to get socket fd.");
        return false;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(fd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        ERROR("Failed to bind server.");
        return false;
    }

    status = listen(fd, 5);
    if (status == -1) {
        ERROR("Failed to set listen().");
        return false;
    }

    freeaddrinfo(res);

    return true;
}

bool Server::add_listener(const int fd) {
    sPollFD* temp = (sPollFD*)realloc(listeners, sizeof(sPollFD) * (num_listeners + 1));

    if (!temp) {
        ERROR("A listener can't be added.");
        return false;
    }

    listeners = temp;
    listeners[num_listeners].fd = fd;
    listeners[num_listeners].events = POLLIN;
    num_listeners++;
    return true;
}
bool Server::delete_listener(const int fd) {
    if (num_listeners == 0) {
        return false;
    }

    int target = -1;
    for (int i = 0; i < num_listeners; i++) {
        if (listeners[i].fd == fd) {
            target = i;
            break;
        }
    }

    if (target == -1) {
        return false; // not found
    }
    if (target == 0) {
        ERROR("Listener socket cannot be deleted.");
        return false;
    }

    if (listeners[target].fd != -1) {
        close(listeners[target].fd);
    }

    for (int i = target; i < num_listeners - 1; i++) {
        listeners[i] = listeners[i + 1];
    }

    num_listeners--;
    sPollFD* temp = (sPollFD*)realloc(listeners, sizeof(sPollFD) * num_listeners);
    if (temp) {
        listeners = temp;
    }
    recv_buffers.erase(fd);

    LOG("Closed connection on fd %d", fd);

    return true;
}

bool Server::read_from_client(const int fd) {
    // NEEDS: attach 4-byte length header to each frame
    //{"type":"message","channel_id":"c1","user_id":"u1","payload":{"text":"message"}}

    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        return false; // closed or error
    }

    std::string& acc = recv_buffers[fd];
    acc.append(buf, n);

    while (acc.size() >= 4) {
        uint32_t len = ((uint8_t)acc[0] << 24) | ((uint8_t)acc[1] << 16) | ((uint8_t)acc[2] << 8) | (uint8_t)acc[3];
        if (len > MAX_FRAME_SIZE) {
            ERROR("Frame too large from fd %d", fd);
            return false;
        }
        if (len == 0) {
            ERROR("Empty frame from fd %d", fd);
            acc.erase(0, 4);
            continue;
        }
        if (acc.size() < 4 + len) {
            break; // wait for full frame
        }
        std::string payload = acc.substr(4, len);
        message_queue.push_back(payload);
        acc.erase(0, 4 + len);
    }

    return true;
}

bool Server::send_frame(const int fd, const std::string& payload) {
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

void Server::broadcast_message(const std::string& payload) {
    for (int i = 1; i < num_listeners; i++) {
        int target_fd = listeners[i].fd;
        if (!send_frame(target_fd, payload)) {
            next_deletion.push_back(target_fd);
        }
    }
}
#pragma endregion
