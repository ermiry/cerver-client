#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <stdarg.h>
#include <time.h>

#include "client/types/string.h"

#include "client/collections/pool.h"

#include "client/files.h"
#include "client/version.h"

#include "client/utils/utils.h"
#include "client/utils/log.h"

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

#pragma region configuration

static LogOutputType log_output_type = LOG_OUTPUT_TYPE_STD;

static LogTimeType log_time_type = LOG_TIME_TYPE_NONE;
static bool use_local_time = false;

static String *logs_pathname = NULL;
static FILE *logfile = NULL;

// returns the current log output type
LogOutputType client_log_get_output_type (void) {

	return log_output_type;

}

// sets the log output type to use
void client_log_set_output_type (LogOutputType type) {

	log_output_type = type;

}

// sets the path where logs files will be stored
// returns 0 on success, 1 on error
unsigned int client_log_set_path (const char *pathname) {

	unsigned int retval = 1;

	if (pathname) {
		if (!file_exists (pathname)) {
			if (!files_create_dir (pathname, 0755)) {
				logs_pathname = str_new (pathname);
				retval = 0;
			}
		}

		else {
			logs_pathname = str_new (pathname);
			retval = 0;
		}
	}

	return retval;

}

const char *client_log_time_type_to_string (LogTimeType type) {

	switch (type) {
		#define XX(num, name, string, description) case LOG_TIME_TYPE_##name: return #string;
		LOG_TIME_TYPE_MAP(XX)
		#undef XX

		default: return client_log_time_type_to_string (LOG_TIME_TYPE_NONE);
	}

}

const char *client_log_time_type_description (LogTimeType type) {

	switch (type) {
		#define XX(num, name, string, description) case LOG_TIME_TYPE_##name: return #description;
		LOG_TIME_TYPE_MAP(XX)
		#undef XX

		default: return client_log_time_type_description (LOG_TIME_TYPE_NONE);
	}

}

// returns the current log time configuration
LogTimeType client_log_get_time_config (void) {

	return log_time_type;

}

// sets the log time configuration to be used by log methods
// none: print logs with no dates
// time: 24h time format
// date: day/month/year format
// both: day/month/year - 24h date time format
void client_log_set_time_config (LogTimeType type) {

	log_time_type = type;

}

// set if logs datetimes will use local time or not
void client_log_set_local_time (bool value) { use_local_time = value; }

#pragma endregion

#pragma region internal

static Pool *log_pool = NULL;

typedef struct {

	char datetime[LOG_DATETIME_SIZE];
	char header[LOG_HEADER_SIZE];
	char *second;

	char message[LOG_MESSAGE_SIZE];

} ClientLog;

static void *client_log_new (void) {

	ClientLog *log = (ClientLog *) malloc (sizeof (ClientLog));
	if (log) {
		memset (log->datetime, 0, LOG_DATETIME_SIZE);
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

	if (logfile) retval = logfile;
	else {
		switch (first_type) {
			case LOG_TYPE_ERROR:
			case LOG_TYPE_WARNING:
				retval = stderr;
				break;

			default: break;
		}
	}

	return retval;

}

static void client_log_internal_normal_std (
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: fprintf (stdout, "%s\n", log->message); break;

		case LOG_TYPE_ERROR: fprintf (stderr, LOG_COLOR_RED "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_WARNING: fprintf (stderr, LOG_COLOR_YELLOW "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: fprintf (stdout, LOG_COLOR_GREEN "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (stdout, LOG_COLOR_MAGENTA "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

			else fprintf (stdout, LOG_COLOR_MAGENTA "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
		} break;
		
		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (stdout, LOG_COLOR_CYAN "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

			else fprintf (stdout, LOG_COLOR_CYAN "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: fprintf (stdout, LOG_COLOR_BLUE "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_EVENT: fprintf (stdout, LOG_COLOR_MAGENTA "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

		default: fprintf (stdout, "%s: %s\n", log->header, log->message); break;
	}

}

static void client_log_internal_normal_file (
	FILE *__restrict __stream,
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: fprintf (__stream, "%s\n", log->message); break;

		case LOG_TYPE_ERROR: fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_WARNING: fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_SUCCESS: fprintf (__stream, "%s: %s\n", log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE) fprintf (__stream, "%s%s: %s\n", log->header, log->second, log->message);
			else fprintf (__stream,  "%s: %s\n", log->header, log->message);
		} break;
		
		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE) fprintf (__stream, "%s%s: %s\n", log->header, log->second, log->message);
			else fprintf (__stream, "%s: %s\n", log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_EVENT: fprintf (__stream, "%s: %s\n", log->header, log->message); break;

		default: fprintf (__stream, "%s: %s\n", log->header, log->message); break;
	}

}

static void client_log_internal_normal (
	FILE *__restrict __stream,
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (log_output_type) {
		case LOG_OUTPUT_TYPE_STD: client_log_internal_normal_std (log, first_type, second_type); break;
		case LOG_OUTPUT_TYPE_FILE: client_log_internal_normal_file (__stream, log, first_type, second_type); break;

		case LOG_OUTPUT_TYPE_BOTH:
			client_log_internal_normal_std (log, first_type, second_type);
			client_log_internal_normal_file (__stream, log, first_type, second_type);
			break;

		default: break;
	}

}

static void client_log_internal_with_time_std (
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: fprintf (stdout, "[%s]: %s\n", log->datetime, log->message); break;

		case LOG_TYPE_ERROR: fprintf (stderr, "[%s]" LOG_COLOR_RED "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_WARNING: fprintf (stderr, "[%s]" LOG_COLOR_YELLOW "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: fprintf (stdout, "[%s]" LOG_COLOR_GREEN "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s" LOG_COLOR_RESET "%s: %s\n", log->datetime, log->header, log->second, log->message);

			else fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s: " LOG_COLOR_RESET "%s\n", log->datetime, log->header, log->message);
		} break;
		
		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (stdout, "[%s]" LOG_COLOR_CYAN "%s" LOG_COLOR_RESET "%s: %s\n", log->datetime, log->header, log->second, log->message);

			else fprintf (stdout, "[%s]" LOG_COLOR_CYAN "%s: " LOG_COLOR_RESET "%s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: fprintf (stdout, "[%s]" LOG_COLOR_BLUE "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_EVENT: fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;

		default: fprintf (stdout, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
	}

}

static void client_log_internal_with_time_file (
	FILE *__restrict __stream,
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: fprintf (__stream, "[%s]: %s\n", log->datetime, log->message); break;

		case LOG_TYPE_ERROR: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_WARNING: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (__stream, "[%s]%s%s: %s\n", log->datetime, log->header, log->second, log->message);

			else fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message);
		} break;
		
		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				fprintf (__stream, "[%s]%s%s: %s\n", log->datetime, log->header, log->second, log->message);

			else fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_EVENT: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;

		default: fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
	}

}

static void client_log_internal_with_time (
	FILE *__restrict __stream,
	ClientLog *log,
	LogType first_type, LogType second_type
) {

	switch (log_output_type) {
		case LOG_OUTPUT_TYPE_STD: client_log_internal_with_time_std (log, first_type, second_type); break;
		case LOG_OUTPUT_TYPE_FILE: client_log_internal_with_time_file (__stream, log, first_type, second_type); break;

		case LOG_OUTPUT_TYPE_BOTH:
			client_log_internal_with_time_std (log, first_type, second_type);
			client_log_internal_with_time_file (__stream, log, first_type, second_type);
			break;

		default: break;
	}

}

static void client_log_internal (
	FILE *__restrict __stream,
	LogType first_type, LogType second_type,
	const char *format, va_list args
) {

	ClientLog *log = (ClientLog *) pool_pop (log_pool);
	if (log) {
		if (first_type != LOG_TYPE_NONE) client_log_header_create (log, first_type, second_type);
		(void) vsnprintf (log->message, LOG_MESSAGE_SIZE, format, args);

		if (log_time_type != LOG_TIME_TYPE_NONE) {
			time_t datetime = time (NULL);
			struct tm *timeinfo = use_local_time ? localtime (&datetime) : gmtime (&datetime);

			switch (log_time_type) {
				case LOG_TIME_TYPE_TIME: strftime (log->datetime, LOG_DATETIME_SIZE, "%T", timeinfo); break;
				case LOG_TIME_TYPE_DATE: strftime (log->datetime, LOG_DATETIME_SIZE, "%d/%m/%y", timeinfo); break;
				case LOG_TIME_TYPE_BOTH: strftime (log->datetime, LOG_DATETIME_SIZE, "%d/%m/%y - %T", timeinfo); break;

				default: break;
			}

			client_log_internal_with_time (__stream, log, first_type, second_type);
		}

		else {
			client_log_internal_normal (__stream, log, first_type, second_type);
		}

		pool_push (log_pool, log);
	}

}

#pragma endregion

#pragma region public

// creates and prints a message of custom types
// based on the first type, the message can be printed with colors to stdout
void cerver_log (
	LogType first_type, LogType second_type,
	const char *format, ...
) {

	if (format) {
		va_list args;
		va_start (args, format);

		client_log_internal (
			client_log_get_stream (first_type),
			first_type, second_type,
			format, args
		);

		va_end (args);
	}

}

// prints a message with no type, effectively making this a custom printf ()
void client_log_msg (const char *msg, ...) {

	if (msg) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_NONE),
			LOG_TYPE_NONE, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a red error message to stderr
void client_log_error (const char *msg, ...) {

	if (msg) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_ERROR),
			LOG_TYPE_ERROR, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a yellow warning message to stderr
void client_log_warning (const char *msg, ...) {

	if (msg) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_WARNING),
			LOG_TYPE_WARNING, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a green success message to stdout
void client_log_success (const char *msg, ...) {

	if (msg) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_SUCCESS),
			LOG_TYPE_SUCCESS, LOG_TYPE_NONE,
			msg, args
		);

		va_end (args);
	}

}

// prints a debug message to stdout
void client_log_debug (const char *msg, ...) {

	if (msg) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_DEBUG),
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

	if (!logs_pathname) {
		logs_pathname = str_new (LOG_DEFAULT_PATH);
	}

	switch (log_output_type) {
		case LOG_OUTPUT_TYPE_FILE:
		case LOG_OUTPUT_TYPE_BOTH: {
			char filename[1024] = { 0 };
			snprintf (filename, 1024, "%s/%ld.log", logs_pathname->str, time (NULL));

			logfile = fopen (filename, "w+");
			if (logfile) {
				fprintf (logfile, "\nCerver Version: %s\n", CLIENT_VERSION_NAME);
				fprintf (logfile, "Release Date & time: %s - %s\n", CLIENT_VERSION_DATE, CLIENT_VERSION_TIME);
				fprintf (logfile, "Author: %s\n\n", CLIENT_VERSION_AUTHOR);
			}

			else {
				fprintf (stderr, "\n\nFailed to open %s log file!\n", filename);
				perror ("Error");
				printf ("\n\n");
			}
		} break;

		default: break;
	}

}

void client_log_end (void) {

	if (logfile) {
		fclose (logfile);
		logfile = NULL;
	}

	str_delete (logs_pathname);

	pool_delete (log_pool);
	log_pool = NULL;

}

#pragma endregion