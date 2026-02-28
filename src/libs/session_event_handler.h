#ifndef __SESSION_EVENT_HANDLER_H__
#define __SESSION_EVENT_HANDLER_H__

#include "loggable.h"
#include "class.h"
#include "network_service.h"

typedef struct {
	const unsigned char* data;
	size_t len;
} RecvStream;

template <typename T>
class SessionEvHandler: virtual public Loggable {
	func_public:
		int callback(const typename NetworkService<T>::CallbackParam&);
	func_protected:
		SessionEvHandler();

		virtual void on_accept(typename NetworkService<T>::Session&) = 0;
		virtual void on_recv(typename NetworkService<T>::Session&, const RecvStream&) = 0;
		virtual void on_send(typename NetworkService<T>::Session&) = 0;
		virtual void on_close(typename NetworkService<T>::Session&) = 0;
		virtual void on_rate_limit_packet_drop(typename NetworkService<T>::Session&) = 0;
		virtual void pre_event(const typename NetworkService<T>::CallbackParam&);
};

#include "session_event_handler.tpp"

#endif