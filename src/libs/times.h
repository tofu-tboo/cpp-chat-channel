#ifndef __TIMES_H__
#define __TIMES_H__

#include <cstdint>
#include <chrono>

#define U2S			0.000001
#define M2S			0.001
#define S2U			1000000
#define S2M			1000
#define M2U			1000
#define U2M			0.001

typedef uint32_t msec;
typedef uint64_t msec64;

inline msec64 now_ms() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif