#ifndef _CLIENT_THREADS_H_
#define _CLIENT_THREADS_H_

#include <pthread.h>

#include "client/types/types.h"

#include "client/config.h"

#define THREAD_OK						0

#define THREAD_NAME_BUFFER_SIZE			64

#ifdef __cplusplus
extern "C" {
#endif

#pragma region threads

// creates a custom detachable thread (will go away on its own upon completion)
// returns 0 on success, 1 on error
CLIENT_PUBLIC u8 thread_create_detachable (
	pthread_t *thread,
	void *(*work) (void *), void *args
);

// sets thread name from inisde it
CLIENT_PUBLIC unsigned int thread_set_name (
	const char *name, ...
);

#pragma endregion

#pragma region mutex

// allocates & initializes a new mutex that should be deleted after use
CLIENT_PUBLIC pthread_mutex_t *pthread_mutex_new (void);

// destroys & frees an allocated mutex
CLIENT_PUBLIC void pthread_mutex_delete (pthread_mutex_t *mutex);

#pragma endregion

#pragma region cond

// allocates & initializes a new cond that should be deleted after use
CLIENT_PUBLIC pthread_cond_t *pthread_cond_new (void);

// destroys & frees an allocated cond
CLIENT_PUBLIC void pthread_cond_delete (pthread_cond_t *cond);

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif