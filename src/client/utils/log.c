#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "client/types/string.h"

#include "client/collections/pool.h"

#include "client/files.h"
#include "client/version.h"

#include "client/threads/thread.h"

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

static LogOutputType log_global_output_type = LOG_OUTPUT_TYPE_STD;

static LogTimeType log_time_type = LOG_TIME_TYPE_NONE;
static bool use_local_time = false;

static String *logs_pathname = NULL;
static FILE *logfile = NULL;

static pthread_t update_log_thread_id = 0;
static bool update_log_file = false;
static unsigned int log_file_update_interval = LOG_DEFAULT_UPDATE_INTERVAL;

static bool quiet = false;

// returns the current log output type
LogOutputType client_log_get_output_type (void) {

	return log_global_output_type;

}

// sets the log output type to use
void client_log_set_output_type (LogOutputType type) {

	log_global_output_type = type;

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

// sets the interval in secs which will be used to sync the contents of the log file to disk
void client_log_set_update_interval (unsigned int interval) {

	log_file_update_interval = interval;

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

// if the log's quiet option is set to TRUE,
// only success, warning & error messages will be handled
// any other type will be ignored
void client_log_set_quiet (bool value) { quiet = value; }

#pragma endregion

#pragma region internal

static Pool *log_pool = NULL;

typedef struct {

	char datetime[LOG_DATETIME_SIZE];
	char header[LOG_HEADER_SIZE];
	char *second;

	char message[LOG_MESSAGE_SIZE];

} CerverLog;

static void *client_log_new (void) {

	CerverLog *log = (CerverLog *) malloc (sizeof (CerverLog));
	if (log) {
		(void) memset (log->datetime, 0, LOG_DATETIME_SIZE);
		(void) memset (log->header, 0, LOG_HEADER_SIZE);
		log->second = log->header + LOG_HEADER_HALF_SIZE;

		(void) memset (log->message, 0, LOG_MESSAGE_SIZE);
	}

	return log;

}

static void client_log_delete (void *client_log_ptr) {

	if (client_log_ptr) free (client_log_ptr);

}

static void client_log_header_create (
	CerverLog *log,
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
	CerverLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: (void) fprintf (stdout, "%s\n", log->message); break;

		case LOG_TYPE_ERROR: (void) fprintf (stderr, LOG_COLOR_RED "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_WARNING: (void) fprintf (stderr, LOG_COLOR_YELLOW "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: (void) fprintf (stdout, LOG_COLOR_GREEN "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (stdout, LOG_COLOR_MAGENTA "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

			else (void) fprintf (stdout, LOG_COLOR_MAGENTA "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
		} break;

		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (stdout, LOG_COLOR_CYAN "%s" LOG_COLOR_RESET "%s: %s\n", log->header, log->second, log->message);

			else (void) fprintf (stdout, LOG_COLOR_CYAN "%s: " LOG_COLOR_RESET "%s\n", log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: (void) fprintf (stdout, LOG_COLOR_BLUE "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;
		case LOG_TYPE_EVENT: (void) fprintf (stdout, LOG_COLOR_MAGENTA "%s: %s\n" LOG_COLOR_RESET, log->header, log->message); break;

		default: (void) fprintf (stdout, "%s: %s\n", log->header, log->message); break;
	}

}

static void client_log_internal_normal_file (
	FILE *__restrict __stream,
	CerverLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: (void) fprintf (__stream, "%s\n", log->message); break;

		case LOG_TYPE_ERROR: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_WARNING: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_SUCCESS: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE) (void) fprintf (__stream, "%s%s: %s\n", log->header, log->second, log->message);
			else (void) fprintf (__stream,  "%s: %s\n", log->header, log->message);
		} break;

		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE) (void) fprintf (__stream, "%s%s: %s\n", log->header, log->second, log->message);
			else (void) fprintf (__stream, "%s: %s\n", log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;
		case LOG_TYPE_EVENT: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;

		default: (void) fprintf (__stream, "%s: %s\n", log->header, log->message); break;
	}

}

static void client_log_internal_normal (
	FILE *__restrict __stream,
	CerverLog *log,
	LogType first_type, LogType second_type,
	LogOutputType log_output_type
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
	CerverLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: (void) fprintf (stdout, "[%s]: %s\n", log->datetime, log->message); break;

		case LOG_TYPE_ERROR: (void) fprintf (stderr, "[%s]" LOG_COLOR_RED "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_WARNING: (void) fprintf (stderr, "[%s]" LOG_COLOR_YELLOW "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: (void) fprintf (stdout, "[%s]" LOG_COLOR_GREEN "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s" LOG_COLOR_RESET "%s: %s\n", log->datetime, log->header, log->second, log->message);

			else (void) fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s: " LOG_COLOR_RESET "%s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (stdout, "[%s]" LOG_COLOR_CYAN "%s" LOG_COLOR_RESET "%s: %s\n", log->datetime, log->header, log->second, log->message);

			else (void) fprintf (stdout, "[%s]" LOG_COLOR_CYAN "%s: " LOG_COLOR_RESET "%s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: (void) fprintf (stdout, "[%s]" LOG_COLOR_BLUE "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;
		case LOG_TYPE_EVENT: (void) fprintf (stdout, "[%s]" LOG_COLOR_MAGENTA "%s: %s\n" LOG_COLOR_RESET, log->datetime, log->header, log->message); break;

		default: (void) fprintf (stdout, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
	}

}

static void client_log_internal_with_time_file (
	FILE *__restrict __stream,
	CerverLog *log,
	LogType first_type, LogType second_type
) {

	switch (first_type) {
		case LOG_TYPE_NONE: (void) fprintf (__stream, "[%s]: %s\n", log->datetime, log->message); break;

		case LOG_TYPE_ERROR: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_WARNING: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_SUCCESS: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;

		case LOG_TYPE_DEBUG: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (__stream, "[%s]%s%s: %s\n", log->datetime, log->header, log->second, log->message);

			else (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_TEST: {
			if (second_type != LOG_TYPE_NONE)
				(void) fprintf (__stream, "[%s]%s%s: %s\n", log->datetime, log->header, log->second, log->message);

			else (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message);
		} break;

		case LOG_TYPE_CERVER: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
		case LOG_TYPE_EVENT: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;

		default: (void) fprintf (__stream, "[%s]%s: %s\n", log->datetime, log->header, log->message); break;
	}

}

static void client_log_internal_with_time_actual (
	FILE *__restrict __stream,
	CerverLog *log,
	LogType first_type, LogType second_type,
	LogTimeType log_time_type,
	LogOutputType log_output_type
) {

	time_t datetime = time (NULL);
	struct tm *timeinfo = use_local_time ? localtime (&datetime) : gmtime (&datetime);

	switch (log_time_type) {
		case LOG_TIME_TYPE_TIME: (void) strftime (log->datetime, LOG_DATETIME_SIZE, "%T", timeinfo); break;
		case LOG_TIME_TYPE_DATE: (void) strftime (log->datetime, LOG_DATETIME_SIZE, "%d/%m/%y", timeinfo); break;
		case LOG_TIME_TYPE_BOTH: (void) strftime (log->datetime, LOG_DATETIME_SIZE, "%d/%m/%y - %T", timeinfo); break;

		default: break;
	}

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
	const char *format, va_list args,
	LogOutputType log_output_type
) {

	CerverLog *log = (CerverLog *) pool_pop (log_pool);
	if (log) {
		if (first_type != LOG_TYPE_NONE) client_log_header_create (log, first_type, second_type);
		(void) vsnprintf (log->message, LOG_MESSAGE_SIZE, format, args);

		if (log_time_type != LOG_TIME_TYPE_NONE) {
			client_log_internal_with_time_actual (
				__stream, log,
				first_type, second_type,
				log_time_type, log_output_type
			);
		}

		else {
			client_log_internal_normal (
				__stream, log,
				first_type, second_type,
				log_output_type
			);
		}

		(void) pool_push (log_pool, log);
	}

}

static void client_log_internal_with_time (
	FILE *__restrict __stream,
	LogType first_type, LogType second_type,
	const char *format, va_list args,
	LogOutputType log_output_type
) {

	CerverLog *log = (CerverLog *) pool_pop (log_pool);
	if (log) {
		if (first_type != LOG_TYPE_NONE) client_log_header_create (log, first_type, second_type);
		(void) vsnprintf (log->message, LOG_MESSAGE_SIZE, format, args);

		client_log_internal_with_time_actual (
			__stream, log,
			first_type, second_type,
			LOG_TIME_TYPE_BOTH, log_output_type
		);

		(void) pool_push (log_pool, log);
	}

}

static void client_log_internal_raw (
	FILE *__restrict __stream,
	const char *format, va_list args,
	LogOutputType log_output_type
) {

	CerverLog *log = (CerverLog *) pool_pop (log_pool);
	if (log) {
		(void) vsnprintf (log->message, LOG_MESSAGE_SIZE, format, args);

		switch (log_output_type) {
			case LOG_OUTPUT_TYPE_STD: (void) fprintf (stdout, "%s", log->message); break;
			case LOG_OUTPUT_TYPE_FILE: (void) fprintf (__stream, "%s", log->message); break;

			case LOG_OUTPUT_TYPE_BOTH: {
				(void) fprintf (stdout, "%s", log->message);
				(void) fprintf (__stream, "%s", log->message);
			} break;

			default: break;
		}

		(void) pool_push (log_pool, log);
	}

}

#pragma endregion

#pragma region public

// creates and prints a message of custom types
// based on the first type, the message can be printed with colors to stdout
void client_log (
	LogType first_type, LogType second_type,
	const char *format, ...
) {

	if (format) {
		va_list args;
		va_start (args, format);

		if (quiet) {
			switch (first_type) {
				case LOG_TYPE_ERROR:
				case LOG_TYPE_WARNING:
				case LOG_TYPE_SUCCESS:
					client_log_internal (
						client_log_get_stream (first_type),
						first_type, second_type,
						format, args,
						log_global_output_type
					);
					break;

				default: break;
			}
		}

		else {
			client_log_internal (
				client_log_get_stream (first_type),
				first_type, second_type,
				format, args,
				log_global_output_type
			);
		}

		va_end (args);
	}

}

// creates and prints a message of custom types
// and adds the date & time
// if the log_time_type has been configured, it will be kept
void client_log_with_date (
	LogType first_type, LogType second_type,
	const char *format, ...
) {

	va_list args;
		va_start (args, format);

		if (quiet) {
			switch (first_type) {
				case LOG_TYPE_ERROR:
				case LOG_TYPE_WARNING:
				case LOG_TYPE_SUCCESS:
					client_log_internal_with_time (
						client_log_get_stream (first_type),
						first_type, second_type,
						format, args,
						log_global_output_type
					);
					break;

				default: break;
			}
		}

		else {
			client_log_internal_with_time (
				client_log_get_stream (first_type),
				first_type, second_type,
				format, args,
				log_global_output_type
			);
		}

		va_end (args);

}

// creates and prints a message of custom types
// to stdout or stderr based on type
// and to log file if available
// this messages ignore the quiet flag
void client_log_both (
	LogType first_type, LogType second_type,
	const char *format, ...
) {

	if (format) {
		va_list args;
		va_start (args, format);

		client_log_internal (
			client_log_get_stream (first_type),
			first_type, second_type,
			format, args,
			logfile ? LOG_OUTPUT_TYPE_BOTH : LOG_OUTPUT_TYPE_STD
		);

		va_end (args);
	}

}

// prints a message with no type, effectively making this a custom printf ()
void client_log_msg (const char *msg, ...) {

	if (msg && !quiet) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_NONE),
			LOG_TYPE_NONE, LOG_TYPE_NONE,
			msg, args,
			log_global_output_type
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
			msg, args,
			log_global_output_type
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
			msg, args,
			log_global_output_type
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
			msg, args,
			log_global_output_type
		);

		va_end (args);
	}

}

// prints a debug message to stdout
void client_log_debug (const char *msg, ...) {

	if (msg && !quiet) {
		va_list args;
		va_start (args, msg);

		client_log_internal (
			client_log_get_stream (LOG_TYPE_DEBUG),
			LOG_TYPE_DEBUG, LOG_TYPE_NONE,
			msg, args,
			log_global_output_type
		);

		va_end (args);
	}

}

// prints a message with no type or format
void client_log_raw (const char *msg, ...) {

	if (msg && !quiet) {
		va_list args;
		va_start (args, msg);

		client_log_internal_raw (
			client_log_get_stream (LOG_TYPE_NONE),
			msg, args,
			log_global_output_type
		);

		va_end (args);
	}

}

// prints a line break, equivalent to printf ("\n")
void client_log_line_break (void) {

	switch (log_global_output_type) {
		case LOG_OUTPUT_TYPE_STD:
			(void) fprintf (stdout, "\n");
			break;
		case LOG_OUTPUT_TYPE_FILE:
			(void) fprintf (client_log_get_stream (LOG_TYPE_NONE), "\n");
			break;
		case LOG_OUTPUT_TYPE_BOTH:
			(void) fprintf (stdout, "\n");
			(void) fprintf (client_log_get_stream (LOG_TYPE_NONE), "\n");
			break;

		default: break;
	}

}

#pragma endregion

#pragma region main

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

static void *client_log_update (void *data) {

	(void) sleep (log_file_update_interval);

	while (update_log_file) {
		(void) fflush (logfile);
		(void) sleep (log_file_update_interval);
	}

	return NULL;

}

#pragma GCC diagnostic pop

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

	switch (log_global_output_type) {
		case LOG_OUTPUT_TYPE_FILE:
		case LOG_OUTPUT_TYPE_BOTH: {
			char filename[1024] = { 0 };
			(void) snprintf (filename, 1024, "%s/%ld.log", logs_pathname->str, time (NULL));

			logfile = fopen (filename, "w+");
			if (logfile) {
				update_log_file = true;
				(void) thread_create_detachable (&update_log_thread_id, client_log_update, NULL);
			}

			else {
				(void) fprintf (stderr, "\n\nFailed to open %s log file!\n", filename);
				perror ("Error");
				(void) fprintf (stderr, "\n\n");
			}
		} break;

		default: break;
	}

}

void client_log_end (void) {

	update_log_file = false;

	if (logfile) {
		(void) fclose (logfile);
		logfile = NULL;
	}

	str_delete (logs_pathname);

	pool_delete (log_pool);
	log_pool = NULL;

}

#pragma endregion