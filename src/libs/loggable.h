#ifndef __LOGGABLE_H__
#define __LOGGABLE_H__

#define LOG_BUF_SIZE							(256)

#define _L_RED									"\033[0;31m"
#define _L_GREEN								"\033[0;32m"
#define _L_BLUE									"\033[0;34m"
#define _L_YELLOW								"\033[0;33m"
#define _L_CYAN									"\033[0;36m"
#define _L_WHITE								"\033[0;37m"
#define _L_DEFAULT								"\033[00000"
#define ___L_ESCAPE								"\033[0m"

#include <string>
#include <queue>
#include <map>
#include <thread>
#include <cstdarg>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "class.h"
#include "times.h"
#include "../dynamic_compile/dynamic_compile.h"
__DYNAMIC_INCLUDES__

struct LogEntry {
    std::string message;
    bool is_stderr;
};

// RAII wrapper for the logger thread and queue
struct LoggerContext {
    std::multimap<msec64, LogEntry> queue;
    std::mutex              mutex;
    std::condition_variable cv;
    std::atomic<bool>       running;
    std::thread             worker;

    LoggerContext();
    ~LoggerContext();

    void enqueue(msec64 timestamp, std::string message, bool is_stderr);

private:
    void run();
};

__virtual_parent__ class Loggable {
	var_protected:
		std::string _log_header;
		std::string _color;
	func_protected:
		Loggable(std::string className, std::string color, void* ptr = nullptr);

		const char* get_log_context() const;

		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
		inline void dlog(const char* format, ...) const {
		#ifdef DEBUG
			va_list args;
			va_start(args, format);
			vlog(format, args, false);
			va_end(args);
		#endif
		}
		std::string datetime_str() const;

	func_private:
		static LoggerContext& _ctx();
		void vlog(const char* format, va_list args, bool is_err) const;
		void repl(std::string& str, const std::string& sub) const;
};

#endif