#ifndef _CLIENT_UTILS_SHA256_H_
#define _CLIENT_UTILS_SHA256_H_

#include <stddef.h>
#include <stdint.h>

#include "client/config.h"

#define SHA256_STRING_LEN		80

#ifdef __cplusplus
extern "C" {
#endif

CLIENT_PUBLIC void sha256_calc (
	uint8_t hash[32], const void *input, size_t len
);

CLIENT_PUBLIC void sha256_hash_to_string (
	char string[], const uint8_t hash[32]
);

// generates a sha256 string from the input and places it in output
// output must be at least 65 bytes long
CLIENT_PUBLIC void sha256_generate (
	char *output, const char *input, const size_t inlen
);

// generates a sha256 string from the input
// returns a newly allocated string
CLIENT_PUBLIC char *sha256_generate_output (
	const char *input, const size_t inlen
);

#ifdef __cplusplus
}
#endif

#endif