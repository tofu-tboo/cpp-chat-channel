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
	
#ifdef DEBUG
#define DLOG(format, ...)                       LoggerI.log(format _WHITE_ "\n", ##__VA_ARGS__)
#else
#define DLOG(format, ...) // log for debug mode
#endif

#define EVENT(color, format, ...)				LoggerI.log(color "" format _WHITE_ "\n", ##__VA_ARGS__)
#define LOG1L(format, ...)						LoggerI.log(format, ##__VA_ARGS__)
#define LOG(format, ...)                        LoggerI.log(format _WHITE_ "\n", ##__VA_ARGS__)
#ifndef ERROR
#define ERROR(format, ...)                      LoggerI.elog(format "\n", ##__VA_ARGS__)
#endif

#include <string>
#include <queue>
#include <map>
#include <thread>
#include <cstdarg>
#include <atomic>
#include <semaphore>

#include "../times.h"
#include "../singleton.h"

// v2 -> v3 diff
// combine LoggerContext & Loggable into Logger


class Logger: public Singleton<Logger> { // Logger::Instance()
	protected:
		struct LogEntry {
  		  	std::string message;
    		bool is_err;
		};
		struct LogNode {
			LogEntry entry;
			msec64 timestamp;
			LogNode* next;
		};
		std::atomic<LogNode*> head{nullptr};
		std::atomic<bool>       running;
		std::thread             worker;

		std::map<void*, std::string> indi_map;
	public:
		void set_src_indicator(void* ptr, std::string classname, std::string color);
	protected:
		Logger();

		const char* get_log_context() const;

		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
		// std::string get_str(const char* format, ...) const;
		std::string datetime_str() const;

	private:
		void vlog(const char* format, va_list args, bool is_err) const;
		void repl(std::string& str, const std::string& sub) const;
   		void enqueue(msec64 timestamp, std::string message, bool is_err);
		void consume_logs();
};

using LoggerI = Logger::Instance();

#endif