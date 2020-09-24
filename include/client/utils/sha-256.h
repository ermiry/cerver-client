#ifndef _CLIENT_UTILS_SHA_256_H_
#define _CLIENT_UTILS_SHA_256_H_

#include <stdint.h>
#include <stddef.h>

#include "client/config.h"

CLIENT_PUBLIC void sha_256_calc (uint8_t hash[32], const void *input, size_t len);

CLIENT_PUBLIC void sha_256_hash_to_string (char string[65], const uint8_t hash[32]);

#endif