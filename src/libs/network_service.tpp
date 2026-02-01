#include "network_service.h"


template <typename T>
protocols_t NetworkService<T>::protocols[] = {
	{
		"ws",
		NetworkService<T>::lws_callback,
		sizeof(typename NetworkService<T>::Session),
		2048,
	},
	{
		"tcp",
		NetworkService<T>::lws_callback,
		sizeof(typename NetworkService<T>::Session),
		2048,
	},
	{ NULL, NULL, 0, 0 }
};


template <typename T>
NetworkService<T>::NetworkService(const int port): context(nullptr) {
	memset(&info, 0, sizeof(info));
	ctx_port = port;
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
	info.port = ctx_port;
	info.protocols = NetworkService<T>::protocols;
	info.user = i_handler;
	info.options = LWS_SERVER_OPTION_FALLBACK_TO_RAW;
	context = lws_create_context(&info);
	if (!context) throw runtime_errorf("Failed to create context.");
}

template <typename T>
void NetworkService<T>::serve(const msec to) {
	lws_service(context, to);
}

// template <typename T>
// ctx* NetworkService<T>::get_ctx() const {
// 	return context;
// }

template <typename T>
void NetworkService<T>::send(lws* wsi, const std::string& msg) {
    send(wsi, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::send(lws* wsi, const unsigned char* data, size_t len) {
	accumulate(wsi, data, len);

    lws_callback_on_writable(wsi);
    lws_cancel_service(context);
}

template <typename T>
void NetworkService<T>::broadcast(std::string& msg) {
	broadcast(reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::broadcast(const unsigned char* data, size_t len) {
    for (auto wsi : this->session_inst) {
		accumulate(wsi, data, len);
        lws_callback_on_writable(wsi);
    }
    lws_cancel_service(context);
}
#pragma region PRIVATE_FUNC
template <typename T>
void NetworkService<T>::accumulate(lws* wsi, const unsigned char* data, size_t len) {
	typename NetworkService<T>::Session* ses = static_cast<typename NetworkService<T>::Session*>(lws_wsi_user(wsi));
    if (!ses || !ses->buf) throw runtime_errorf("Invalid session.");

	std::vector<unsigned char> packet(LWS_PRE + len);
    if (len > 0) {
        memcpy(&packet[LWS_PRE], data, len);
    }

    std::lock_guard<std::mutex> lock(*ses->mtx);
    ses->buf->push(std::move(packet));
}

template <typename T>
int NetworkService<T>::lws_callback(lws* wsi, callback_reason reason, void* session, void* in, size_t len) {
	typename NetworkService<T>::Session* ses = static_cast<typename NetworkService<T>::Session*>(session);
	SessionEvHandler<T>* handler;
	decltype(LwsCallbackParam::event) event = LwsCallbackParam::NONE;
	NetworkService* instance = static_cast<NetworkService*>(lws_context_user(lws_get_context(wsi)));

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
				ses->handler = static_cast<SessionEvHandler<T>*>(lws_context_user(lws_get_context(wsi)));
				ses->buf = new std::queue<std::vector<unsigned char>>();
            	ses->mtx = new std::mutex();

				std::unique_lock<std::shared_mutex> lock(instance->mtx);
				instance->session_inst.insert(wsi);
			}
			break;
		case LWS_CALLBACK_RECEIVE:
		case LWS_CALLBACK_RAW_RX:
			event = LwsCallbackParam::RECV;
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
        case LWS_CALLBACK_RAW_WRITEABLE:
			{
				if (ses && !ses->buf->empty()) {
					event = LwsCallbackParam::SEND;
				
					if (lws_partial_buffered(wsi)) {
						lws_callback_on_writable(wsi);
						break;
					}

					std::vector<unsigned char> packet;
					{
						std::lock_guard<std::mutex> lock(*ses->mtx);
						packet = std::move(ses->buf->front());
						ses->buf->pop();
					}
					
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

					if (n < 0) return -1;
					
					if (!ses->buf->empty()) {
						lws_callback_on_writable(wsi);
					}
				}
			}
            break;
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_RAW_CLOSE:
			{
				if (ses) {
					event = LwsCallbackParam::CLOSE;

					delete ses->buf;
					delete ses->mtx;
					ses->buf = nullptr;
					ses->mtx = nullptr;

					std::unique_lock<std::shared_mutex> lock(instance->mtx);
					instance->session_inst.erase(wsi);
				}
			}
			break;
		default:
			break;
	}	

	if (event == LwsCallbackParam::NONE)
		return -1;

	handler = ses->handler;

	if (handler) {
		return handler->callback({ .wsi = wsi, .event = event, .user = (ses ? &ses->user : nullptr), .in = static_cast<unsigned char*>(in), .len = len, .prot_id = ses->prot_id }); // uncapsulate user
	}
	return 0;
}
#pragma endregion