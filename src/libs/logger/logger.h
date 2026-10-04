#ifndef __LOGGING_H__
#define __LOGGING_H__
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define LOG_BUF_SIZE							(256)

#define _WHITE_									"\033[0m"
#define _RED_        							"\033[0;31m"
#define _GREEN_       							"\033[0;32m"
#define _YELLOW_      							"\033[0;33m"
#define _BLUE_        							"\033[0;34m"
#define _MAGENTA_     							"\033[0;35m"
#define _CYAN_        							"\033[0;36m"
#define _END_									"\x1F" // placeholder
#define LoggerI Logger::Instance()

#ifdef DEBUG
#define DLOG(format, ...)                       LoggerI.log(format _WHITE_ "\n", ##__VA_ARGS__)
#define DLOG(ctx, format, ...)                       ctx.log(format _END_ "\n", ##__VA_ARGS__)
#else
#define DLOG(format, ...) // log for debug mode
#define DLOG(ctx, format, ...) // log for debug mode
#endif

#define EVLOG(color, format, ...)				LoggerI.log(color "" format _WHITE_ "\n", ##__VA_ARGS__)
#define LOG(format, ...)                        LoggerI.log(format _WHITE_ "\n", ##__VA_ARGS__)
#define EVLOG(ctx, color, format, ...)			ctx.log(color "" format _END_ "\n", ##__VA_ARGS__)
#define LOG(ctx, format, ...)                   ctx.log(format _END_ "\n", ##__VA_ARGS__)

#ifndef ELOG
#define ELOG(format, ...)                      LoggerI.elog(format _WHITE_ "\n", ##__VA_ARGS__)
#define ELOG(ctx, format, ...)                 ctx.elog(format _END_ "\n", ##__VA_ARGS__)
#endif

#include <string>
#include <queue>
#include <map>
#include <thread>
#include <cstdarg>
#include <atomic>
#include <semaphore>
#include <chrono>
#include <array>
#include <cstddef>
#include <utility>

/*ASSUMPTION
- A logger is always destructed after other log producers dead. (*For other global instances, no-joined thread, and etc, BE CAREFUL to use logger in them.*)
*/

/* USAGE
	class { LoggerContext ctx; constructor(): ctx(_RED_ "my class") {} }
	...
	ctx.log("...");
	OR
	LOG(ctx, "...");

	in non-class,
	LOG("...");
*/
using LoggerTimePoint = std::chrono::system_clock::time_point;

struct LogEntry {
	std::string message;
	LoggerTimePoint timestamp;
	bool is_err;
};

class LogRingBuffer {
	static constexpr std::size_t Capacity = 1024;
	std::array<LogEntry, Capacity> entries;
	std::atomic<std::size_t> head{0};
	std::atomic<std::size_t> tail{0};

public:
	bool try_push(LogEntry entry);
	bool try_pop(LogEntry& entry);
};

class LoggerContext {
	private:
		// It makes sense to each instances have their info of indicators.
		std::string indi;
		const char* name; // for a static string
		const void* src; // addr of class inst
		mutable LogRingBuffer log_queue;
		void vlog(const char* format, va_list args, bool is_err) const;
	public:
		LoggerContext(const void* const s, const char* n);
		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
};


class Logger { // Logger::Instance()
	protected:
		// struct Node {
		// 	LogEntry entry;
		// 	Node* next;
		// };
		// std::atomic<Node*> head{nullptr};
		std::atomic<bool>       running;
		std::thread             worker;
		std::counting_semaphore<INT_MAX> sem{0};
		mutable LogRingBuffer log_queue; // for non-class logging

	public: // singleton
		static Logger& Instance() { // user can control initialization time by calling
			static Logger instance;
			return instance;
		}
	protected:
		Logger();

		// NOT IN THE MEMBER FUNC
		// log() & elog() just delivery the arguments to vlog() right away, indicating the error flag as true or false.
		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
		std::string datetime_str(LoggerTimePoint time) const;

		inline LoggerTimePoint now() const {
			return std::chrono::system_clock::now();
		}

	private:
		friend class LoggerContext;
		void vlog(const char* format, va_list args, bool is_err) const;
		std::string apply_default_color(const char* str, const char* color) const;
		// void enqueue(LoggerTimePoint timestamp, std::string message, bool is_err);
		void consume_logs();
};


#endif
