#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <map>

#include "typed_json_frame_server.h"
#include "chat_server.h"
#include "channel.h"
#include "../libs/json.h"


/* Requirement of ChannelServer
- Manage Channels: Create and manage multiple Channel instances.
- Handle Channel Reports: Process requests from channels.

*/

class ChannelServer: public TypedJsonFrameServer<User> {
    public:
        struct ChannelReport {
			enum { JOIN } type;
            typename NetworkService<User>::Session* from;
			UReportDto dto;
        };
    private:
        std::map<ch_id_t, Channel*> channels;
		ProducerConsumerQueue<ChannelReport> reports;
        std::mutex report_mtx;
		std::unordered_map<typename NetworkService<User>::Session*, msec64> last_act;

		int ch_max_conn;
    public:
        ChannelServer(NetworkService<User>* service, const int max_fd = 256, const int ch_max_fd = 32, const msec to = 1000);
        ~ChannelServer();
		virtual bool init() override;
        void report(const ChannelReport& req);
    protected:

		virtual void resolve_deletion() override;

		virtual void on_accept(typename NetworkService<User>::Session& ses) override;
        virtual void on_req(const typename NetworkService<User>::Session& ses, const char* target, Json& root) override;
		void consume_report();
	private:
		Channel* get_channel(const ch_id_t channel_id);
        Channel* find_or_create_channel(ch_id_t preferred_id);
		void check_lobby();
		void check_channels();
};


#endif