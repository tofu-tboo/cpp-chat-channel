#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string>

#define _EC_                                    "\033[0m"
#define _CR_                		            "\033[0;31m"
#define _CG_                   		            "\033[0;32m"
#define _CB_                		            "\033[0;34m"

#ifdef DEBUG
#define DLOG(format, ...)                       printf(format "\n", ##__VA_ARGS__)
#else
#define DLOG(format, ...) // log for debug mode
#endif
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

inline constexpr unsigned int hash(const char* str) {
    return str && str[0] ? static_cast<unsigned int>(str[0]) + 0xEDB8832Full * hash(str + 1) : 8603;
}

#endif
