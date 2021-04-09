#include "client/config.h"

#include "client/json/config.h"
#include "client/json/json.h"

CLIENT_INLINE json_t *json_incref (json_t *json) {

	if (json && json->refcount != (size_t) - 1)
		JSON_INTERNAL_INCREF(json);
	
	return json;

}

CLIENT_INLINE void json_decref (json_t *json) {

	if (
		json
		&& json->refcount != (size_t) - 1
		&& JSON_INTERNAL_DECREF(json) == 0
	) {
		json_delete(json);
	}

}

#if defined(__GNUC__) || defined(__clang__)
CLIENT_INLINE void json_decrefp (json_t **json) {

	if (json) {
		json_decref(*json);
		*json = NULL;
	}

}

#endif