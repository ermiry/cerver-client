#ifndef _CLIENT_UTILS_H_
#define _CLIENT_UTILS_H_

#include <stdbool.h>
#include <stddef.h>

#include "client/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*** misc ***/

CLIENT_PUBLIC bool system_is_little_endian (void);

/*** math ***/

CLIENT_PUBLIC int clamp_int (int val, int min, int max);

CLIENT_PUBLIC int abs_int (int value);

CLIENT_PUBLIC float lerp (float first, float second, float by);

CLIENT_PUBLIC bool float_compare (float f1, float f2);

/*** random ***/

// init psuedo random generator based on our seed
CLIENT_PUBLIC void random_set_seed (unsigned int seed);

CLIENT_PUBLIC int random_int_in_range (int min, int max);

// abds = 5 for random float values between 0.0001 and 4.9999
CLIENT_PUBLIC float random_float (float abs);

/*** converters ***/

// convert a string representing a hex to a string
CLIENT_PUBLIC int xtoi (char *hexString);

CLIENT_PUBLIC char *itoa (int i, char *b);

/*** c strings ***/

// copies a c string into another one previuosly allocated
CLIENT_PUBLIC void c_string_copy (
	char *to, const char *from
);

// copies n bytes from a c string into another one previuosly allocated
CLIENT_PUBLIC void c_string_n_copy (
	char *to, const char *from, size_t n
);

// concats two c strings into a newly allocated buffer of len s1 + s2
// returns a newly allocated buffer on success, NULL on any error
CLIENT_PUBLIC char *c_string_concat (
	const char *s1, const char *s2, size_t *des_size
);

// concats two strings into the same buffer
// wont perform operation if result would overflow buffer
// returns the len of the final string
CLIENT_PUBLIC size_t c_string_concat_safe (
	const char *s1, const char *s2,
	const char *des, size_t des_size
);

// creates a new c string with the desired format, as in printf
CLIENT_PUBLIC char *c_string_create (const char *format, ...);

// removes all the spaces in the c string
CLIENT_PUBLIC void c_string_remove_spaces (char *s);

// removes any CRLF characters in a string
CLIENT_PUBLIC void c_string_remove_line_breaks (char *s);

// removes all spaces and CRLF in the c string
CLIENT_PUBLIC void c_string_remove_spaces_and_line_breaks (char *s);

// get how many tokens will be extracted by counting the number of apperances of the delim
// the original string won't be affected
CLIENT_PUBLIC size_t c_string_count_tokens (
	const char *original, const char delim
);

// splits a c string into tokens based on a delimiter
// the original string won't be affected
// this method is thread safe as it uses __strtok_r () instead of the regular strtok ()
CLIENT_PUBLIC char **c_string_split (
	const char *original, const char delim, size_t *n_tokens
);

// revers a c string
// returns a newly allocated c string
CLIENT_PUBLIC char *c_string_reverse (const char *str);

// removes all ocurrances of a char from a string
CLIENT_PUBLIC void c_string_remove_char (
	char *string, char garbage
);

// removes the exact sub string from the main one
// returns a newly allocated copy of the original str but withput the sub
CLIENT_PUBLIC char *c_string_remove_sub (
	char *str, const char *sub
);

// removes any white space from the string
CLIENT_PUBLIC char *c_string_trim (char *str);

// removes quotes from string
CLIENT_PUBLIC char *c_string_strip_quotes (char *str);

// returns true if the string starts with the selected sub string
CLIENT_PUBLIC bool c_string_starts_with (
	const char *str, const char *substr
);

// creates a newly allocated string using the data between the two pointers of the SAME string
// returns a new string, NULL on error
CLIENT_PUBLIC char *c_string_create_with_ptrs (
	char *first, char *last
);

// removes a substring from a c string that is defined after a token
// returns a newly allocated string without the sub,
// and option to retrieve the actual substring
CLIENT_PUBLIC char *c_string_remove_sub_after_token (
	char *str, const char token, char **sub
);

// removes a substring from a c string after the idex of the token
// returns a newly allocated string without the sub,
// and option to retrieve the actual substring
// idx set to -1 for the last token match
// example: /home/ermiry/Documents, token: '/', idx: -1, returns: Documents
CLIENT_PUBLIC char *c_string_remove_sub_after_token_with_idx (
	char *str, const char token, char **sub, int idx
);

// removes a substring from a c string delimited by two equal tokens
// takes the first and last appearance of the token
// example: test_20191118142101759__TEST__.png - token: '_'
// result: test.png
// returns a newly allocated string, and a option to get the substring
CLIENT_PUBLIC char *c_string_remove_sub_simetric_token (
	char *str, const char token, char **sub
);

// removes a substring from a c string delimitied by two equal tokens
// and you can select the idx of the token; use -1 for last token
// example: test_20191118142101759__TEST__.png - token: '_' - idx (first: 1,  last: 3)
// result: testTEST__.png
// returns a newly allocated string, and a option to get the substring
CLIENT_PUBLIC char *c_string_remove_sub_range_token (
	char *str,
	const char token, int first, int last,
	char **sub
);

#ifdef __cplusplus
}
#endif

#endif