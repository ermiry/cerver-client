#ifndef _CLIENT_UTILS_LOG_H_
#define _CLIENT_UTILS_LOG_H_

#include <stdio.h>

#include "client/config.h"

#define LOG_POOL_INIT			32

#define LOG_HEADER_SIZE			32
#define LOG_HEADER_HALF_SIZE	LOG_HEADER_SIZE / 2

#define LOG_MESSAGE_SIZE		4096

#define LOG_COLOR_RED       "\x1b[31m"
#define LOG_COLOR_GREEN     "\x1b[32m"
#define LOG_COLOR_YELLOW    "\x1b[33m"
#define LOG_COLOR_BLUE      "\x1b[34m"
#define LOG_COLOR_MAGENTA   "\x1b[35m"
#define LOG_COLOR_CYAN      "\x1b[36m"
#define LOG_COLOR_RESET     "\x1b[0m"

#pragma region types

#define LOG_TYPE_MAP(XX)						\
	XX(0, 	NONE, 		[NONE])					\
	XX(1, 	ERROR, 		[ERROR])				\
	XX(2, 	WARNING, 	[WARNING])				\
	XX(3, 	SUCCESS, 	[SUCCESS])				\
	XX(4, 	DEBUG, 		[DEBUG])				\
	XX(5, 	TEST, 		[TEST])					\
	XX(6, 	CERVER, 	[CERVER])				\
	XX(7, 	CLIENT, 	[CLIENT])				\
	XX(8, 	HANDLER, 	[HANDLER])				\
	XX(9, 	ADMIN, 		[ADMIN])				\
	XX(10, 	EVENT, 		[EVENT])				\
	XX(11, 	PACKET, 	[PACKET])				\
	XX(12, 	REQ, 		[REQ])					\
	XX(13, 	FILE, 		[FILE])					\
	XX(14, 	HTTP, 		[HTTP])					\
	XX(15, 	GAME, 		[GAME])					\
	XX(16, 	PLAYER, 	[PLAYER)				\

typedef enum LogType {

	#define XX(num, name, string) LOG_TYPE_##name = num,
	LOG_TYPE_MAP (XX)
	#undef XX
	
} LogType;

#pragma endregion

#pragma region public

CLIENT_PUBLIC void client_log (
	LogType first_type, LogType second_type,
	const char *format, ...
);

CLIENT_PUBLIC void client_log_msg (
	FILE *__restrict __stream, 
	LogType first_type, LogType second_type,
	const char *msg
);

// prints a red error message to stderr
CLIENT_PUBLIC void client_log_error (const char *msg, ...);

// prints a yellow warning message to stderr
CLIENT_PUBLIC void client_log_warning (const char *msg, ...);

// prints a green success message to stdout
CLIENT_PUBLIC void client_log_success (const char *msg, ...);

// prints a debug message to stdout
CLIENT_PUBLIC void client_log_debug (const char *msg, ...);

#pragma endregion

#endif