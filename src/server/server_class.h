#ifndef __SERVER_CLASS_H__
#define __SERVER_CLASS_H__

typedef struct addrinfo sAddrinfo;
typedef struct pollfd sPollFD;

class Server {
    private:
        int fd;
        pollfd* listeners;
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
};

#endif