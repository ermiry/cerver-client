#ifndef _CLIENT_TIMER_H_
#define _CLIENT_TIMER_H_

#include <time.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timespec TimeSpec;

CLIENT_EXPORT void timespec_delete (void *timespec_ptr);

CLIENT_EXPORT TimeSpec *timer_get_timespec (void);

CLIENT_EXPORT double timer_elapsed_time (TimeSpec *start, TimeSpec *end);

CLIENT_EXPORT void timer_sleep_for_seconds (double seconds);

CLIENT_EXPORT double timer_get_current_time (void);

CLIENT_EXPORT struct tm *timer_get_gmt_time (void);

CLIENT_EXPORT struct tm *timer_get_local_time (void);

// returns a string representing the 24h time
CLIENT_EXPORT String *timer_time_to_string (
	const struct tm *timeinfo
);

// returns a string with day/month/year
CLIENT_EXPORT String *timer_date_to_string (
	const struct tm *timeinfo
);

// returns a string with day/month/year - 24h time
CLIENT_EXPORT String *timer_date_and_time_to_string (
	const struct tm *timeinfo
);

// returns a string representing the time with custom format
CLIENT_EXPORT String *timer_time_to_string_custom (
	const struct tm *timeinfo, const char *format
);

#ifdef __cplusplus
}
#endif

#endif