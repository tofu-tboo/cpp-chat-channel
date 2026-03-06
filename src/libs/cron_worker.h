#ifndef __CRON_WORKER_H__
#define __CRON_WORKER_H__

#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

#include "class.h"
#include "times.h"
#include "univ_key.h"

class CronWorker {
	type_private:
		struct CronTask {
			UnivKey id;
			std::function<void()> func;
			msec64 interval; // 0 for one-shot tasks
		};

	var_private:
		std::multimap<msec64, CronTask> tasks; // Key: next execution time
		mutable std::mutex mtx;
		std::atomic<uint64_t> next_task_id{1};
		
		std::condition_variable wait_cv;
		std::atomic<bool> scheded_in_waiting;

		std::atomic<bool> stopped_;
	func_public:
		/**
		 * @brief Schedule a task to run once at a specific time.
		 * @param key The unique key for the task.
		 * @param exec_time The absolute time (in milliseconds since epoch) to execute the task.
		 * @param func The function to execute.
		 */
		void schedule_at(const UnivKey& key, msec64 exec_time, std::function<void()> func);
		UnivKey schedule_at(msec64 exec_time, std::function<void()> func);

		/**
		 * @brief Schedule a task to run once after a certain delay.
		 * @param key The unique key for the task.
		 * @param delay The delay (in milliseconds) from now.
		 * @param func The function to execute.
		 */
		void schedule_in(const UnivKey& key, msec64 delay, std::function<void()> func);
		UnivKey schedule_in(msec64 delay, std::function<void()> func);

		/**
		 * @brief Schedule a task to run periodically.
		 * @param key The unique key for the task.
		 * @param interval The interval (in milliseconds) between executions.
		 * @param func The function to execute.
		 * @param run_immediately If true, the first execution is scheduled immediately. Otherwise, after the first interval.
		 */
		void schedule_every(const UnivKey& key, msec64 interval, std::function<void()> func, bool run_immediately = false);
		UnivKey schedule_every(msec64 interval, std::function<void()> func, bool run_immediately = false);

		/**
		 * @brief Cancel a scheduled task by its Key.
		 * @param key The Key of the task to cancel.
		 */
		void cancel(const UnivKey& key);

		/**
		 * @brief Run all tasks that are due.
		 * This should be called periodically (e.g., in a dedicated thread loop).
		 */
		void run_pending();

		/**
		 * @brief Get the duration until the next task is due.
		 * @return Duration in milliseconds. Returns a very large value if no tasks are scheduled.
		 */
		msec64 get_next_tick_duration() const;
		msec64 get_next_tick_duration_unsafe() const;

		bool wait_for_next_task();

		void stop();
	func_private:
		void notify();
};

#endif