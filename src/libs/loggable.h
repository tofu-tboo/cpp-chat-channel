#ifndef __LOGGABLE_H__
#define __LOGGABLE_H__

#define _L_RED									"\033[0;31m"
#define _L_GREEN								"\033[0;32m"
#define _L_BLUE									"\033[0;34m"
#define _L_YELLOW								"\033[0;33m"
#define _L_CYAN									"\033[0;36m"
#define _L_WHITE								"\033[0;37m"
#define _L_DEFAULT								"\033[00000"
#define ___L_ESCAPE								"\033[0m"

#include <string>

class Loggable {
protected:
    std::string _log_header;
	std::string _color;
public:
    Loggable(std::string className, std::string color, void* ptr = nullptr);

    const char* get_log_context() const;

	void log(const char* format, ...);
	void elog(const char* format, ...);

private:
	void repl(std::string& str, const std::string& sub);
};

#endif