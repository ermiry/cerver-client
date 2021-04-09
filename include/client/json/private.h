#ifndef _CLIENT_PRIVATE_H_
#define _CLIENT_PRIVATE_H_

#include <stdarg.h>
#include <stddef.h>

#include "client/config.h"

#include "client/json/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JANSSON_USING_CMAKE /* disabled if using cmake */
#if JSON_INTEGER_IS_LONG_LONG
#ifdef _WIN32
#define JSON_INTEGER_FORMAT "I64d"
#else
#define JSON_INTEGER_FORMAT "lld"
#endif
typedef long long json_int_t;
#else
#define JSON_INTEGER_FORMAT "ld"
typedef long json_int_t;
#endif /* JSON_INTEGER_IS_LONG_LONG */
#endif

/* do not call JSON_INTERNAL_INCREF or JSON_INTERNAL_DECREF directly */
#if JSON_HAVE_ATOMIC_BUILTINS
#define JSON_INTERNAL_INCREF(json)                                                       \
	__atomic_add_fetch(&json->refcount, 1, __ATOMIC_ACQUIRE)
#define JSON_INTERNAL_DECREF(json)                                                       \
	__atomic_sub_fetch(&json->refcount, 1, __ATOMIC_RELEASE)
#elif JSON_HAVE_SYNC_BUILTINS
#define JSON_INTERNAL_INCREF(json) __sync_add_and_fetch(&json->refcount, 1)
#define JSON_INTERNAL_DECREF(json) __sync_sub_and_fetch(&json->refcount, 1)
#else
#define JSON_INTERNAL_INCREF(json) (++json->refcount)
#define JSON_INTERNAL_DECREF(json) (--json->refcount)
#endif

/* If __atomic or __sync builtins are available the library is thread
 * safe for all read-only functions plus reference counting. */
#if JSON_HAVE_ATOMIC_BUILTINS || JSON_HAVE_SYNC_BUILTINS
#define JANSSON_THREAD_SAFE_REFCOUNT 1
#endif

#define container_of(ptr_, type_, member_)                                               \
	((type_ *)((char *)ptr_ - offsetof(type_, member_)))

struct json_t;

struct hashtable_t;

/* Create a string by taking ownership of an existing buffer */
CLIENT_PUBLIC struct json_t *jsonp_stringn_nocheck_own (
	const char *value, size_t len
);

CLIENT_PUBLIC int jsonp_dtostr (
	char *buffer, size_t size, double value, int prec
);

/* Wrappers for custom memory functions */
CLIENT_PUBLIC void *jsonp_malloc (
	size_t size
) CLIENT_ATTRS((warn_unused_result));

CLIENT_PUBLIC void jsonp_free (void *ptr);

CLIENT_PUBLIC char *jsonp_strndup (
	const char *str, size_t length
) CLIENT_ATTRS((warn_unused_result));

CLIENT_PUBLIC char *jsonp_strdup (
	const char *str
) CLIENT_ATTRS((warn_unused_result));

CLIENT_PUBLIC char *jsonp_strndup (
	const char *str, size_t len
) CLIENT_ATTRS((warn_unused_result));

/* Circular reference check*/
/* Space for "0x", double the sizeof a pointer for the hex and a terminator. */
#define LOOP_KEY_LEN (2 + (sizeof(struct json_t *) * 2) + 1)

CLIENT_PUBLIC int jsonp_loop_check (
	struct hashtable_t *parents,
	const struct json_t *json, char *key, size_t key_size
);

CLIENT_PUBLIC struct json_t *json_sprintf (
	const char *fmt, ...
) CLIENT_ATTRS((warn_unused_result, format(printf, 1, 2)));

CLIENT_PUBLIC struct json_t *json_vsprintf (
	const char *fmt, va_list ap
) CLIENT_ATTRS((warn_unused_result, format(printf, 1, 0)));

CLIENT_PUBLIC int json_equal (
	const struct json_t *value1, const struct json_t *value2
);

CLIENT_PUBLIC struct json_t *json_copy (
	struct json_t *value
) CLIENT_ATTRS((warn_unused_result));

CLIENT_PUBLIC struct json_t *json_deep_copy (
	const struct json_t *value
) CLIENT_ATTRS((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif