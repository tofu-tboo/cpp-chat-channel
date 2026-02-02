
#include "session_event_handler.h"

template <typename T>
int SessionEvHandler<T>::callback(const LwsCallbackParam& param) {

	Connection connection = { .wsi = param.wsi, .prot_id = param.prot_id };
	T& user = translate(param);

	try {
		pre_event(param);

		switch (param.event) {
			case LwsCallbackParam::ACPT:
				on_accept(user);
				break;
			case LwsCallbackParam::RECV:
				on_recv(user, { .data = param.in, .len = param.len});
				break;
			case LwsCallbackParam::SEND:
				on_send(user);
				break;
			case LwsCallbackParam::CLOSE:
				on_close(user);
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
T& SessionEvHandler<T>::translate(const LwsCallbackParam& param) {
	// if (!param.user) return T(); // Should ensure param.user is valid in caller
	return *static_cast<T*>(param.user); // Return reference to persistent session object
}

template <typename T>
void SessionEvHandler<T>::pre_event(const LwsCallbackParam& param) {}