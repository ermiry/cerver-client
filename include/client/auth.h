#ifndef _CLIENT_AUTH_H_
#define _CLIENT_AUTH_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TOKEN_SIZE         256

// serialized session id - token
struct _SToken {

	char token[TOKEN_SIZE];

};

typedef struct _SToken SToken;

#ifdef __cplusplus
}
#endif

#endif