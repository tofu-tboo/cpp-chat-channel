#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#define MAX_FRAME_SIZE      		(16 * 1024)
#define DISCONNECTED_BY_FIN 		500

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

#include "../libs/socket.h"
#include "../libs/util.h"

class Communication {
	private:
        std::unordered_map<fd_t, std::string> rbuf; // per-connection accumulation buffer
	public:
		~Communication();

        virtual std::vector<std::string> recv_frame(const fd_t fd); // frame format can be overridden
        virtual void send_frame(const fd_t fd, const std::string& payload); // frame format can be overridden
        virtual std::vector<fd_t> broadcast(const std::unordered_set<fd_t>& clients, const std::string& payload);

		void clear_buffer(const fd_t fd);
};

#endif