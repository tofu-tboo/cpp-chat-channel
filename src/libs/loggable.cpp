#include "loggable.h"
#include <cstring>
#include <ctime>
#include <chrono>

Loggable::Loggable(std::string className, std::string color, void* ptr): _color(color) {
	if (ptr == nullptr)
		_log_header = color + "[" + className + "] ";
	else {
		char buffer[15];
		snprintf(buffer, sizeof(buffer), "%14p", ptr);
		_log_header = color + "[" + className + ":" + std::string(buffer) + "] ";
	}
}

const char* Loggable::get_log_context() const {
	return _log_header.c_str();
}

void Loggable::log(const char* format, ...) const {
    char msg_buffer[LOG_BUF_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);
    va_end(args);

    std::string fmt_msg(msg_buffer);
    repl(fmt_msg, _color);

    std::string full_log = datetime_str() + " " + get_log_context() + fmt_msg + ___L_ESCAPE + "\n";

    printf("%s", full_log.c_str());
};

void Loggable::elog(const char* format, ...) const {
	char msg_buffer[LOG_BUF_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);
    va_end(args);

    std::string fmt_msg(msg_buffer);
    repl(fmt_msg, _L_RED);

    std::string full_log = datetime_str() + " " + get_log_context() + _L_RED + fmt_msg + ___L_ESCAPE + "\n";

    fprintf(stderr, "%s", full_log.c_str());
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

std::string Loggable::datetime_str() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t timer = std::chrono::system_clock::to_time_t(now);
    std::tm* bt = std::localtime(&timer);

    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d.%03ld", bt->tm_year + 1900, bt->tm_mon + 1, bt->tm_mday, bt->tm_hour, bt->tm_min, bt->tm_sec, ms.count());

    return _color + time_buf;
}
#pragma GCC diagnostic pop

void Loggable::repl(std::string& str, const std::string& sub) const {
	size_t pos = str.find(_L_DEFAULT);
	if (pos != std::string::npos) {
		str.replace(pos, sizeof(_L_DEFAULT) - 1, sub);
	}
}