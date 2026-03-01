#include "lws_service.h"
#include "times.h"
#include "hash.h"
#include "exception.h"
#include <vector>

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
thread_local int LwsService<T>::tsi = 0;

template <typename T>
LwsService<T>::LwsService(const port_t port, const size_t tcnt): context(nullptr), fl_resv(false), Loggable("LwsService", _L_GREEN, this) {
	thread_pool = new ThreadPool<T, "lws">(tcnt);

	// Set context info
	memset(&info, 0, sizeof(info));
	info.port = port;
	info.protocols = LwsService<T>::protocols;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW | LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS; // TCP & WS compatibility
	info.timeout_secs = 15; // WS handshake timeout
	// info.fd_limit_per_thread = int;
	info.count_threads = thread_pool->get_size();
	info.user = this;

	// Set sul wrapper
	tlist_wrapper.service = this;
	memset(&tlist_wrapper.sul, 0, sizeof(utimer_list_t));
}

template <typename T>
LwsService<T>::~LwsService() {
	if (context) {
		lws_context_destroy(context);
		context = nullptr;
	}

	lws_sul_cancel(&tlist_wrapper.sul); // stack safe
}

#pragma region PUBLIC_FUNC
template <typename T>
void LwsService<T>::setup(SessionEvHandler<T>* i_handler, const std::function<void()>& task) {
	if (context) return;
	
	context = lws_create_context(&info);
	if (!context) throw runtime_errorf("Failed to create context.");

	int supported = lws_get_count_threads(context);
	if (supported < (int)info.count_threads) {
		throw runtime_errorf("LWS context supports %d threads, but %d requested. Rebuild libwebsockets with LWS_MAX_SMP > 1.", supported, info.count_threads);
	}
	
	super::setup(i_handler, task);
}

template <typename T>
void LwsService<T>::serve() {
	lws_service_tsi(context, 10, tsi);
}

template <typename T>
void LwsService<T>::send(Session* ses, const unsigned char* data, size_t len) {
	if (!ses) {
		elog("Null Session");
		return;
	}
	LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
	lws* wsi = extra->wsi;
	if (!wsi) {
		elog("Null wsi");
		return;
	}

	accumulate(ses, data, len);
	lws_callback_on_writable(wsi);
	flush();
}

template <typename T>
void LwsService<T>::broadcast(const unsigned char* data, size_t len) {
	session_group.task([this, data, len](auto& ses_group) {
		for (auto& [group, sessions] : ses_group) {
			for (auto ses : sessions) {
				LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
				lws* wsi = extra->wsi;
				if (!wsi) {
					elog("Null wsi");
					return;
				}

				accumulate(ses, data, len);
				lws_callback_on_writable(wsi);
			}
		}
	});
	flush();
}

template <typename T>
void LwsService<T>::broadcast_group(int group, const unsigned char* data, size_t len) {
	session_group.task([this, group, data, len](auto& ses_group) {
		for (auto ses: ses_group[group]) {
			LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
			lws* wsi = extra->wsi;
			if (!wsi) {
				elog("Null wsi");
				return;
			}

			accumulate(ses, data, len);
			lws_callback_on_writable(wsi);
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
#pragma region PROTECTED_FUNC
template <typename T>
void LwsService<T>::accumulate(Session* ses, const unsigned char* data, size_t len) {
	if (!ses) throw runtime_errorf("Null session.");
	else if (len > MAX_FRAME_SIZE) throw runtime_errorf("Frame too large.");

	short extra = ses->secret->prot_id == TCP ? 4 : 0;
	std::vector<unsigned char> packet(LWS_PRE + len + extra);

	if (ses->secret->prot_id == TCP) {
		char header[5];
		snprintf(header, sizeof(header), "%04x", (unsigned int)len);
		memcpy(&packet[LWS_PRE], header, 4);
	}

    if (len > 0) {
		memcpy(&packet[LWS_PRE + extra], data, len);
    }

	send_resv.add(ses, std::move(packet));
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
	LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
	
	if (ses && ses->secret->prot_id == TCP && extra->last_act) {
		if (now - extra->last_act > 10 * S2M) {
			close(ses, std::string(""));
			log(_L_BLUE "Failed ping-pong of raw TCP Session (%p).", (void*)ses);
		}
		else if (now - extra->last_act > 5 * S2M) {
			send(ses, std::string("-")); // ping
			log(_L_BLUE "Ping raw TCP Session (%p).", (void*)ses);
		}
	}
}

template <typename T>
__CALLBACK_SAFE__ void LwsService<T>::set_timeout(Session* ses, lws* wsi, int flag) {
	if (ses) {
		LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
		
		extra->to_flag = flag;
		if (flag & TO_EV_PING_PONG)
			lws_set_timer_usecs(wsi, S2U);
	}
}

template <typename T>
__CALLBACK_SAFE__ std::string LwsService<T>::get_ip(lws* wsi) {
	char buf[64];
	const char* ret = lws_get_peer_simple(wsi, buf, sizeof(buf));
	return ret ? std::string(ret) : "";
}

template <typename T>
__CALLBACK_SAFE__ void LwsService<T>::quit_service(utimer_list_t* sul) {
	SulWrapper* wrapper = lws_container_of(sul, SulWrapper, sul);
	LwsService<T>* service = wrapper->service;

	service->flush();

	reserve_quit(service);
}

template <typename T>
__CALLBACK_SAFE__ void LwsService<T>::reserve_quit(LwsService<T>* service) {
	lws_sul_schedule(service->context, service->tlist_wrapper.tsi, &service->tlist_wrapper.sul, quit_service, 500 * M2U);
}

template <typename T>
int LwsService<T>::lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	if (!wsi) return 0;
	ctx* context = lws_get_context(wsi);
	if (!context) return 0;
	LwsService<T>* instance = static_cast<LwsService<T>*>(lws_context_user(context));
	if (!instance) return 0;

	Session* ses = static_cast<Session*>(session);
	SessionEvHandler<T>* handler;
	NS_EV event = NS_EV::NONE;

	int in_offset = 0;

	switch (reason) {
		case LWS_CALLBACK_PROTOCOL_INIT:
		{
			instance->log(_L_YELLOW "Quit timer is reserved.");
			int tsi = lws_get_tsi(wsi);
			if (tsi == 0) {
				instance->tlist_wrapper.tsi = tsi;
				reserve_quit(instance);
			}
            break;
		}
		case LWS_CALLBACK_RAW_ADOPT:
		case LWS_CALLBACK_ESTABLISHED:
		{
			instance->log("acc");
			event = NS_EV::ACPT;

			std::string ip = instance->get_ip(wsi);
			int current_conn = 0;
			instance->log("get ip");
			instance->ip_conn_map.get(ip, current_conn);
			instance->log(_L_BLUE "[%14p] Network connection detected at %s.", (void*)ses, ip.c_str());

			if (current_conn >= MAX_ALLOWED_WSI_PER_IP) {
				instance->log(_L_YELLOW "[%14p] IP connection limits.", (void*)ses);
				return -1;
			}

			new (ses) Session(instance);

			ses->secret->ip = ip;
			instance->ip_conn_map.task([ip](auto& map) {
				map[ip]++;
			});

			switch_hash(lws_get_protocol(wsi)->name) {
				case_hash(WS_NAME):
					ses->secret->prot_id = WS;
					break;
				case_hash(TCP_NAME):
					ses->secret->prot_id = TCP;
					break;
			}

			LwsSession* new_extra = new LwsSession();
			new_extra->wsi = wsi;
			ses->secret->extra = new_extra;

			if (reason == LWS_CALLBACK_RAW_ADOPT) {
				instance->set_timeout(ses, wsi, TO_EV_PING_PONG); // set TCP ping-pong timer
			}

			instance->send_resv.add(ses, std::queue<std::vector<unsigned char>>());

			instance->session_group.add(ses->group, ses); 
			

			instance->log(_L_BLUE "[%14p] A Session is initialized.", (void*)ses);
			break;
		}
		case LWS_CALLBACK_RAW_RX:
		{
			// Do only length validation
			std::string acc(static_cast<const char*>(in), len);

			LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
			extra->last_act = now_ms();
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

			if (ses->secret->tokens == RL_BURST_MAX)
				instance->set_timeout(ses, wsi, TO_EV_TOKEN_REFILL);
			else if (ses->secret->tokens == 0) {
				event = NS_EV::RL_DROP;
				lws_rx_flow_control(wsi, 0);
				break;
			}
			
			ses->secret->tokens--;

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
			switch (ses->secret->prot_id) {
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
			std::map<Session*, std::string> dels;

			dels = instance->del_resv.move();
			
			if (!dels.empty()) {
				instance->log(_L_YELLOW "Close asynchronously: Sessions * %d.", (int)dels.size());
				for (auto [ses_to_close, msg]: dels) {
					LwsSession* extra = static_cast<LwsSession*>(ses_to_close->secret->extra);
					lws* wsi_to_close = extra->wsi;

					if (wsi_to_close && ses_to_close->secret->prot_id == WS) {
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
			
			break;
		}
		case LWS_CALLBACK_TIMER:
		{
			LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
			if (extra->to_flag & TO_EV_PING_PONG) {
				instance->check_pong(ses);
				instance->set_timeout(ses, wsi, TO_EV_PING_PONG);
			}
			if (extra->to_flag & TO_EV_TOKEN_REFILL) {
				extra->to_flag ^= TO_EV_TOKEN_REFILL;
				ses->secret->tokens = RL_BURST_MAX;
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
	if (ses && ses->secret) {
		handler = ses->secret->handler;
		ret = handler->callback({ .ses = ses, .event = event, .in = static_cast<unsigned char*>(in) + in_offset, .len = len - in_offset });

		// Deferred cleanup
		if (event == NS_EV::CLOSE) {
			LwsSession* extra = static_cast<LwsSession*>(ses->secret->extra);
			if (extra)
				delete extra;
			
			if (!ses->secret->ip.empty()) {
				instance->ip_conn_map.task([ip = ses->secret->ip](auto& map) {
					if (map.find(ip) != map.end()) {
						map[ip]--;
						if (map[ip] <= 0) map.erase(ip);
					}
				});
			}
			ses->~Session();

			instance->send_resv.del(ses);
			
			instance->session_group.task([ses](auto& group_map) {
				if (group_map.find(ses->group) != group_map.end())
					group_map[ses->group].erase(ses);
			});

			instance->log(_L_BLUE "[%14p] A Session is closed.", (void*)ses);
		}
	}

	return ret;
}
#pragma endregion