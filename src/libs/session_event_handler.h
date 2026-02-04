#ifndef __SESSION_EVENT_HANDLER_H__
#define __SESSION_EVENT_HANDLER_H__

#include "socket.h"

template <typename T>
class NetworkService;

typedef struct {
	const unsigned char* data;
	size_t len;
} RecvStream;

template <typename T>
class SessionEvHandler {
	private:
		int callback(const LwsCallbackParam&);
		friend class NetworkService<T>;
	public:
		virtual void on_accept(typename NetworkService<T>::Session&) = 0;
		virtual void on_recv(typename NetworkService<T>::Session&, const RecvStream&) = 0;
		virtual void on_send(typename NetworkService<T>::Session&) = 0;
		virtual void on_close(typename NetworkService<T>::Session&) = 0;
		virtual typename NetworkService<T>::Session& translate(const LwsCallbackParam&);
		virtual void pre_event(const LwsCallbackParam&);
};

#include "session_event_handler.tpp"

#endif