
#include "session_event_handler.h"

template <typename T>
SessionEvHandler<T>::SessionEvHandler(): Loggable("SessionEvHandler", _L_GREEN, this) {}

template <typename T>
int SessionEvHandler<T>::callback(const typename NetworkService<T>::CallbackParam& param) {
	typename NetworkService<T>::Session& ses = *(param.ses);

	try {
		pre_event(param);

		switch (param.event) {
			case NS_EV::ACPT:
				on_accept(ses);
				break;
			case NS_EV::RECV:
				on_recv(ses, { .data = param.in, .len = param.len});
				break;
			case NS_EV::SEND:
				on_send(ses);
				break;
			case NS_EV::CLOSE:
				on_close(ses);
				break;
			case NS_EV::RL_DROP:
				on_rate_limit_packet_drop(ses);
				break;
			default:
				break;
		}
	} catch (std::exception& e) {
		elog("Exception in event callback: %s", e.what());
		return -1;
	}
	return 0;
}

template <typename T>
void SessionEvHandler<T>::pre_event(const typename NetworkService<T>::CallbackParam& param) {}