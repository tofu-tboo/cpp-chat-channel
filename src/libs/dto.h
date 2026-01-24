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
	ch_id_t channel_id;
	msec64 timestamp;
	std::string user_name;
} JoinReqDto;

typedef struct {
	// std::string type;
	ch_id_t channel_id;
	msec64 timestamp;
} RejoinReqDto;

typedef union {
	JoinReqDto* join;
	RejoinReqDto* rejoin;
} UJoinDto;

typedef struct {
	ch_id_t channel_id;
	msec64 timestamp;
} JoinBlockReqDto;
typedef union {
	JoinReqDto* join;
	RejoinReqDto* rejoin;
	JoinBlockReqDto* join_block;
	// etc...
} UReportDto;
#endif