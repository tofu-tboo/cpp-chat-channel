#include "network_service.h"
#include "session_event_handler.h"
#include "exception.h"

template <typename T>
NetworkService<T>::NetworkService(): Loggable("NetworkService", _L_GREEN, this) {}

template <typename T>
NetworkService<T>::~NetworkService() {
	if (thread_pool)
		delete thread_pool;
}

#pragma region PUBLIC_FUNC
template <typename T>
void NetworkService<T>::setup(SessionEvHandler<T>* i_handler, const std::function<void()>& task) {
	handler = i_handler;
	thread_pool->start(task);
}

template <typename T>
void NetworkService<T>::send(Session* ses, const std::string& msg) {
	send(ses, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::broadcast(const std::string& msg) {
	broadcast(reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::broadcast_group(int group, const std::string& msg) {
	broadcast_group(group, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::close(Session* ses, const std::string& msg) {
	close(ses, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size());
}

template <typename T>
void NetworkService<T>::threads_join() {
	thread_pool->join();
}

template <typename T>
void NetworkService<T>::change_session_group(Session* ses, int new_group) {
    if (!ses) return;

	session_group.task([ses, new_group](auto& session_group) {
		// Remove from old group if it exists
		auto old_group_it = session_group.find(ses->group);
		if (old_group_it != session_group.end()) {
			old_group_it->second.erase(ses);
		}

		// Add to new group
		ses->group = new_group;
		session_group[new_group].insert(ses);
	});

}

template <typename T>
void NetworkService<T>::register_handler(Session* ses, SessionEvHandler<T>* handler) {
	ses->secret->handler = handler;
}

#pragma endregion
#pragma region PRIVATE_FUNC
template <typename T>
void NetworkService<T>::construct_session(Session* ses) {
	if (!ses) return;
	if (!ses->secret)
		ses->secret = new SessionSecret();
	if (!ses->user)
		ses->user = new T();
}

template <typename T>
void NetworkService<T>::destruct_session(Session* ses) {
	if (!ses) return;
	if (ses->secret)
		delete ses->secret;
	if (ses->user)
		delete ses->user;
}

#pragma endregion