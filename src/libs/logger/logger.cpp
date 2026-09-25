#include "Logger.h"
#include <ctime>
#include <chrono>

LoggerContext::LoggerContext(const void* const s, const char* i): indi(i), src(s) {}
void LoggerContext::log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LoggerI.vlog(src, indi, format, args, false);
    va_end(args);
}
void LoggerContext::elog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LoggerI.vlog(src, indi, format, args, true);
    va_end(args);
}

Logger* Logger::inst = nullptr; 

Logger::Logger(): running(true) {
    // Setup a worker
    running = true;
    worker = std::thread(&LoggerI::consume_logs, this);
	
	// if (ptr == nullptr)
	// 	_log_header = color + "[" + className + "] ";
	// else {
	// 	char buffer[15];
	// 	snprintf(buffer, sizeof(buffer), "%14p", ptr);
	// 	_log_header = color + "[" + className + ":" + std::string(buffer) + "] ";
	// }
}

Logger::~Logger() {
    running = false;
	// flush logs
    sem.release(); // Wake up worker
    if (worker.joinable()) {
        worker.join();
    }
    
    // Cleanup remaining nodes
    LogNode* current = head.load();
    while (current) {
        LogNode* next = current->next;
        delete current;
        current = next;
    }
}


void Logger::enqueue(std::uint64_t timestamp, std::string message, bool is_err) {
    LogNode* node = new LogNode{ {std::move(message), is_err}, timestamp, nullptr };
    
    // Lock-free push to head
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed));
    
    sem.release();
}

void Logger::consume_logs() {
    while (running) {
        sem.acquire();

        // Atomic pop all (take ownership of the whole list)
        LogNode* local_head = head.exchange(nullptr, std::memory_order_acquire);
        
        if (!local_head) continue; 
		
        // Reverse the list to restore FIFO order (Oldest -> Newest)
        LogNode* prev = nullptr;
        LogNode* current = local_head;
        while (current) {
            LogNode* next = current->next;
            current->next = prev;
            prev = current;
            current = next;
        }
        local_head = prev;

        // Sort by timestamp
        std::multimap<std::uint64_t, LogEntry> sorted_logs;
        while (local_head) {
            LogNode* next = local_head->next;
            sorted_logs.emplace(local_head->timestamp, std::move(local_head->entry));
            delete local_head;
            local_head = next;
        }

        for (const auto& [timestamp, entry] : sorted_logs) {
            fprintf(entry.is_err ? stderr : stdout, "%s", entry.message.c_str());
        }
    }
}

str::string Logger::indicate(const void* src = nullptr, const char* indi = nullptr) {
    if (src == nullptr || indi == nullptr) {
        return // TODO: to resolve string construction cost of func returns & vlog's declaration /   
    }
}
//void vlog(const void* src, const char* indi, const char* format, va_list args, bool is_err) const; // TODO: params changed from src/indi to ctx for using ctx.indicator()
		
void Logger::vlog(const char* format, va_list args, bool is_err) const {
    char msg_buffer[LOG_BUF_SIZE];
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);

    std::string fmt_msg(msg_buffer);
    std::string full_log;
    
    if (is_err) {
        repl(fmt_msg, _RED_);
        full_log = datetime_str() + " " + _RED_ + fmt_msg;
    } else {
        repl(fmt_msg, _color);
        full_log = datetime_str() + " " + fmt_msg;
    }

    std::uint64_t timestamp = now();
    _ctx().enqueue(timestamp, std::move(full_log), is_err);
}

void Logger::log(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    vlog(format, args, false);
    va_end(args);
}

void Logger::elog(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    vlog(format, args, true);
    va_end(args);
}
// std::string Logger::get_str(const char* format, ...) const {
//     char msg_buffer[LOG_BUF_SIZE];
//     vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);

//     std::string fmt_msg(msg_buffer);
//     std::string full_log;
    
//     if (is_err) {
//         repl(fmt_msg, _RED_);
//         full_log = datetime_str() + " " + get_log_context() + _RED_ + fmt_msg + _WHITE_ + "\n";
//     } else {
//         repl(fmt_msg, _color);
//         full_log = datetime_str() + " " + get_log_context() + fmt_msg + _WHITE_ + "\n";
//     }

//     return full_log;
// }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

std::string Logger::datetime_str() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t timer = std::chrono::system_clock::to_time_t(now);
    
    std::tm bt_buf;
#ifdef _WIN32
    localtime_s(&bt_buf, &timer);
    std::tm* bt = &bt_buf;
#else
    std::tm* bt = localtime_r(&timer, &bt_buf);
#endif

    char time_buf[64];
    if (!bt) return "invalid_time";

    snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        bt->tm_year + 1900, bt->tm_mon + 1, bt->tm_mday,
        bt->tm_hour, bt->tm_min, bt->tm_sec, ms.count());

    return _color + time_buf;
}
#pragma GCC diagnostic pop

void Logger::repl(std::string& str, const std::string& sub) const {
	size_t pos = str.find(_L_DEFAULT);
	if (pos != std::string::npos) {
		str.replace(pos, sizeof(_L_DEFAULT) - 1, sub);
	}
}