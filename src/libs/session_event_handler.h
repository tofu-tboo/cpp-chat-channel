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
		virtual void on_accept(const T&, const Connection&) = 0;
		virtual void on_recv(const T&, const Connection&, const RecvStream&) = 0;
		virtual void on_send(const T&, const Connection&) = 0;
		virtual void on_close(const T&, const Connection&) = 0;
		virtual T translate(const LwsCallbackParam&);
		virtual void pre_event(const LwsCallbackParam&);
};

#include "session_event_handler.tpp"

#endif