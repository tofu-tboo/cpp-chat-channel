#include <cstdio>
#include <sys/socket.h>
#include <cerrno>

#include "communication.h"

Communication::~Communication() {
	rbuf.clear();
}

std::vector<std::string> Communication::recv_frame(const fd_t fd) {
	// NEEDS: attach 4-byte length header to each frame

	std::vector<std::string> frames;
    std::string& acc = rbuf[fd];
    char buf[4096];

    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            throw std::runtime_error("Recv failed.");
        } else if (n == 0) {
            throw runtime_errorf(DISCONNECTED_BY_FIN, "Disconnected: fd %d", fd);
        }
        acc.append(buf, n);
        if (n < 4096) break;
    }

    while (acc.size() >= 4) {
        uint32_t len = 0;
        try {
            len = std::stoul(acc.substr(0, 4), nullptr, 16);
        } catch (...) {
            throw std::runtime_error("Invalid frame header.");
        }

        if (len > MAX_FRAME_SIZE) {
            throw runtime_errorf("Frame too large from fd %d", fd);
        } else if (len == 0) {
            acc.erase(0, 4);
            continue;
        } else if (acc.size() < 4 + len) {
            break; // wait for full frame
        }
        
        std::string payload = acc.substr(4, len);
		frames.push_back(payload);

        acc.erase(0, 4 + len);
    }

	return frames;
}
void Communication::send_frame(const fd_t fd, const std::string& payload) {
	uint32_t len = static_cast<uint32_t>(payload.size());
    if (len == 0) return;
    if (len > MAX_FRAME_SIZE) {
        throw std::runtime_error("Frame too large.");
    }

    char header[5];
    std::snprintf(header, sizeof(header), "%04x", len);

    ssize_t sent = send(fd, header, 4, MSG_NOSIGNAL); // MSG_NOSIGNAL => prevent SIGPIPE abort
    if (sent != 4) {
        throw std::runtime_error("Invalid header sent.");
    }

    size_t total = 0;
    while (total < payload.size()) {
        ssize_t n = send(fd, payload.data() + total, payload.size() - total, MSG_NOSIGNAL);
        if (n <= 0) {
            throw std::runtime_error("Minus frame.");
        }
        total += static_cast<size_t>(n);
    }
}
std::vector<fd_t> Communication::broadcast(const std::unordered_set<fd_t>& clients, const std::string& payload) {
	std::vector<fd_t> failed_fds;
	for (const fd_t& fd : clients) {
		try {
			send_frame(fd, payload);
		} catch (const std::exception&) {
			failed_fds.push_back(fd);
		}
	}

	return failed_fds;
}

void Communication::clear_buffer(const fd_t fd) {
	rbuf.erase(fd);
}