#ifndef _CLIENT_UTILS_LOG_H_
#define _CLIENT_UTILS_LOG_H_

#include <stdio.h>
#include <stdbool.h>

#include "client/config.h"

#define LOG_DEFAULT_PATH		"/var/log/cerver"

#define LOG_POOL_INIT			32

#define LOG_DATETIME_SIZE		32
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

#define LOG_DEFAULT_UPDATE_INTERVAL			1

#ifdef __cplusplus
extern "C" {
#endif

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
	XX(8, 	CONNECTION, [CONNECTION])			\
	XX(9, 	HANDLER, 	[HANDLER])				\
	XX(10, 	ADMIN, 		[ADMIN])				\
	XX(11, 	EVENT, 		[EVENT])				\
	XX(12, 	PACKET, 	[PACKET])				\
	XX(13, 	REQ, 		[REQ])					\
	XX(14, 	FILE, 		[FILE])					\
	XX(15, 	HTTP, 		[HTTP])					\
	XX(16, 	GAME, 		[GAME])					\
	XX(17, 	PLAYER, 	[PLAYER)				\

typedef enum LogType {

	#define XX(num, name, string) LOG_TYPE_##name = num,
	LOG_TYPE_MAP (XX)
	#undef XX
	
} LogType;

#pragma endregion

#pragma region configuration

typedef enum LogOutputType {

	LOG_OUTPUT_TYPE_NONE		= 0,
	LOG_OUTPUT_TYPE_STD			= 1,	// log to stdout & stderr
	LOG_OUTPUT_TYPE_FILE		= 2,	// log to a file
	LOG_OUTPUT_TYPE_BOTH		= 3		// log to both std output and file

} LogOutputType;

// returns the current log output type
CLIENT_EXPORT LogOutputType client_log_get_output_type (void);

// sets the log output type to use
CLIENT_EXPORT void client_log_set_output_type (
	LogOutputType type
);

// sets the path where logs files will be stored
// returns 0 on success, 1 on error
CLIENT_EXPORT unsigned int client_log_set_path (
	const char *pathname
);

// sets the interval in secs which will be used to sync the contents of the log file to disk
// the default values is 1 second
CLIENT_EXPORT void client_log_set_update_interval (
	unsigned int interval
);

#define LOG_TIME_TYPE_MAP(XX)										\
	XX(0, 	NONE, 		None,		Logs without time)				\
	XX(1, 	TIME, 		Time,		Logs with time)					\
	XX(2, 	DATE, 		Date,		Logs with date)					\
	XX(3, 	BOTH, 		Both,		Logs with date and time)

typedef enum LogTimeType {

	#define XX(num, name, string, description) LOG_TIME_TYPE_##name = num,
	LOG_TIME_TYPE_MAP (XX)
	#undef XX

} LogTimeType;

CLIENT_PUBLIC const char *client_log_time_type_to_string (
	LogTimeType type
);

CLIENT_PUBLIC const char *client_log_time_type_description (
	LogTimeType type
);

// returns the current log time configuration
CLIENT_EXPORT LogTimeType client_log_get_time_config (void);

// sets the log time configuration to be used by log methods
// none: print logs with no dates
// time: 24h time format
// date: day/month/year format
// both: day/month/year - 24h date time format
CLIENT_EXPORT void client_log_set_time_config (LogTimeType type);

// set if logs datetimes will use local time or not
CLIENT_EXPORT void client_log_set_local_time (bool value);

// if the log's quiet option is set to TRUE,
// only success, warning & error messages will be handled
// any other type will be ignored
CLIENT_EXPORT void client_log_set_quiet (bool value);

#pragma endregion

#pragma region public

// creates and prints a message of custom types
// based on the first type, the message can be printed with colors to stdout
CLIENT_PUBLIC void client_log (
	LogType first_type, LogType second_type,
	const char *format, ...
);

// creates and prints a message of custom types
// and adds the date & time
// if the log_time_type has been configured, it will be kept
CLIENT_PUBLIC void client_log_with_date (
	LogType first_type, LogType second_type,
	const char *format, ...
);

// creates and prints a message of custom types
// to stdout or stderr based on type
// and to log file if available
// this messages ignore the quiet flag
CLIENT_PUBLIC void client_log_both (
	LogType first_type, LogType second_type,
	const char *format, ...
);

// prints a message with no type, effectively making this a custom printf ()
CLIENT_PUBLIC void client_log_msg (const char *msg, ...);

// prints a red error message to stderr
CLIENT_PUBLIC void client_log_error (const char *msg, ...);

// prints a yellow warning message to stderr
CLIENT_PUBLIC void client_log_warning (const char *msg, ...);

// prints a green success message to stdout
CLIENT_PUBLIC void client_log_success (const char *msg, ...);

// prints a debug message to stdout
CLIENT_PUBLIC void client_log_debug (const char *msg, ...);

// prints a message with no type or format
CLIENT_PUBLIC void client_log_raw (const char *msg, ...);

// prints a line break, equivalent to printf ("\n")
CLIENT_PUBLIC void client_log_line_break (void);

#pragma endregion

#pragma region main

CLIENT_PRIVATE void client_log_init (void);

CLIENT_PRIVATE void client_log_end (void);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif