#include "cron_worker.h"
#include <vector>
#include <limits>

void CronWorker::schedule_at(const UnivKey& key, msec64 exec_time, std::function<void()> func) {
	if (stopped_) return;
	std::lock_guard lock(mtx);
	// Remove existing task with the same key to ensure uniqueness/update
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == key) {
			tasks.erase(it);
			break;
		}
	}
	tasks.emplace(exec_time, CronTask{key, std::move(func), 0});
	notify();
}

UnivKey CronWorker::schedule_at(msec64 exec_time, std::function<void()> func) {
	if (stopped_) return -1;
	UnivKey id = next_task_id++;
	std::lock_guard lock(mtx);
	tasks.emplace(exec_time, CronTask{id, std::move(func), 0});
	notify();
	return id;
}

void CronWorker::schedule_in(const UnivKey& key, msec64 delay, std::function<void()> func) {
	schedule_at(key, now_ms() + delay, std::move(func));
}

UnivKey CronWorker::schedule_in(msec64 delay, std::function<void()> func) {
	return schedule_at(now_ms() + delay, std::move(func));
}

void CronWorker::schedule_every(const UnivKey& key, msec64 interval, std::function<void()> func, bool run_immediately) {
	if (stopped_) return;
	msec64 first_exec_time = now_ms() + (run_immediately ? 0 : interval);
	std::lock_guard lock(mtx);
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == key) {
			tasks.erase(it);
			break;
		}
	}
	tasks.emplace(first_exec_time, CronTask{key, std::move(func), interval});
	notify();
}

UnivKey CronWorker::schedule_every(msec64 interval, std::function<void()> func, bool run_immediately) {
	if (stopped_) return -1;
	UnivKey id = next_task_id++;
	msec64 first_exec_time = now_ms() + (run_immediately ? 0 : interval);
	std::lock_guard lock(mtx);
	tasks.emplace(first_exec_time, CronTask{id, std::move(func), interval});
	notify();
	return id;
}

void CronWorker::cancel(const UnivKey& id) {
	if (stopped_) return;
	std::lock_guard lock(mtx);
	for (auto it = tasks.begin(); it != tasks.end(); ++it) {
		if (it->second.id == id) {
			tasks.erase(it);
			return;
		}
	}
}

msec64 CronWorker::get_next_tick_duration_unsafe() const {
	if (tasks.empty()) {
		// No tasks, so we can wait for a very long time.
		return std::numeric_limits<msec64>::max();
	}

	msec64 next_exec_time = tasks.begin()->first;
	msec64 now = now_ms();

	return (next_exec_time <= now) ? 0 : next_exec_time - now;
}
msec64 CronWorker::get_next_tick_duration() const {
	std::lock_guard<std::mutex> lock(mtx);
	return get_next_tick_duration_unsafe();
}

// return whether it did normally wait.
bool CronWorker::wait_for_next_task() {
	std::unique_lock lock(mtx);
	while (true) {
		bool res = wait_cv.wait_for(lock, std::chrono::milliseconds(get_next_tick_duration_unsafe()), [this]() {
			return stopped_|| scheded_in_waiting;
		});

		if (res) { // satisfy cond
			if (stopped_) return false;
			if (scheded_in_waiting) {
				scheded_in_waiting = false;
				return true;
			}
		} else return true;
	}
}

void CronWorker::stop() {
	{
		std::lock_guard lock(mtx);
		stopped_ = true;
	}
	notify();
}


void CronWorker::run_pending() {
	std::vector<CronTask> tasks_to_run;
	std::vector<CronTask> tasks_to_reschedule;
	msec64 now = now_ms();

	{
		std::lock_guard lock(mtx);
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
			tasks_to_reschedule.push_back(task);
		}
	}

	if (!tasks_to_reschedule.empty()) {
		std::lock_guard lock(mtx);
		for (const auto& task : tasks_to_reschedule) {
			tasks.emplace(now + task.interval, task);
		}
		notify();
	}
}

void CronWorker::notify() {
	scheded_in_waiting = true;
	wait_cv.notify_one();
}