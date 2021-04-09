#ifndef _CLIENT_JSON_TYPES_H_
#define _CLIENT_JSON_TYPES_H_

#include "client/json/private.h"
#include "client/json/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum json_type {

	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_INTEGER,
	JSON_REAL,
	JSON_TRUE,
	JSON_FALSE,
	JSON_NULL

} json_type;

struct json_t {

	json_type type;
	volatile size_t refcount;

};

typedef struct json_t json_t;

typedef struct {

	json_t json;
	hashtable_t hashtable;

} json_object_t;

typedef struct {

	json_t json;
	size_t size;
	size_t entries;
	json_t **table;

} json_array_t;

typedef struct {

	json_t json;
	char *value;
	size_t length;

} json_string_t;

typedef struct {

	json_t json;
	double value;

} json_real_t;

typedef struct {

	json_t json;
	json_int_t value;

} json_integer_t;

#ifdef __cplusplus
}
#endif

#endif