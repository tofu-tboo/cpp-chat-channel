#include <climits>
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
NetworkService<T>::NetworkService(const int port): context(nullptr) {
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


template <typename T>
void NetworkService<T>::setup(SessionEvHandler<T>* i_handler) {
	if (context) throw runtime_errorf("Context is already initialized.");
	handler = i_handler;
	
	context = lws_create_context(&info);
	if (!context) throw runtime_errorf("Failed to create context.");
}

template <typename T>
void NetworkService<T>::serve(const msec to) {
	// LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CG_ "Serve on." _EC_, (void*)this);
	lws_service(context, to);
	// LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CG_ "Serve off." _EC_, (void*)this);
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
void NetworkService<T>::broadcast_group_async(int group, std::string& msg) {
	broadcast_group_async(group, msg.c_str(), msg.size());
}
template <typename T>
void NetworkService<T>::broadcast_group_async(int group, const unsigned char* data, size_t len) {
	for (auto ses: this->session_group[group]) {
		accumulate(ses, data, len);
	}
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
	lws_cancel_service(context);
}
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
int NetworkService<T>::lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	typename NetworkService<T>::Session* ses = static_cast<typename NetworkService<T>::Session*>(session);
	SessionEvHandler<T>* handler;
	decltype(LwsCallbackParam::event) event = LwsCallbackParam::NONE;
	NetworkService<T>* instance = static_cast<NetworkService<T>*>(lws_context_user(lws_get_context(wsi)));

	int in_offset = 0;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
		case LWS_CALLBACK_RAW_ADOPT:
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

			{
				std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
				instance->send_resv[wsi] = std::queue<std::vector<unsigned char>>();
			}

			{
				std::unique_lock<std::shared_mutex> lock(instance->sg_mtx);
				instance->session_group[ses->group].insert(ses);
			}

			LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] A Session is initialized." _EC_, (void*)instance, (void*)wsi);
			break;
		}
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_RAW_RX:
			event = LwsCallbackParam::RECV;
			switch (ses->prot_id)
			{
			case TCP:
				{
					// Do only length validation
					std::string acc(static_cast<const char*>(in), len);

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
							return -1;
						} else if (acc.size() < 4 + len) {
							return -1;
						}

						in_offset = 4;
					}
					break;
				}
			default:
				break;
			}
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
        case LWS_CALLBACK_RAW_WRITEABLE:
		{
			std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
			if (ses && !instance->send_resv[wsi].empty()) {
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
            break;
		}
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_RAW_CLOSE:
		{
			if (ses) {
				event = LwsCallbackParam::CLOSE;

				if (ses->user)
					delete ses->user;
				ses->user = nullptr;
				ses->wsi = nullptr;

				{
					std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
					auto it = instance->send_resv.find(wsi);
					if (it != instance->send_resv.end()) {
						instance->send_resv.erase(it);
					}
				}

				{
					std::unique_lock<std::shared_mutex> lock(instance->sg_mtx);
					auto it = instance->session_group[ses->group].find(ses);
				
					instance->session_group[ses->group].erase(it);
				}

				LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CB_ "[%14p] A Session is closed." _EC_, (void*)instance, (void*)wsi);
			}
			break;
		}
		case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		{
			std::map<lws*, std::string> dels;
			{
				std::unique_lock<std::shared_mutex> lock(instance->sr_mtx);
				dels = std::move(instance->del_resv);
			}
			if (!dels.empty()) {
				LOG(_CG_ "NetworkService [%14p]" _EC_ " / " _CY_ "Close asynchronously: Sessions * %d." _EC_, (void*)instance, (int)dels.size());
				for (auto [wsi_to_close, msg]: dels) {
					if (ses->prot_id == WS) {
						lws_close_reason(wsi_to_close, LWS_CLOSE_STATUS_NORMAL, reinterpret_cast<unsigned char*>(msg.data()), msg.size());
					}
				
					// deferred to next loop
					lws_set_timeout(wsi_to_close, PENDING_TIMEOUT_CLOSE_ACK, LWS_TO_KILL_ASYNC);
				}
				// lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NORMAL); // 즉시 종료
			}
			break;
		}
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION: 
		{
			LOG(_CG_ "NetworkService [%14p]" _EC_ " / [%14p] Network connection detected.", (void*)instance, (void*)wsi);
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
		default:
			break;
	}	

	if (event == LwsCallbackParam::NONE)
		return 0;

	handler = ses->handler;

	if (handler) {
		return handler->callback({ .wsi = wsi, .event = event, .user = ses, .in = static_cast<unsigned char*>(in) + in_offset, .len = len - in_offset, .prot_id = ses->prot_id });
	}
	return 0;
}
#pragma endregion