/*
 * Copyright (c) 2009-2011 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <client/json/json.h>

#include "json.h"
#include "errors.h"

struct my_source {
	const char *buf;
	size_t off;
	size_t cap;
};

static const char my_str[] = "[\"A\", {\"B\": \"C\", \"e\": false}, 1, null, \"foo\"]";

static size_t greedy_reader(void *buf, size_t buflen, void *arg) {
	struct my_source *s = (struct my_source *) arg;
	if (buflen > s->cap - s->off)
		buflen = s->cap - s->off;
	if (buflen > 0) {
		memcpy(buf, s->buf + s->off, buflen);
		s->off += buflen;
		return buflen;
	} else {
		return 0;
	}
}

static void start () {
	struct my_source s;
	json_t *json;
	json_error_t error;

	s.off = 0;
	s.cap = strlen(my_str);
	s.buf = my_str;

	json = json_load_callback(greedy_reader, &s, 0, &error);

	if (!json)
		fail("json_load_callback failed on a valid callback");
	json_decref(json);

	s.off = 0;
	s.cap = strlen(my_str) - 1;
	s.buf = my_str;

	json = json_load_callback(greedy_reader, &s, 0, &error);
	if (json) {
		json_decref(json);
		fail("json_load_callback should have failed on an incomplete stream, "
			 "but it didn't");
	}
	if (strcmp(error.source, "<callback>") != 0) {
		fail("json_load_callback returned an invalid error source");
	}
	if (strcmp(error.text, "']' expected near end of file") != 0) {
		fail("json_load_callback returned an invalid error message for an "
			 "unclosed top-level array");
	}

	json = json_load_callback(NULL, NULL, 0, &error);
	if (json) {
		json_decref(json);
		fail("json_load_callback should have failed on NULL load callback, but "
			 "it didn't");
	}
	if (strcmp(error.text, "wrong arguments") != 0) {
		fail("json_load_callback returned an invalid error message for a NULL "
			 "load callback");
	}
}

void json_tests_load_cb (void) {

	(void) printf ("Testing JSON load callback...\n");

	start ();

	(void) printf ("Done!\n");

}