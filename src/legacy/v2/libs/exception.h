#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <vector>
#include <stdexcept>
#include <string>
#include <cstdarg>
#include <cstdio>

class coded_runtime_error : public std::runtime_error {
public:
    int code;
    coded_runtime_error(int c, const std::string& s) : std::runtime_error(s), code(c) {}
    coded_runtime_error(int c, const char* s) : std::runtime_error(s), code(c) {}
};

inline std::runtime_error runtime_errorf(const char* s) {
    return std::runtime_error(s);
}

inline coded_runtime_error runtime_errorf(int code, const char* s) {
    return coded_runtime_error(code, s);
}

inline coded_runtime_error runtime_errorf(int code) {
	return coded_runtime_error(code, "");
}

template <typename... Args>
std::runtime_error runtime_errorf(const char* fmt, Args&&... args) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    if (n < 0) {
        return std::runtime_error("format error");
    }
    if (n < static_cast<int>(sizeof(buf))) {
        return std::runtime_error(buf);
    }
    // 버퍼가 모자라면 정확한 크기만큼 할당 후 다시 포맷
    std::vector<char> big(n + 1);
    snprintf(big.data(), big.size(), fmt, std::forward<Args>(args)...);
    return std::runtime_error(big.data());
}

template <typename... Args>
coded_runtime_error runtime_errorf(int code, const char* fmt, Args&&... args) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    if (n < 0) {
        return coded_runtime_error(code, "format error");
    }
    if (n < static_cast<int>(sizeof(buf))) {
        return coded_runtime_error(code, buf);
    }
    // 버퍼가 모자라면 정확한 크기만큼 할당 후 다시 포맷
    std::vector<char> big(n + 1);
    snprintf(big.data(), big.size(), fmt, std::forward<Args>(args)...);
    return coded_runtime_error(code, big.data());
}

inline const coded_runtime_error* try_get_coded_error(const std::exception& e) {
    return dynamic_cast<const coded_runtime_error*>(&e);
}


#endif