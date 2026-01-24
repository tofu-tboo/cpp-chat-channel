
#include "connection_tracker.h"

ConnectionTracker::ConnectionTracker(fd_t& fd, const int max_fd): efd(FD_ERR), listener_fd(fd), max_fd(max_fd) {}

ConnectionTracker::~ConnectionTracker() {
    if (efd != FD_ERR) { // cleanup epoll clients
        for (fd_t client_fd : clients) {
            epoll_ctl(efd, EPOLL_CTL_DEL, client_fd, nullptr);
            if (client_fd != FD_ERR) {
                close(client_fd);
            }
        }
        close(efd);
        efd = FD_ERR;
    } else { // cleanup only clients
        for (fd_t client_fd : clients) {
            if (client_fd != FD_ERR) {
                close(client_fd);
            }
        }
    }
    clients.clear();
}

void ConnectionTracker::init() {
    if ((efd = epoll_create1(0)) == FD_ERR) {
        throw std::runtime_error("Failed to create epoll instance.");
    }
    pollev ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listener_fd;
    if (FAILED(epoll_ctl(efd, EPOLL_CTL_ADD, listener_fd, &ev))) {
        throw std::runtime_error("Failed to add listen fd to epoll.");
    }

}

void ConnectionTracker::polling(const msec to) {
    if (FAILED(evcnt = epoll_wait(efd, events, MAX_PEV, to))) {
        throw std::runtime_error("Failed during polling.");
    }
}

void ConnectionTracker::add_client(const int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    if (fd == FD_ERR) {
        throw std::runtime_error("Invalid client fd.");
    } else if (efd == FD_ERR) {
        throw std::runtime_error("Epoll instance is not initialized.");
    } else if (fd == listener_fd) {
        throw std::runtime_error("Listening socket cannot be re-added as a client.");
    } else if (clients.size() >= max_fd) {
        throw runtime_errorf(POOL_FULL, "Pool full.");
    }

    pollev ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (FAILED(epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev))) {
        throw runtime_errorf("Failed to add fd %d to epoll.", fd);
    }

    clients.insert(fd);
}
void ConnectionTracker::delete_client(const int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    if (fd == FD_ERR) {
        throw std::runtime_error("Invalid client fd.");
    } else if (efd == FD_ERR) {
        throw std::runtime_error("Epoll instance is not initialized.");
    } else if (fd == listener_fd) {
        throw std::runtime_error("Listening socket cannot be deleted.");
    } else if (FAILED(epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr))) {
        throw runtime_errorf("Failed to remove fd %d from epoll.", fd);
    }

    if (clients.find(fd) != clients.end())
        clients.erase(fd);
}

const pollev* ConnectionTracker::get_ev() const {
    return events;
}

const int ConnectionTracker::get_evcnt() const {
    return evcnt;
}

std::unordered_set<fd_t> ConnectionTracker::get_clients() const {
    std::lock_guard<std::mutex> lock(mtx);
    return clients;
}

bool ConnectionTracker::is_full() const {
	std::lock_guard<std::mutex> lock(mtx);
	return clients.size() >= max_fd;
}

int ConnectionTracker::get_max_fd() const {
	return max_fd;
}

size_t ConnectionTracker::get_client_count() const {
	std::lock_guard<std::mutex> lock(mtx);
	return clients.size();
}
