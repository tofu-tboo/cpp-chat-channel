#include "lws_service.h"
#include "times.h"
#include "hash.h"
#include "exception.h"

template <typename T>
protocols_t LwsService<T>::protocols[] = {
	{
		TCP_NAME,
		LwsService<T>::lws_callback,
		sizeof(typename LwsService<T>::Session),
		MAX_FRAME_SIZE + LWS_PRE,
	},
	{
		WS_NAME,
		LwsService<T>::lws_callback,
		sizeof(typename LwsService<T>::Session),
		MAX_FRAME_SIZE + LWS_PRE,
	},
	{ NULL, NULL, 0, 0 }
};

template <typename T>
LwsService<T>::LwsService(const int port): context(nullptr), fl_resv(false), Loggable("LwsService", _L_GREEN, this) {
	memset(&info, 0, sizeof(info));
	info.port = port;
	info.protocols = LwsService<T>::protocols;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW | LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS; // TCP & WS compatibility
	info.timeout_secs = 15; // WS handshake timeout
	// info.fd_limit_per_thread = int;
	info.count_threads = 8; // TODO
	info.user = this;
}

template <typename T>
LwsService<T>::~LwsService() {
	if (context) {
		lws_context_destroy(context);
		context = nullptr;
	}

	lws_sul_cancel(&tlist); // stack safe
}

#pragma region PUBLIC_FUNC
template <typename T>
void LwsService<T>::setup(SessionEvHandler<T>* i_handler) {
	if (context) return;
	super::setup(i_handler);
	
	context = lws_create_context(&info);
	if (!context) throw runtime_errorf("Failed to create context.");
}

template <typename T>
void LwsService<T>::serve() {
	log(_L_WHITE "Serve()------------");
	lws_service(context, 0);
}

template <typename T>
void LwsService<T>::send(Session* ses, const unsigned char* data, size_t len) {
	if (!ses) {
		elog("Null Session");
		return;
	}
	accumulate(ses, data, len);
	lws* wsi;
	if (wsi_map.get(ses, wsi)) {
		lws_callback_on_writable(wsi);
	} else {
		elog("wsi_map does not contain Session %p.", (void*)ses);
	}
	flush();
}

template <typename T>
void LwsService<T>::broadcast(const unsigned char* data, size_t len) {
	session_group.task([this, data, len](auto& ses_group) {
		for (auto& [group, sessions] : ses_group) {
			for (auto ses : sessions) {
				accumulate(ses, data, len);
				lws* wsi;
				if (wsi_map.get(ses, wsi)) {
					lws_callback_on_writable(wsi);
				}
				else {
					elog("wsi_map does not contain Session %p.", (void*)ses);
				}
			}
		}
	});
	flush();
}

template <typename T>
void LwsService<T>::broadcast_group(int group, const unsigned char* data, size_t len) {
	session_group.task([this, group, data, len](auto& ses_group) {
		for (auto ses: ses_group[group]) {
			accumulate(ses, data, len);
			lws* wsi;
			if (wsi_map.get(ses, wsi)) {
				lws_callback_on_writable(wsi);
			}
			else {
				elog("wsi_map does not contain Session %p.", (void*)ses);
			}
		}
	});
	flush();
}

template <typename T>
void LwsService<T>::close(Session* ses, const unsigned char* data, size_t len) {
	if (ses) {
		del_resv.add(ses, std::string(reinterpret_cast<const char*>(data), len));
	}
	flush();
}

#pragma endregion
#pragma region PRIVATE_FUNC
template <typename T>
void LwsService<T>::flush() {
	if (!fl_resv) {
		lws_cancel_service(context);
		fl_resv = true;
	}
}


template <typename T>
__CALLBACK_SAFE__ void LwsService<T>::check_pong(Session* ses) {
	msec64 now = now_ms();
	
	if (ses && SA::prot_id(ses) == TCP && SA::last_act(ses)) {
		if (now - SA::last_act(ses) > 10 * S2M) {
			close(ses, std::string(""));
			log(_L_BLUE "Failed ping-pong of raw TCP Session (%p).", (void*)ses);
		}
		else if (now - SA::last_act(ses) > 5 * S2M) {
			send(ses, std::string("-")); // ping
			log(_L_BLUE "Ping raw TCP Session (%p).", (void*)ses);
		}
	}
}

template <typename T>
__CALLBACK_SAFE__ void LwsService<T>::set_timeout(Session* ses, lws* wsi, int flag) {
	if (ses) {
		SA::to_flag(ses) = flag;
		if (flag & TO_EV_PING_PONG)
			lws_set_timer_usecs(wsi, S2U);
	}
}

template <typename T>
int LwsService<T>::lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	Session* ses = static_cast<Session*>(session);
	SessionEvHandler<T>* handler;
	NS_EV event = NS_EV::NONE;
	LwsService<T>* instance = static_cast<LwsService<T>*>(lws_context_user(lws_get_context(wsi)));

	int in_offset = 0;

	switch (reason) {
		case LWS_CALLBACK_PROTOCOL_INIT:
		{
			// lws_sul_schedule(instance->context, lws_get_tsi(wsi), &instance->tlist, /* 함수 */, 500 * M2U); // TODO: tlist를 멤버로 하는 구조체 관리와 task 등록 구현
            break;
		}
		case LWS_CALLBACK_RAW_ADOPT:
		case LWS_CALLBACK_ESTABLISHED:
		{
			event = NS_EV::ACPT;

			switch_hash(lws_get_protocol(wsi)->name) {
				case_hash(WS_NAME):
					SA::prot_id(ses) = WS;
					break;
				case_hash(TCP_NAME):
					SA::prot_id(ses) = TCP;
					break;
			}
			SA::handler(ses) = static_cast<SessionEvHandler<T>*>(instance->handler);
			ses->user = new T();
			ses->group = INT_MIN; //reserved
			SA::last_act(ses) = now_ms();
			SA::tokens(ses) = RL_BURST_MAX; // 초기 접속 시 최대치 부여

			if (reason == LWS_CALLBACK_RAW_ADOPT) {
				instance->set_timeout(ses, wsi, TO_EV_PING_PONG); // set TCP ping-pong timer
			}

			instance->send_resv.add(ses, std::queue<std::vector<unsigned char>>());

			instance->session_group.add(ses->group, ses); 
			
			instance->wsi_map.add(ses, wsi);

			instance->log(_L_BLUE "[%14p] A Session is initialized.", (void*)ses);
			break;
		}
		case LWS_CALLBACK_RAW_RX:
		{
			// Do only length validation
			std::string acc(static_cast<const char*>(in), len);

			SA::last_act(ses) = now_ms();
			if (acc.size() >= 4) {
				uint32_t len = 0;
				try {
					len = std::stoul(acc.substr(0, 4), nullptr, 16);
				} catch (...) {
					throw runtime_errorf("Invalid frame header from Session %p", (void*)ses);
				}

				if (len > MAX_FRAME_SIZE) {
					throw runtime_errorf("Frame too large from Session %p", (void*)ses);
				} else if (len == 0) {
					return 0;
				} else if (len == 2) { // pong 0002{}
					instance->log(_L_BLUE "[%14p] Pong received.", (void*)ses);
					return 0;
				} else if (acc.size() < 4 + len) {
					return -1;
				}

				in_offset = 4;
			}
		}
		case LWS_CALLBACK_RECEIVE:
		{
			event = NS_EV::RECV;

			if (SA::tokens(ses) == RL_BURST_MAX)
				instance->set_timeout(ses, wsi, TO_EV_TOKEN_REFILL);
			else if (SA::tokens(ses) == 0) {
				event = NS_EV::RL_DROP;
				lws_rx_flow_control(wsi, 0);
				break;
			}
			
			SA::tokens(ses)--;

			instance->log(_L_BLUE "[%14p] Frame received: %zu bytes.", (void*)ses, len - in_offset);
			break;
		}
        case LWS_CALLBACK_RAW_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
		{
			std::vector<unsigned char> packet;
			bool more = false;
			instance->send_resv.task([ses, wsi, instance, &event, &packet, &more](auto& resv_map) {
				if (resv_map[ses].empty()) return;

				event = NS_EV::SEND;
				if (lws_partial_buffered(wsi)) {
					lws_callback_on_writable(wsi);
					instance->log(_L_BLUE "[%14p] Unsent data exist.", (void*)ses);
					return;
				}

				packet = std::move(resv_map[ses].front());
				resv_map[ses].pop();
				more = !resv_map[ses].empty();
			});
				
			int n;
			lws_write_protocol flag;
			switch (SA::prot_id(ses)) {
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
				instance->elog("[%14p] Try to send minus frame.", (void*)ses);
				return -1;
			}

			if (more) {
				instance->log(_L_YELLOW "[%14p] Send is deferred.", (void*)ses);
				lws_callback_on_writable(wsi);
			}
			instance->log(_L_BLUE "[%14p] Frame sent.", (void*)ses);
            break;
		}
		case LWS_CALLBACK_RAW_CLOSE:
		case LWS_CALLBACK_CLOSED:
		{
			event = NS_EV::CLOSE;
			break;
		}
		case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		{
			instance->fl_resv = false;
			instance->log(_L_YELLOW "Start side action.");
			std::map<Session*, std::string> dels;

			dels = instance->del_resv.move();
			
			if (!dels.empty()) {
				instance->log(_L_YELLOW "Close asynchronously: Sessions * %d.", (int)dels.size());
				for (auto [ses_to_close, msg]: dels) {
					lws* wsi_to_close;

					if (instance->wsi_map.get(ses_to_close, wsi_to_close) && SA::prot_id(ses_to_close) == WS) {
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
			instance->log(_L_BLUE "[%14p] Network connection detected.", (void*)ses);
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
			if (SA::to_flag(ses) & TO_EV_PING_PONG) {
				instance->check_pong(ses);
				instance->set_timeout(ses, wsi, TO_EV_PING_PONG);
			}
			if (SA::to_flag(ses) & TO_EV_TOKEN_REFILL) {
				SA::to_flag(ses) ^= TO_EV_TOKEN_REFILL;
				SA::tokens(ses) = RL_BURST_MAX;
				lws_rx_flow_control(wsi, 1);
			}
			break;
		}
		default:
			break;
	}	

	if (event == NS_EV::NONE)
		return 0;

	int ret = 0;
	if (ses) {
		handler = SA::handler(ses);
		ret = handler->callback({ .ses = ses, .event = event, .in = static_cast<unsigned char*>(in) + in_offset, .len = len - in_offset });
	}

	// Deferred cleanup
	if (event == NS_EV::CLOSE) {
		if (ses->user)
			delete ses->user;

		instance->send_resv.del(ses);
		
		instance->session_group.task([ses](auto& group_map) {
			if (group_map.find(ses->group) != group_map.end())
				group_map[ses->group].erase(ses);
		});

		instance->wsi_map.del(ses);

		instance->log(_L_BLUE "[%14p] A Session is closed.", (void*)ses);
	}

	return ret;
}
#pragma endregion