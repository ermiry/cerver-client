#ifndef _CLIENT_UTILS_BASE64_H_
#define _CLIENT_UTILS_BASE64_H_

#include <stddef.h>

#include "client/config.h"

CLIENT_PUBLIC char *base64_encode (size_t *enclen, size_t len, unsigned char *data);

CLIENT_PUBLIC unsigned char *base64_decode (size_t *declen, size_t len, char *data);

#endif
