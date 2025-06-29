#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>

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

    add_listener(fd);
}

Server::~Server() {
    if (fd) {
        close(fd);
        fd = -1;
        LOG(_CG_ "Server is closed successfully." _EC_);
    }

    for (int i = 0; i < num_listeners; i++) {
        close(listeners[i].fd);
    }
    if (listeners) {
        free(listeners);
    }
}

void Server::lobby() {
    while (1) {
        int ready = poll(listeners, 1, 0);
        if (ready == -1) {
            ERROR("Failed to poll in lobby.");
            break;
        }

        if (listeners[0].revents & POLLIN) {
            int client = accept(fd, NULL, NULL);
            if (client == -1) {
                ERROR("Failed to accept.");
                break;
            }

            add_listener(client);
        }

        for (int i = 0; i < num_listeners; i++) {
            if (listeners[i].revents & POLLIN) {

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

    status = getaddrinfo(NULL, "5000", &hints, &res);
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
    sPollFD* temp = (sPollFD*)malloc(sizeof(sPollFD) * (num_listeners - 1));

    if (!temp) {
        ERROR("A listener can't be deleted.");
        return false;
    }

    int j = 0;
    for (int i = 0; i < num_listeners; i++) {
        if (listeners[i].fd == fd)
            continue;
        temp[j] = listeners[i];
        j++;
    }
    num_listeners--;
    free(listeners);
    listeners = temp;

    return true;
}
#pragma endregion