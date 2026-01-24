#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <stdexcept>

#define _EC_                                    "\033[0m"
#define _CR_                		            "\033[0;31m"
#define _CG_                   		            "\033[0;32m"
#define _CB_                		            "\033[0;34m"
#define _CY_                		            "\033[0;33m"

#ifdef DEBUG
#define DLOG(format, ...)                       printf(format "\n", ##__VA_ARGS__)
#else
#define DLOG(format, ...) // log for debug mode
#endif
#define LOG2(format, ...)						printf(format, ##__VA_ARGS__)
#define LOG(format, ...)                        printf(format "\n", ##__VA_ARGS__)
#define ERROR(format, ...)                      printf(_CR_ format _EC_ "\n", ##__VA_ARGS__)

#define SELECT_ARITIES(_1,_2,_3,_4,FUNC,...)    FUNC
#define AMP1(a)                                 &a
#define AMP2(a, ...)                            &a,AMP1(__VA_ARGS__)
#define AMP3(a, ...)                            &a,AMP2(__VA_ARGS__)
#define AMP4(a, ...)                            &a,AMP3(__VA_ARGS__)
#define AMPS(...)                               SELECT_ARITIES(__VA_ARGS__, AMP4, AMP3, AMP2, AMP1)(__VA_ARGS__)

#define CNT_ARGS_IMPL(_1,_2,_3,_4,N,...)        N
#define CNT_ARGS(...)                           CNT_ARGS_IMPL(__VA_ARGS__, 4,3,2,1,0)

#define SAME_STR(x, y)                          (x != NULL && y != NULL && !strcmp((x), (y)))

#define __FREES(...)                            frees(CNT_ARGS(__VA_ARGS__), __VA_ARGS__)

void frees(int, ...);

class coded_runtime_error : public std::runtime_error {
public:
    int code;
    coded_runtime_error(int c, const std::string& s) : std::runtime_error(s), code(c) {}
    coded_runtime_error(int c, const char* s) : std::runtime_error(s), code(c) {}
};

inline constexpr unsigned int hash(const char* str) {
    return str && str[0] ? static_cast<unsigned int>(str[0]) + 0xEDB8832Full * hash(str + 1) : 8603;
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

#endif
