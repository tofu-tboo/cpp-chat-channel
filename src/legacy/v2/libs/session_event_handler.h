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
	type_protected:
		USING_TYPENAME(Session, NetworkService<T>);
		USING_TYPENAME(CallbackParam, NetworkService<T>);
	func_public:
		int callback(const CallbackParam&);
	func_protected:
		SessionEvHandler();

		virtual void on_accept(Session&) = 0;
		virtual void on_recv(Session&, const RecvStream&) = 0;
		virtual void on_send(Session&) = 0;
		virtual void on_close(Session&) = 0;
		virtual void on_rate_limit_packet_drop(Session&) = 0;
		virtual void pre_event(const CallbackParam&);
};

#include "session_event_handler.tpp"

#endif