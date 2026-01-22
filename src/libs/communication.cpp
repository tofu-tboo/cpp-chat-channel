
#include <sys/socket.h>

#include "communication.h"

Communication::~Communication() {
	rbuf.clear();
}

std::vector<std::string> Communication::recv_frame(const fd_t fd) {
	// NEEDS: attach 4-byte length header to each frame

    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n < 0) {
        throw std::runtime_error("Minus frame.");
    } else if (n == 0) {
		throw std::runtime_error("Disconnected.");
	}

	std::vector<std::string> frames;
    std::string& acc = rbuf[fd];
    acc.append(buf, n);

    while (acc.size() >= 4) {
        uint32_t len = ((uint8_t)acc[0] << 24) | ((uint8_t)acc[1] << 16) | ((uint8_t)acc[2] << 8) | (uint8_t)acc[3];
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