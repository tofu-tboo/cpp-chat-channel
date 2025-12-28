#ifndef __SERVER_CLASS_H__
#define __SERVER_CLASS_H__

#include <unordered_map>
#include <string>
#include <vector>

typedef struct addrinfo sAddrinfo;
typedef struct pollfd sPollFD;

class Server {
    private:
        int fd;
        pollfd* listeners;
        std::unordered_map<int, std::string> recv_buffers; // per-connection accumulation buffer
        std::vector<std::string> message_queue; // parsed json frames
        std::vector<int> next_deletion;

        static const size_t MAX_FRAME_SIZE = 16 * 1024;
    public:
        static int num_listeners;
    public:
        Server();
        ~Server();

        void lobby();

    private:
        bool set_network();
        bool add_listener(const int fd);
        bool delete_listener(const int fd);
        bool read_from_client(const int fd);
        bool send_frame(const int fd, const std::string& payload);
        void broadcast_message(const std::string& payload);
};

#endif
