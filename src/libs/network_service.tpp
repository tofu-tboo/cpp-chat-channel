#include <climits>
#include <new>
#include "network_service.h"

template <typename T>
protocols_t NetworkService<T>::protocols[] = {
	{
		TCP_NAME,
		NetworkService<T>::lws_callback,
		sizeof(typename NetworkService<T>::Session),
		MAX_FRAME_SIZE + LWS_PRE,
	},
	{
		WS_NAME,
		NetworkService<T>::lws_callback,
		sizeof(typename NetworkService<T>::Session),
		MAX_FRAME_SIZE + LWS_PRE,
	},
	{ NULL, NULL, 0, 0 }
};


template <typename T>
NetworkService<T>::NetworkService(const int port): context(nullptr), fl_resv(false) {
	memset(&info, 0, sizeof(info));
	info.port = port;
	info.protocols = NetworkService<T>::protocols;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW | LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS; // TCP & WS compatibility
	info.timeout_secs = 15; // WS handshake timeout
	// info.fd_limit_per_thread = int;
	info.count_threads = 8; // TODO
	info.user = this;
}


template <typename T>
NetworkService<T>::~NetworkService() {
	if (context) {
		lws_context_destroy(context);
		context = nullptr;
	}
}

#pragma region PUBLIC_FUNC
template <typename T>
void NetworkService<T>::setup(SessionEvHandler<T>* i_handler) {
	if (context) return;
	handler = i_handler;
	
	context = lws_create_context(&info);
	if (!context) throw runtime_errorf("Failed to create context.");
}

template <typename T>
void NetworkService<T>::serve() {
	LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CG_ "Serve() ------------" _EC_, (void*)this);
	lws_service(context, 0);
}

// template <typename T>
// ctx* NetworkService<T>::get_ctx() const {
// 	return context;
// }

template <typename T>
void NetworkService<T>::send_async(Session* ses, const std::string& msg) {
	send_async(ses, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::send_async(Session* ses, const unsigned char* data, size_t len) {
	accumulate(ses, data, len);
	flush();
}

template <typename T>
void NetworkService<T>::broadcast_async(const std::string& msg) {
	broadcast_async(reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::broadcast_async(const unsigned char* data, size_t len) {
	for (auto& [group, sessions] : this->session_group) {
		for (auto ses : sessions) {
			accumulate(ses, data, len);
		}
	}
	flush();
}

template <typename T>
void NetworkService<T>::broadcast_group_async(int group, const std::string& msg) {
	broadcast_group_async(group, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}
template <typename T>
void NetworkService<T>::broadcast_group_async(int group, const unsigned char* data, size_t len) {
	for (auto ses: this->session_group[group]) {
		accumulate(ses, data, len);
	}
	flush();
}

template <typename T>
void NetworkService<T>::change_session_group(Session* ses, int new_group) {
    if (!ses) return;
    std::unique_lock<std::shared_mutex> lock(sg_mtx);
    
    // Remove from old group if it exists
    auto old_group_it = session_group.find(ses->group);
    if (old_group_it != session_group.end()) {
        old_group_it->second.erase(ses);
    }

    // Add to new group
    ses->group = new_group;
    session_group[new_group].insert(ses);
}

template <typename T>
void NetworkService<T>::register_handler(Session* ses, SessionEvHandler<T>* handler) {
	ses->handler = handler;
}

template <typename T>
void NetworkService<T>::close_async(Session* ses, const std::string& msg) {
	close_async(ses, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::close_async(Session* ses, const unsigned char* data, size_t len) {
	lws* wsi = ses->wsi;
	if (wsi) {
		std::unique_lock<std::shared_mutex> lock(dr_mtx);
		del_resv.insert({wsi, std::string(reinterpret_cast<const char*>(data), len)});
	}
	flush();
}

template <typename T>
void NetworkService<T>::flush() {
	if (!fl_resv) {
		lws_cancel_service(context);
		fl_resv = true;
	}
}
#pragma endregion
#pragma region PRIVATE_FUNC
template <typename T>
void NetworkService<T>::accumulate(Session* ses, const unsigned char* data, size_t len) {
	lws* wsi = ses->wsi;
	if (!wsi) throw runtime_errorf("Zero wsi.");
	else if (len > MAX_FRAME_SIZE) throw runtime_errorf("Frame too large.");

	short extra = ses->prot_id == TCP ? 4 : 0;
	std::vector<unsigned char> packet(LWS_PRE + len + extra);

	if (ses->prot_id == TCP) {
		char header[5];
		snprintf(header, sizeof(header), "%04x", (unsigned int)len);
		memcpy(&packet[LWS_PRE], header, 4);
	}

    if (len > 0) {
		memcpy(&packet[LWS_PRE + extra], data, len);
    }

    std::unique_lock<std::shared_mutex> lock(sr_mtx);
	send_resv[wsi].push(std::move(packet));
	lock.unlock();

	lws_callback_on_writable(wsi);
}

template <typename T>
void NetworkService<T>::check_pong(Session* ses) {
	msec64 now = now_ms();
	
	if (ses && ses->prot_id == TCP && ses->last_act) {
		if (now - ses->last_act > 10 * M2S) {
			close_async(ses, std::string(""));
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "Failed ping-pong of raw TCP user (%p)." _EC_, (void*)this, (void*)ses->wsi);
		}
		else if (now - ses->last_act > 5 * M2S) {
			send_async(ses, std::string("-")); // ping
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "Ping raw TCP user (%p)." _EC_, (void*)this, (void*)ses->wsi);
			
		}
	}
}

template <typename T>
void NetworkService<T>::set_timeout(Session* ses, int flag) {
	if (ses) {
		ses->to_flag = flag;
		if (flag & TO_EV_PING_PONG)
			lws_set_timer_usecs(ses->wsi, U2S);
	}
}

template <typename T>
int NetworkService<T>::lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	typename NetworkService<T>::Session* ses = static_cast<typename NetworkService<T>::Session*>(session);
	SessionEvHandler<T>* handler;
	decltype(LwsCallbackParam::event) event = LwsCallbackParam::NONE;
	NetworkService<T>* instance = static_cast<NetworkService<T>*>(lws_context_user(lws_get_context(wsi)));

	int in_offset = 0;

	// instance->pre_proc(wsi, reason, session, in, len);
	switch (reason) {
		case LWS_CALLBACK_RAW_ADOPT:
			set_timeout(ses, TO_EV_PING_PONG);
		case LWS_CALLBACK_ESTABLISHED:
		{
			event = LwsCallbackParam::ACPT;

			switch (hash(lws_get_protocol(wsi)->name)) {
				case hash(WS_NAME):
					ses->prot_id = WS;
					break;
				case hash(TCP_NAME):
					ses->prot_id = TCP;
					break;
			}
			ses->handler = static_cast<SessionEvHandler<T>*>(instance->handler);
			ses->user = new T();
			ses->wsi = wsi;
			ses->group = INT_MIN; //reserved
			ses->last_act = now_ms();
			ses->tokens = RL_BURST_MAX; // 초기 접속 시 최대치 부여

			std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
			instance->send_resv[wsi] = std::queue<std::vector<unsigned char>>();
			lock.unlock();

			std::unique_lock<std::shared_mutex> lock2(instance->sg_mtx);
			instance->session_group[ses->group].insert(ses);
			lock2.unlock();

			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] A Session is initialized." _EC_, (void*)instance, (void*)wsi);
			break;
		}
		case LWS_CALLBACK_RAW_RX:
		{
			// Do only length validation
			std::string acc(static_cast<const char*>(in), len);

			ses->last_act = now_ms();
			if (acc.size() >= 4) {
				uint32_t len = 0;
				try {
					len = std::stoul(acc.substr(0, 4), nullptr, 16);
				} catch (...) {
					throw runtime_errorf("Invalid frame header from Session %p", (void*)wsi);
				}

				if (len > MAX_FRAME_SIZE) {
					throw runtime_errorf("Frame too large from Session %p", (void*)wsi);
				} else if (len == 0) {
					return 0;
				} else if (len == 2) { // pong 0002{}
					LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "[%14p] Pong" _EC_, (void*)instance, (void*)wsi);
					return 0;
				} else if (acc.size() < 4 + len) {
					return -1;
				}

				in_offset = 4;
			}
		}
		case LWS_CALLBACK_RECEIVE:
		{
			event = LwsCallbackParam::RECV;

			if (ses->tokens == RL_BURST_MAX)
				instance->set_timeout(ses, TO_EV_TOKEN_REFILL);
			else if (ses->tokens == 0) {
				event = LwsCallbackParam::RL_DROP;
				lws_rx_flow_control(wsi, 0);
				break;
			}
			
			ses->tokens--;

			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] Frame received." _EC_, (void*)instance, (void*)wsi);
			break;
		}
        case LWS_CALLBACK_RAW_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
		{
			std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
			if (!instance->send_resv[wsi].empty()) {
				event = LwsCallbackParam::SEND;
			
				if (lws_partial_buffered(wsi)) {
					lock.unlock();
					lws_callback_on_writable(wsi);
					LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "[%14p] Unsent data exist." _EC_, (void*)instance, (void*)wsi);
					break;
				}
				std::vector<unsigned char> packet;
				
				packet = std::move(instance->send_resv[wsi].front());
				instance->send_resv[wsi].pop();
				bool more = !instance->send_resv[wsi].empty();
				lock.unlock();
				
				int n;
				lws_write_protocol flag;
				switch (ses->prot_id) {
					case WS:
						flag = LWS_WRITE_TEXT;
						break;
					case TCP:
						flag = LWS_WRITE_RAW;
						break;
					default:
						return -1;
				}

				n = lws_write(wsi, &packet[LWS_PRE], packet.size() - LWS_PRE, flag);

				if (n < 0) {
					ERROR(_CG_ "NetworkService [%14p]" _EC_ " / " _CR_ "[%14p] Try to send minus frame.", (void*)instance,(void*)wsi);
					return -1;
				}

				if (more) {
					LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "[%14p] Send is deferred." _EC_, (void*)instance, (void*)wsi);
					lws_callback_on_writable(wsi);
				}
			}
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] Frame sent." _EC_, (void*)instance, (void*)wsi);
            break;
		}
		case LWS_CALLBACK_RAW_CLOSE:
		case LWS_CALLBACK_CLOSED:
		{
			event = LwsCallbackParam::CLOSE;
			break;
		}
		case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		{
			instance->fl_resv = false;
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "Start side action." _EC_, (void*)instance);
			std::map<lws*, std::string> dels;

			std::unique_lock<std::shared_mutex> lock(instance->dr_mtx);
			dels = std::move(instance->del_resv);
			lock.unlock();
			
			if (!dels.empty()) {
				LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "Close asynchronously: Sessions * %d." _EC_, (void*)instance, (int)dels.size());
				for (auto [wsi_to_close, msg]: dels) {
					typename NetworkService<T>::Session* ses_to_close = static_cast<typename NetworkService<T>::Session*>(lws_wsi_user(wsi_to_close));

					if (ses_to_close && ses_to_close->prot_id == WS) {
						lws_close_reason(wsi_to_close, LWS_CLOSE_STATUS_NORMAL, reinterpret_cast<unsigned char*>(msg.data()), msg.size());
					}
				
					// deferred to next loop
					lws_set_timeout(wsi_to_close, PENDING_TIMEOUT_CLOSE_ACK, LWS_TO_KILL_ASYNC);
				}
			}
			break;
		}
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION: 
		{
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] Network connection detected." _EC_, (void*)instance, (void*)wsi);
			// char ip[64];
			// lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), 0, 0, ip, sizeof(ip));

			// // 2. 현재 해당 IP의 연결 수를 카운트 (내부 Map 등 활용)
			// int current_conn = get_connection_count_by_ip(ip);

			// // 3. 임계치 초과 시 연결 거부
			// if (current_conn >= MAX_ALLOWED_WSI_PER_IP) {
			// 	printf("IP %s: 연결 한도 초과로 차단합니다.\n", ip);
			// 	return -1; // 여기서 -1을 리턴하면 소켓 수락 단계에서 바로 끊김
			// }
			break;
		}
		case LWS_CALLBACK_TIMER:
		{
			if (ses->to_flag & TO_EV_PING_PONG) {
				instance->check_pong(ses);
				instance->set_timeout(ses, TO_EV_PING_PONG);
			}
			if (ses->to_flag & TO_EV_TOKEN_REFILL) {
				ses->to_flag ^= TO_EV_TOKEN_REFILL;
				ses->tokens = RL_BURST_MAX;
				lws_rx_flow_control(wsi, 1);
			}
			break;
		}
		default:
			break;
	}	
	// instance->post_proc(wsi, reason, session, in, len);

	if (event == LwsCallbackParam::NONE)
		return 0;

	int ret = 0;
	if (ses) {
		handler = ses->handler;
		ret = handler->callback({ .wsi = wsi, .event = event, .user = ses, .in = static_cast<unsigned char*>(in) + in_offset, .len = len - in_offset, .prot_id = ses->prot_id });
	}

	// Deferred cleanup
	if (event == LwsCallbackParam::CLOSE) {
		if (ses->user)
			delete ses->user;
		ses->wsi = nullptr;

		std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
		auto it = instance->send_resv.find(wsi);
		if (it != instance->send_resv.end()) {
			instance->send_resv.erase(it);
		}
		lock.unlock();

		std::unique_lock<std::shared_mutex> lock2(instance->sg_mtx);
		// Remove from the group it belongs to (free_user might have changed it to INT_MIN)
		auto it2 = instance->session_group[ses->group].find(ses);
		if (it2 != instance->session_group[ses->group].end())
			instance->session_group[ses->group].erase(it2);
		lock2.unlock();

		LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] A Session is closed." _EC_, (void*)instance, (void*)wsi);
	}

	return ret;
}
#pragma endregion

// #pragma region PROTECTED_FUNC
// template <typename T>
// void NetworkService<T>::pre_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {}

// template <typename T>
// void NetworkService<T>::post_proc(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {}

// #pragma endregion