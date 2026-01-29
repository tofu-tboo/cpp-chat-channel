#include "server_base.h"

fd_t ServerBase::fd = -1;

ServerBase::ServerBase(const char* port, const int max_fd, const msec to): con_tracker(nullptr), comm(nullptr), timeout(to), is_running(true) {
    try {
        branch_id = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

        if (fd == FD_ERR) {
			if (port == nullptr)
				port = "4800\0\0";
            set_network(port);
		}

        LOG(_CG_ "Server initialized on port %s." _EC_, port);

        con_tracker = new ConnectionTracker(fd, max_fd);
        if (!con_tracker)
            throw std::runtime_error("Failed to allocate Connection Tracker.");
        con_tracker->init();

		comm = new Communication();

        task_runner.new_session(TS_COUNT);
		// Cleanup Qs
        task_runner.pushb(TS_PRE, [this]() {
            next_deletion.clear();
        });
		// Polling
        task_runner.pushb(TS_POLL, [this]() {
            con_tracker->polling(timeout);
        });
		// Handle Events
		task_runner.pushb(TS_POLL, [this]() {
			const pollev* events = con_tracker->get_ev();
			const int evcnt = con_tracker->get_evcnt();

			for (int i = 0; i < evcnt; i++) {
				handle_events(events[i]);
			}
		});
		// Deletion fds
        task_runner.pushb(TS_LOGIC, [this]() {
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
    while (is_running) {
        try {
            task_runner.run();
            // frame();
        } catch(const std::exception& e) {
            iERROR("%s", e.what());
        }
    }
}

void ServerBase::stop() {
    is_running = false;
}

#pragma region PRIVATE_FUNC
void ServerBase::set_network(const char* port) {
    if (fd != FD_ERR) {
        throw std::runtime_error("Sever descriptor is already assigned.");
    }

    sAddrInfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, port, &hints, &res);
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
		if (!con_tracker) return;
		fd_t client = accept(ServerBase::fd, nullptr, nullptr);
		if (client == FD_ERR) {
			iERROR("Failed to accept new connection.");
		} else {
			on_accept(client);
			LOG("Accepted new connection: fd %d", client);
		}
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
        close(fd);
		comm->clear_buffer(fd);
        LOG("Normally Disconnected: fd %d", fd);
    }
}

void ServerBase::on_frame(const fd_t from, const std::string& frame) {
    // Default implementation does nothing
}

void ServerBase::on_accept(const fd_t client) {
	try {
        con_tracker->add_client(client);
	} catch (const std::exception& e) {
		if (const auto* cre = try_get_coded_error(e)) {
			if (cre->code == POOL_FULL) {
				comm->send_frame(client, std::string(R"({"type":"error","message":"Server is full."})"));
			}
		}
        iERROR("%s", e.what());
        next_deletion.insert(client);
		return;
    }
}

void ServerBase::on_disconnect(const fd_t fd) {
	next_deletion.insert(fd);
}

void ServerBase::on_recv(const fd_t from) {
	try {
		if (!comm) return;
		std::vector<std::string> frames = comm->recv_frame(from);
		for (const std::string& frame : frames) {
            on_frame(from, frame);
        }
    } catch (const std::exception& e) {
        iERROR("%s", e.what());
        next_deletion.insert(from);
    }
}

#pragma endregion
