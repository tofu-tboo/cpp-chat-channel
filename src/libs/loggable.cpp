#include "loggable.h"
#include <cstring>
#include <cstdarg>

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

void Loggable::log(const char* format, ...) {
	std::string fmt(format);
	repl(fmt, _color);

	va_list args;
	va_start(args, format);
	printf("%s", get_log_context());
	vprintf(fmt.c_str(), args);
	printf("\n" ___L_ESCAPE);
	va_end(args);
};

void Loggable::elog(const char* format, ...) {
	std::string fmt(format);
	repl(fmt, _L_RED);

	va_list args;
	va_start(args, format);
	fprintf(stderr, "%s", get_log_context());
	fprintf(stderr, _L_RED);
	vfprintf(stderr, fmt.c_str(), args);
	fprintf(stderr, "\n" ___L_ESCAPE);
	va_end(args);
}

void Loggable::repl(std::string& str, const std::string& sub) {
	size_t pos = str.find(_L_DEFAULT);
	if (pos != std::string::npos) {
		str.replace(pos, sizeof(_L_DEFAULT) - 1, sub);
	}
}