#ifndef _CLIENT_TIMER_H_
#define _CLIENT_TIMER_H_

#ifndef __USE_POSIX199309
	#define __USE_POSIX199309
#endif

#include <time.h>

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timespec TimeSpec;

CLIENT_PUBLIC void timespec_delete (void *timespec_ptr);

CLIENT_PUBLIC TimeSpec *timer_get_timespec (void);

CLIENT_PUBLIC double timer_elapsed_time (TimeSpec *start, TimeSpec *end);

CLIENT_PUBLIC void timer_sleep_for_seconds (double seconds);

CLIENT_PUBLIC struct tm *timer_get_gmt_time (void);

CLIENT_PUBLIC struct tm *timer_get_local_time (void);

// returns a string representing the 24h time
CLIENT_PUBLIC String *timer_time_to_string (struct tm *timeinfo);

// returns a string with day/month/year
CLIENT_PUBLIC String *timer_date_to_string (struct tm *timeinfo);

// returns a string with day/month/year - 24h time
CLIENT_PUBLIC String *timer_date_and_time_to_string (struct tm *timeinfo);

// returns a string representing the time with custom format
CLIENT_PUBLIC String *timer_time_to_string_custom (
	struct tm *timeinfo, const char *format
);

#ifdef __cplusplus
}
#endif

#endif