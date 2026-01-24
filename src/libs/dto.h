#ifndef __DTO_H__
#define __DTO_H__

#include <string>

#include "socket.h"

typedef unsigned int ch_id_t;

enum MsgType {
	USER,
	SYSTEM
};
typedef struct {
	MsgType type;
	std::string text;
	msec64 timestamp;
	std::string user_name;
} MessageReqDto;

typedef struct {
	// std::string type;
	ch_id_t ch_from;
	ch_id_t ch_to;
	msec64 timestamp;
	std::string user_name;
} JoinReqDto;

typedef union {
	JoinReqDto* join;
	// etc...
} UReportDto;
#endif