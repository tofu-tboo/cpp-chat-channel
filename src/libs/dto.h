#ifndef __DTO_H__
#define __DTO_H__

#include <string>

#include "socket.h"

typedef unsigned int ch_id_t;

typedef struct {
	// std::string type;
	std::string text;
	msec64 timestamp;
} MessageReqDto;


// For union, recommended to match seq of members (aligning)
typedef struct {
	// std::string type;
	ch_id_t channel_id;
	msec64 timestamp;
	std::string user_id;
} JoinReqDto;

typedef struct {
	// std::string type;
	ch_id_t channel_id;
	msec64 timestamp;
} RejoinReqDto;

typedef union {
	JoinReqDto* join;
	RejoinReqDto* rejoin;
	// etc...
} UReportDto;
#endif