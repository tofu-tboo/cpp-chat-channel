
#include "session_event_handler.h"

template <typename T>
int SessionEvHandler<T>::callback(const LwsCallbackParam& param) {

	Connection connection = { .wsi = param.wsi, .prot_id = param.prot_id };
	T user = translate(param);

	try {
		pre_event(param);

		switch (param.event) {
			case LwsCallbackParam::ACPT:
				on_accept(std::move(user), connection);
				break;
			case LwsCallbackParam::RECV:
				on_recv(std::move(user), connection, { .data = param.in, .len = param.len});
				break;
			case LwsCallbackParam::SEND:
				on_send(std::move(user), connection);
				break;
			case LwsCallbackParam::CLOSE:
				on_close(std::move(user), connection);
				break;
			default:
				break;
		}
	} catch (...) {
		return -1;
	}

	return 0;
}

template <typename T>
T SessionEvHandler<T>::translate(const LwsCallbackParam& param) {
	if (!param.user) return T();
	return *static_cast<T*>(param.user);
}

template <typename T>
void SessionEvHandler<T>::pre_event(const LwsCallbackParam& param) {}