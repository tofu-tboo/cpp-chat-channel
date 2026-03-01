#ifndef __CHANNEL_SERVER_H__
#define __CHANNEL_SERVER_H__

#include <map>
#include <mutex>

#include "server_base.h"
#include "chat_server.h"
#include "channel.h"
#include "channel_factory.h"
#include "../libs/chat_req_dto.h"
#include "../libs/set_super.h"
#include "channel_server_types.h"

/* Requirement of ChannelServer
- Manage Channels: Create and manage multiple Channel instances.
- Handle Channel Reports: Process requests from channels.

*/

// template <typename U> class JsonMessageProcessor;

class ChannelServer: public ServerBase<User> {
	SET_SUPER(ServerBase<User>);
    var_private:
        std::map<ch_id_t, Channel*> channels;
        std::mutex report_mtx;
		std::unordered_map<Session*, msec64> last_act;

		std::unique_ptr<ChannelFactory> channel_factory;

		std::shared_mutex la_mtx;
		std::shared_mutex chs_mtx;
    func_public:
        ChannelServer(std::shared_ptr<NetworkService<User>> service, const int max_fd, std::unique_ptr<ChannelFactory> factory);
        ~ChannelServer();
		virtual bool init() override;
		void switch_channel(Session& ses, const ch_id_t from, const ch_id_t to);
    func_protected:

		// virtual void resolve_close() override;

		virtual void on_accept(Session& ses) override;
        virtual void handle_request(Session& ses, std::unique_ptr<Request> req) override;

		virtual void free_user(Session& ses) override;
	func_private:
		Channel* get_channel(const ch_id_t channel_id);
        Channel* find_or_create_channel(ch_id_t preferred_id);
		void check_lobby();
};


#endif
