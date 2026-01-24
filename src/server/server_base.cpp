#include "server_base.h"

fd_t ServerBase::fd = -1;
std::unordered_map<fd_t, std::string> ServerBase::name_map;
std::mutex ServerBase::name_map_mtx;

ServerBase::ServerBase(const int max_fd, const msec to): con_tracker(nullptr), comm(nullptr), timeout(to) {
    try {
        branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

        if (fd == FD_ERR)
            set_network();

        LOG(_CG_ "Server initialized on port 4800." _EC_);

        con_tracker = new ConnectionTracker(fd, max_fd);
        if (!con_tracker)
            throw std::runtime_error("Failed to allocate Connection Tracker.");
        con_tracker->init();

		comm = new Communication();

        task_runner.new_session(3);
		// Cleanup Qs
        task_runner.pushb(0, [this]() {
            next_deletion.clear();
        });
		// Polling
        task_runner.pushb(1, [this]() {
            con_tracker->polling(timeout);
        });
		// Handle Events
		task_runner.pushb(1, [this]() {
			const pollev* events = con_tracker->get_ev();
			const int evcnt = con_tracker->get_evcnt();

			for (int i = 0; i < evcnt; i++) {
				handle_events(events[i]);
			}
		});
		// Deletion fds
        task_runner.pushb(2, [this]() {
            resolve_deletion();
        });
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
    }
}

ServerBase::~ServerBase() {
    if (con_tracker)
        delete con_tracker;
	
	if (comm)
		delete comm;

    if (fd != FD_ERR) { // close listening socket
        close(fd);
        fd = FD_ERR;
    }

    next_deletion.clear();
}

void ServerBase::proc() {
    while (1) {
        try {
            task_runner.run();
            // frame();
        } catch(const std::exception& e) {
            iERROR("%s", e.what());
        }
    }
}

bool ServerBase::get_user_name(const fd_t fd, std::string& out_user_name) const {
	std::lock_guard<std::mutex> lock(name_map_mtx);
	auto it = name_map.find(fd);
	if (it == name_map.end()) {
		return false;
	}
	out_user_name = it->second;
	return true;
}

void ServerBase::set_user_name(const fd_t fd, const std::string& user_name) {
	std::lock_guard<std::mutex> lock(name_map_mtx);
	name_map[fd] = user_name;
}

void ServerBase::remove_user_name(const fd_t fd) {
	std::lock_guard<std::mutex> lock(name_map_mtx);
	name_map.erase(fd);
}

#pragma region PRIVATE_FUNC
void ServerBase::set_network() {
    if (fd != FD_ERR) {
        throw std::runtime_error("Sever descriptor is already assigned.");
    }

    sAddrInfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "4800", &hints, &res);
    if (status != 0) {
        throw std::runtime_error("The getaddrinfo() is not resolved.");
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == FD_ERR) {
        throw std::runtime_error("Failed to get socket fd.");
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(fd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        throw std::runtime_error("Failed to bind server.");
    }

    status = listen(fd, 5);
    if (status == -1) {
        throw std::runtime_error("Failed to set listen().");
    }

    freeaddrinfo(res);
}

void ServerBase::handle_events(const pollev event) {
    fd_t fd = event.data.fd;
    uint32_t evs = event.events;

    if (fd == ServerBase::fd) {
        on_accept();
    } else if (evs & (EPOLLHUP | EPOLLERR)) {
		on_disconnect(fd);
	} else if (evs & EPOLLIN) {
        on_recv(fd);
    }
    // 필요하면 EPOLLOUT도 처리
}
#pragma endregion

#pragma region PROTECTED_FUNC

// void ServerBase::frame() {
//     task_runner.run();
// }

void ServerBase::resolve_deletion() {
	if (!con_tracker) return;
    for (const fd_t fd : next_deletion) {
		try {
        	con_tracker->delete_client(fd);
		} catch (...) {
			continue;
		}
		remove_user_name(fd);
        close(fd);
		comm->clear_buffer(fd);
        LOG("Normally Disconnected: fd %d", fd);
    }
}

void ServerBase::on_req(const fd_t from, const char* target, Json& root) {
    switch (hash(target))
    {
    // case hash("message"):
    //     mq.push_back(payload);
    //     break;
    
    default:
        break;
    }
}

void ServerBase::on_accept() {
	if (!con_tracker) return;
    fd_t client = accept(ServerBase::fd, nullptr, nullptr);
    if (client == FD_ERR) {
        iERROR("Failed to accept new connection.");
    } else {
        LOG("Accepted new connection: fd %d", client);
        try {
            con_tracker->add_client(client);
			set_user_name(client, "user_" + std::to_string(client)); // temporary username assignment
		} catch (const std::exception& e) {
            iERROR("%s", e.what());
            con_tracker->delete_client(client);
            close(client);
        }
    }
}

void ServerBase::on_disconnect(const fd_t fd) {
	next_deletion.push_back(fd);
}

void ServerBase::on_recv(const fd_t from) {
	try {
		if (!comm) return;
		std::vector<std::string> frames = comm->recv_frame(from);
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
        next_deletion.push_back(from);
    }
}

#pragma endregion
