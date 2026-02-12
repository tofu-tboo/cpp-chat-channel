#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <map>
#include <mutex>

#include "typed_json_frame_server.h"
#include "chat_server.h"
#include "channel.h"
#include "../libs/json.h"
#include "channel_factory.h"


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

		ChannelFactory* channel_factory;

		std::shared_mutex la_mtx;
		std::shared_mutex chs_mtx;
    public:
        ChannelServer(NetworkService<User>* service, const int max_fd, ChannelFactory* factory, const msec to = 1000);
        ~ChannelServer();
		virtual bool init() override;
		void switch_channel(typename NetworkService<User>::Session& ses, const ch_id_t from, const ch_id_t to);
    protected:

		// virtual void resolve_close() override;

		virtual void on_accept(typename NetworkService<User>::Session& ses) override;
        virtual void on_req(const typename NetworkService<User>::Session& ses, const char* target, Json& root) override;

		virtual void free_user(typename NetworkService<User>::Session& ses) override;
	private:
		Channel* get_channel(const ch_id_t channel_id);
        Channel* find_or_create_channel(ch_id_t preferred_id);
		void check_lobby();
		void check_channels();
};


#endif
