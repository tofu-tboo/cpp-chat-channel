
#include "session_event_handler.h"

template <typename T>
int SessionEvHandler<T>::callback(const LwsCallbackParam& param) {

	// Connection connection = { .wsi = param.wsi, .prot_id = param.prot_id };
	typename NetworkService<T>::Session& ses = translate(param);

	try {
		pre_event(param);

		switch (param.event) {
			case LwsCallbackParam::ACPT:
				on_accept(ses);
				break;
			case LwsCallbackParam::RECV:
				on_recv(ses, { .data = param.in, .len = param.len});
				break;
			case LwsCallbackParam::SEND:
				on_send(ses);
				break;
			case LwsCallbackParam::CLOSE:
				on_close(ses);
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
typename NetworkService<T>::Session& SessionEvHandler<T>::translate(const LwsCallbackParam& param) {
	// if (!param.user) return T(); // Should ensure param.user is valid in caller
	return *static_cast<typename NetworkService<T>::Session*>(param.user); // Return reference to persistent session object
}

template <typename T>
void SessionEvHandler<T>::pre_event(const LwsCallbackParam& param) {}