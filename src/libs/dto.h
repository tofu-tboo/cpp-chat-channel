#ifndef __DTO_H__
#define __DTO_H__

#include <string>

#include "socket.h"
#include "util.h"

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
	ch_id_t channel_id;
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

typedef struct {
	ch_id_t ch;
	uint64_t uid;
	msec64 join_t;
	char* name; // needed to free
} User;

#endif