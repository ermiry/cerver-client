#ifndef _CENGINE_THREAD_H_
#define _CENGINE_THREAD_H_

#include <SDL2/SDL.h>

#include "client/os.h"

#if defined OS_LINUX
    #include <pthread.h>
#endif

#include "client/types/types.h"
#include "client/types/string.h"

#include "client/collections/dlist.h"

#define THREAD_OK   0

// creates a custom detachable thread (will go away on its own upon completion)
// returns 0 on success, 1 on error
extern u8 thread_create_detachable (pthread_t *thread, void *(*work) (void *), void *args);

// sets thread name from inisde it
extern int thread_set_name (const char *name);

#endif