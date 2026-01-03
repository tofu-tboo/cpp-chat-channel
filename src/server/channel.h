#ifndef __CHANNEL_H__
#define __CHANNEL_H__

typedef unsigned int ch_id_t;

#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdexcept>
#include <thread>
#include <atomic>

#include "server_base.h"
#include "../libs/util.h"

class Channel: ServerBase {
    private:
        ch_id_t channel_id;
        std::thread worker;
        std::atomic<bool> stop_flag{false};
    public:
        Channel();
        ~Channel();

        virtual void proc() override;

        void leave(const fd_t fd);
        void join(const fd_t fd);

    protected:
        virtual void on_accept() override;
        virtual void on_switch(const fd_t from, const char* target, Json& root, const std::string& payload) override;
    private:
};

#endif
