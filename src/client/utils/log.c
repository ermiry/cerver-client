#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#include "client/collections/pool.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

static Pool *log_pool = NULL;

#pragma region types

static const char *log_get_msg_type (LogType type) {

	switch (type) {
		#define XX(num, name, string) case LOG_TYPE_##name: return #string;
		LOG_TYPE_MAP(XX)
		#undef XX

		default: return log_get_msg_type (LOG_TYPE_NONE);
	}

}

#pragma endregion

#pragma region internal

typedef struct {

	char header[LOG_HEADER_SIZE];
	char *second;

	char message[LOG_MESSAGE_SIZE];

} ClientLog;

static void *client_log_new (void) {

	ClientLog *log = (ClientLog *) malloc (sizeof (ClientLog));
	if (log) {
		memset (log->header, 0, LOG_HEADER_SIZE);
		log->second = log->header + LOG_HEADER_HALF_SIZE;

		memset (log->message, 0, LOG_MESSAGE_SIZE);
	}

	return log;

}

static void client_log_delete (void *client_log_ptr) {

	if (client_log_ptr) free (client_log_ptr);

}

static void client_log_header_create (
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	const char *first = log_get_msg_type (first_type);
	if (second_type != LOG_TYPE_NONE) {
		switch (first_type) {
			case LOG_TYPE_DEBUG:
			case LOG_TYPE_TEST: {
				// first type
				(void) snprintf (
					log->header, LOG_HEADER_HALF_SIZE, 
					"%s", 
					first
				);

				// second type
				(void) snprintf (
					log->second, LOG_HEADER_HALF_SIZE, 
					"%s", 
					log_get_msg_type (second_type)
				);
			} break;

			default: {
				(void) snprintf (
					log->header, LOG_HEADER_SIZE, 
					"%s%s", 
					first, log_get_msg_type (second_type)
				);
			} break;
		}
	}

	else {
		(void) snprintf (
			log->header, LOG_HEADER_SIZE,
			"%s",
			first
		);
	}

}

static FILE *client_log_get_stream (LogType first_type) {

	FILE *retval = stdout;

	switch (first_type) {
		case LOG_TYPE_ERROR:
		case LOG_TYPE_WARNING:
			retval = stderr;
			break;

		default: break;
	}

	return retval;

}

static void client_log_internal (
	FILE *__restrict __stream,
	LogType first_type, LogType second_type,
	const char *format, va_list args
) {

	ClientLog *log = (ClientLog *) pool_pop (log_pool);
	if (log) {
		client_log_header_create (log, first_type, second_type);
		(void) vsnprintf (log->message, LOG_MESSAGE_SIZE, format, args);

		switch (first_type) {
			case LOG_TYPE_ERROR: fprintf (__stream, LOG_COLOR_RED "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
			case LOG_TYPE_WARNING: fprintf (__stream, LOG_COLOR_YELLOW "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
			case LOG_TYPE_SUCCESS: fprintf (__stream, LOG_COLOR_GREEN "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

			case LOG_TYPE_DEBUG: {
				if (second_type != LOG_TYPE_NONE)
					fprintf (__stream, LOG_COLOR_MAGENTA "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

				else fprintf (__stream, LOG_COLOR_MAGENTA "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
			} break;
			
			case LOG_TYPE_TEST: {
				if (second_type != LOG_TYPE_NONE)
					fprintf (__stream, LOG_COLOR_CYAN "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

				else fprintf (__stream, LOG_COLOR_CYAN "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
			} break;

			case LOG_TYPE_CERVER: fprintf (__stream, LOG_COLOR_BLUE "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

			case LOG_TYPE_EVENT: fprintf (__stream, LOG_COLOR_MAGENTA "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

			default: fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		}

		pool_push (log_pool, log);
	}

}

#pragma endregion

#pragma region public

void client_log (
	LogType first_type, LogType second_type,
	const char *format, ...
) {

	if (format) {
		va_list args = { 0 };
		va_start (args, format);

		client_log_internal (
			client_log_get_stream (first_type),
			first_type, second_type,
			format, args
		);

		va_end (args);
	}

}

void client_log_msg (
	FILE *__restrict __stream, 
	LogType first_type, LogType second_type,
	const char *msg
) {

	if (__stream && msg) {
		va_list args = { 0 };

		client_log_internal (
			__stream,
			first_type, second_type,
			msg, args
		);
	}

}

// prints a red error message to stderr
void client_log_error (const char *msg, ...) {

	if (msg) {
		va_list args = { 0 };
		va_start (args, msg);

		client_log_internal (
			stderr,
			LOG_TYPE_ERROR, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a yellow warning message to stderr
void client_log_warning (const char *msg, ...) {

	if (msg) {
		va_list args = { 0 };
		va_start (args, msg);

		client_log_internal (
			stderr,
			LOG_TYPE_WARNING, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a green success message to stdout
void client_log_success (const char *msg, ...) {

	if (msg) {
		va_list args = { 0 };
		va_start (args, msg);

		client_log_internal (
			stdout,
			LOG_TYPE_SUCCESS, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a debug message to stdout
void client_log_debug (const char *msg, ...) {

	if (msg) {
		va_list args = { 0 };
		va_start (args, msg);

		client_log_internal (
			stdout,
			LOG_TYPE_DEBUG, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

#pragma endregion

#pragma region main

void client_log_init (void) {

	if (!log_pool) {
		log_pool = pool_create (client_log_delete);
		pool_set_create (log_pool, client_log_new);
		pool_set_produce_if_empty (log_pool, true);
		pool_init (log_pool, client_log_new, LOG_POOL_INIT);
	}

}

void client_log_end (void) {

	pool_delete (log_pool);
	log_pool = NULL;

}

#pragma endregion