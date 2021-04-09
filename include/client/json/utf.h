#ifndef _CLIENT_JSON_UTF_H_
#define _CLIENT_JSON_UTF_H_

#include <stddef.h>
#include <stdint.h>

#include "client/config.h"

#ifdef __cplusplus
extern "C" {
#endif

CLIENT_PUBLIC int utf8_encode (
	int32_t codepoint, char *buffer, size_t *size
);

CLIENT_PUBLIC size_t utf8_check_first (char byte);

CLIENT_PUBLIC size_t utf8_check_full (
	const char *buffer, size_t size, int32_t *codepoint
);

CLIENT_PUBLIC const char *utf8_iterate (
	const char *buffer, size_t bufsize, int32_t *codepoint
);

CLIENT_PUBLIC int utf8_check_string (
	const char *string, size_t length
);

#ifdef __cplusplus
}
#endif

#endif