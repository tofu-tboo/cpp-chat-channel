#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <unordered_map>
#include <queue>
#include <mutex>

#include "chat_server.h"
#include "channel.h"
#include "../libs/json.h"
#include "../libs/socket.h"
#include "../libs/dto.h"
#include "../libs/producer_consumer.h"


/* Requirement of ChannelServer
- Manage Channels: Create and manage multiple Channel instances.
- Handle Channel Reports: Process requests from channels.

*/

class ChannelServer: public ServerBase {
    public:
        struct ChannelReport {
			enum { JOIN, JOIN_BLOCK } type;
            fd_t from;
			UReportDto dto;
        };
    private:
        std::unordered_map<ch_id_t, Channel*> channels;
		ProducerConsumerQueue<ChannelReport> reports;
        std::mutex report_mtx;
		std::unordered_map<fd_t, std::chrono::steady_clock::time_point> last_act;

		int ch_max_fd;
    public:
        ChannelServer(const int max_fd = 256, const int ch_max_fd = 32, const msec to = 0);
        ~ChannelServer();
        void report(const ChannelReport& req);
    protected:
		virtual void on_accept() override;
		virtual void on_recv(const fd_t from) override;
        virtual void on_req(const fd_t from, const char* target, Json& root) override;
		void consume_report();
	private:
		Channel* get_channel(const ch_id_t channel_id);
		void check_lobby();
};


#endif