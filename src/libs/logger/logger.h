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
#define LOGS(format, ...)						LoggerI.log(format, ##__VA_ARGS__)
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

/* USAGE
	class { LoggerContext ctx; constructor(): ctx(_RED_ "my class") {} }
	...
	ctx.log("");
	OR
	LOG(ctx, "");
*/
class LoggerContext {
	private:
		const char* indi;
		const void* src;
	public:
		LoggerContext(const void* const s, const char* i);
		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
		// TODO?: indicator() to return "[indi: src]" form
}


class Logger: public Singleton<Logger> { // Logger::Instance()
	protected:
		struct LogEntry {
  		  	std::string message;
    		bool is_err;
		};
		struct LogNode {
			LogEntry entry;
			std::uint64_t timestamp;
			LogNode* next;
		};
		std::atomic<LogNode*> head{nullptr};
		std::atomic<bool>       running;
		std::thread             worker;

		static Logger* inst;
	public:
		static Logger* Instance() {
			if (!inst) {
				inst = new Logger();
				atexit(Destroy);
			}
			return inst;
		}
		static void Destroy() {
			if (inst) {
				delete inst;
				inst = nullptr;
			}
		}
	protected:
		Logger();

		// NOT IN THE MEMBER FUNC
		// log() & elog() just delivery the arguments to vlog() right away, indicating the error flag as true or false.
		void log(const char* format, ...) const;
		void elog(const char* format, ...) const;
		// std::string get_str(const char* format, ...) const;
		std::string datetime_str() const;

		inline std::uint64_t now() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

	private:
		friend class LoggerContext;
		void vlog(const char* format, va_list args, bool is_err) const;
		void vlog(const void* src, const char* indi, const char* format, va_list args, bool is_err) const;
		str::string indicate(const void* src = nullptr, const char* indi = nullptr);
		void repl(std::string& str, const std::string& sub) const;
   		void enqueue(std::uint64_t timestamp, std::string message, bool is_err);
		void consume_logs();
};

using LoggerI = Logger::Instance();

#endif