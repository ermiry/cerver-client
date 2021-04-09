#ifndef _CLIENT_JSON_INTERNAL_H_
#define _CLIENT_JSON_INTERNAL_H_

#include "client/config.h"

#include "client/json/config.h"
#include "client/json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

CLIENT_PUBLIC json_t *json_incref (json_t *json);

/* do not call json_delete directly */
CLIENT_PUBLIC void json_delete (json_t *json);

CLIENT_PUBLIC void json_decref (json_t *json);

#if defined(__GNUC__) || defined(__clang__)
CLIENT_PUBLIC void json_decrefp (json_t **json);

#define json_auto_t json_t __attribute__((cleanup(json_decrefp)))
#endif

#ifdef __cplusplus
}
#endif

#endif