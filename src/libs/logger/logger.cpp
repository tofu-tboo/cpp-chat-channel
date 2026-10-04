#include "Logger.h"
#include <cassert>
#include <cstring>
#include <chrono>
#include <ctime>

LoggerContext::LoggerContext(const void* const s, const char* n): name(n), src(s) {
    assert(strlen(n) <= 32);
    char buf[2 + 32 + 2 + 14];
    snprintf(buf, sizeof(buf), "[%s: %014p]", name, src);
    indi = buf;
}
void LoggerContext::log(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    LoggerI.vlog(this, format, args, false);
    va_end(args);
}
void LoggerContext::elog(const char* format, ...) const {
    va_list args;
    va_start(args, format);
    LoggerI.vlog(this, format, args, true);
    va_end(args);
}
const std::string& LoggerContext::indicator() const {
    return indi;
}

Logger::Logger(): running(true) {
    // Setup a worker
    running = true;
    worker = std::thread(&Logger::consume_logs, this);
}

Logger::~Logger() {
    running = false;
	// flush logs
    sem.release(); // Wake up worker
    if (worker.joinable()) {
        worker.join();
    }
    
    // Cleanup remaining nodes
    Node* current = head.load();
    while (current) {
        Node* next = current->next;
        delete current;
        current = next;
    }
}


void Logger::enqueue(TimePoint timestamp, std::string message, bool is_err) {
    Node* node = new Node{ {std::move(message), timestamp, is_err}, nullptr };
    
    // Lock-free push to head
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed));
    
    sem.release();
}

void Logger::consume_logs() {
    std::multimap<TimePoint, LogEntry> sorted_logs;
    while (true) {
        sem.acquire();

        // Atomic pop all (take ownership of the whole list)
        Node* local_head = head.exchange(nullptr, std::memory_order_acquire);
        
        if (!local_head) {
            if (!running) break; // clear 판별
            continue;
        }
		
        // Reverse the list to restore FIFO order (Oldest -> Newest)
        Node* prev = nullptr;
        Node* current = local_head;
        while (current) {
            Node* next = current->next;
            current->next = prev;
            prev = current;
            current = next;
        }
        local_head = prev;

        // Sort by timestamp
        sorted_logs.clear();
        while (local_head) {
            Node* next = local_head->next;
            sorted_logs.emplace(local_head->entry.timestamp, std::move(local_head->entry));
            delete local_head;
            local_head = next;
        }

        for (const auto& [timestamp, entry] : sorted_logs) {
            fprintf(entry.is_err ? stderr : stdout, "%s", entry.message.c_str());
        }
    }
}

// TODO: to resolve string construction cost of func returns & vlog's declaration /   

void Logger::vlog(const LoggerContext* ctx, const char* format, va_list args, bool is_err) const {
    char msg_buffer[LOG_BUF_SIZE];
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);

    const auto timestamp = now();
    std::string full_log = datetime_str(timestamp); //rvo
    full_log += " ";
    full_log += ctx->indicator();
    full_log += " ";
    
    if (is_err) {
        full_log += _RED_;
        full_log += apply_default_color(msg_buffer, _RED_);
    } else {
        full_log += apply_default_color(msg_buffer, _WHITE_);
    }

    enqueue(timestamp, std::move(full_log), is_err);
}
		
void Logger::vlog(const char* format, va_list args, bool is_err) const {
    char msg_buffer[LOG_BUF_SIZE];
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);

    const auto timestamp = now();
    std::string full_log = datetime_str(timestamp); //rvo
    full_log += " ";
    
    if (is_err) {
        full_log += _RED_
        full_log += apply_default_color(msg_buffer, _RED_);
    } else {
        full_log += apply_default_color(msg_buffer, _WHITE_);
    }

    enqueue(timestamp, std::move(full_log), is_err);
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

std::string Logger::datetime_str(TimePoint time) const {
    // std::format의 chrono 포맷팅은 내부 저장공간을 더 소모할 가능성이 있음. 
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
    std::time_t timer = std::chrono::system_clock::to_time_t(time);
    
    std::tm bt_buf;
#ifdef _WIN32
    if (gmtime_s(&bt_buf, &timer))
        return "UTC-ERROR";
    std::tm* bt = &bt_buf;
#else
    std::tm* bt = gmtime_r(&timer, &bt_buf);
#endif

    char buf[24];
    if (!bt) return "UTC-ERROR";

    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        bt->tm_year + 1900, bt->tm_mon + 1, bt->tm_mday,
        bt->tm_hour, bt->tm_min, bt->tm_sec, ms.count());

    return std::string(buf);
}
#pragma GCC diagnostic pop

std::string Logger::apply_default_color(
    const char* str,
    const char* color
) const {
    assert(strlen(color) == 1); // guarantee color tag
    std::string result(str);

    std::size_t pos = result.find(_END_);
    while (pos != std::string::npos) {
        result.replace(pos, 1, color);
        pos = result.find(_END_);
    }

    return result;
}
