#include "cron_worker.h"
#include <vector>

void CronWorker::schedule_at(const UnivKey& key, msec64 exec_time, std::function<void()> func) {
	std::lock_guard<std::mutex> lock(mtx);
	// Remove existing task with the same key to ensure uniqueness/update
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == key) {
			tasks.erase(it);
			break;
		}
	}
	tasks.emplace(exec_time, CronTask{key, std::move(func), 0});
}

UnivKey CronWorker::schedule_at(msec64 exec_time, std::function<void()> func) {
	UnivKey id = next_task_id++;
	std::lock_guard<std::mutex> lock(mtx);
	tasks.emplace(exec_time, CronTask{id, std::move(func), 0});
	return id;
}

void CronWorker::schedule_in(const UnivKey& key, msec64 delay, std::function<void()> func) {
	schedule_at(key, now_ms() + delay, std::move(func));
}

UnivKey CronWorker::schedule_in(msec64 delay, std::function<void()> func) {
	return schedule_at(now_ms() + delay, std::move(func));
}

void CronWorker::schedule_every(const UnivKey& key, msec64 interval, std::function<void()> func, bool run_immediately) {
	msec64 first_exec_time = now_ms() + (run_immediately ? 0 : interval);
	std::lock_guard<std::mutex> lock(mtx);
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == key) {
			tasks.erase(it);
			break;
		}
	}
	tasks.emplace(first_exec_time, CronTask{key, std::move(func), interval});
}

UnivKey CronWorker::schedule_every(msec64 interval, std::function<void()> func, bool run_immediately) {
	UnivKey id = next_task_id++;
	msec64 first_exec_time = now_ms() + (run_immediately ? 0 : interval);
	std::lock_guard<std::mutex> lock(mtx);
	tasks.emplace(first_exec_time, CronTask{id, std::move(func), interval});
	return id;
}

void CronWorker::cancel(const UnivKey& id) {
	std::lock_guard<std::mutex> lock(mtx);
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == id) {
			tasks.erase(it);
			return;
		}
	}
}

void CronWorker::run_pending() {
	std::vector<CronTask> tasks_to_run;
	msec64 now = now_ms();

	{
		std::lock_guard<std::mutex> lock(mtx);
		// Find all tasks that are due
		auto end_it = tasks.upper_bound(now);
		for (auto it = tasks.begin(); it != end_it; ++it) {
			tasks_to_run.push_back(it->second);
		}
		// Remove due tasks from the main list
		tasks.erase(tasks.begin(), end_it);
	}

	// Execute tasks without holding the lock
	for (const auto& task : tasks_to_run) {
		if (task.func) {
			try {
				task.func();
			} catch (...) {
				// It's good practice to catch exceptions from user-provided tasks
			}
		}
		// Reschedule if it's a periodic task
		if (task.interval > 0) {
			std::lock_guard<std::mutex> lock(mtx);
			tasks.emplace(now + task.interval, task);
		}
	}
}
