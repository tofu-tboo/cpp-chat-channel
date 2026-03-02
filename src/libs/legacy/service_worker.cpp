#include "service_worker.h"
#include <shared_mutex>

ServiceWorker::ServiceWorker() {}
ServiceWorker::~ServiceWorker() {}

void ServiceWorker::add(Type type, const UnivKey& key, Task task) {
	switch (type) {
		case PRE:
			pre_poll.push_back(key, std::move(task));
			break;
		case POST:
			post_poll.push_back(key, std::move(task));
			break;
		case BG:
			bg_proc.push_back(key, std::move(task));
			break;
	}
}

void ServiceWorker::add(Type type, Task task) {
	switch (type) {
		case PRE:
			pre_poll.push_back(std::move(task));
			break;
		case POST:
			post_poll.push_back(std::move(task));
			break;
		case BG:
			bg_proc.push_back(std::move(task));
			break;
	}
}

void ServiceWorker::remove(Type type, const UnivKey& key) {
	switch (type) {
		case PRE:
			pre_poll.remove(key);
			break;
		case POST:
			post_poll.remove(key);
			break;
		case BG:
			bg_proc.remove(key);
			break;
	}
}

void ServiceWorker::run(Type type) {
	KeyList<Task>* target = nullptr;
	switch (type) {
		case PRE: target = &pre_poll; break;
		case POST: target = &post_poll; break;
		case BG: target = &bg_proc; break;
	}

	if (target) {
		std::shared_lock lock(target->get_mutex());
		for (auto& [key, task] : *target) {
			task();
		}
	}
}

void ServiceWorker::clear(Type type) {
	KeyList<Task>* target = nullptr;
	switch (type) {
		case PRE: target = &pre_poll; break;
		case POST: target = &post_poll; break;
		case BG: target = &bg_proc; break;
	}

	if (target) {
		while (!target->empty()) {
			target->pop_front();
		}
	}
}
