#include <climits>
#include <new>
#include "network_service.h"
#include "session_event_handler.h"
#include "exception.h"

template <typename T>
NetworkService<T>::NetworkService(): Loggable("NetworkService", _L_GREEN, this) {}

template <typename T>
NetworkService<T>::~NetworkService() {}

#pragma region PUBLIC_FUNC
template <typename T>
void NetworkService<T>::setup(SessionEvHandler<T>* i_handler) {
	handler = i_handler;
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
	ses->handler = handler;
}

#pragma endregion
#pragma region PRIVATE_FUNC
template <typename T>
void NetworkService<T>::accumulate(Session* ses, const unsigned char* data, size_t len) {
	if (!ses) throw runtime_errorf("Null session.");
	else if (len > MAX_FRAME_SIZE) throw runtime_errorf("Frame too large.");

	short extra = ses->prot_id == TCP ? 4 : 0;
	std::vector<unsigned char> packet(LWS_PRE + len + extra);

	if (ses->prot_id == TCP) {
		char header[5];
		snprintf(header, sizeof(header), "%04x", (unsigned int)len);
		memcpy(&packet[LWS_PRE], header, 4);
	}

    if (len > 0) {
		memcpy(&packet[LWS_PRE + extra], data, len);
    }

	send_resv.add(ses, std::move(packet));
}