#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/prctl.h>

#include "types/types.h"
#include "threads/thread.h"
#include "utils/log.h"
#include "utils/utils.h"

// creates a custom detachable thread (will go away on its own upon completion)
// handle manually in linux, with no name
// in any other platform, created with sdl api and you can pass a custom name
u8 thread_create_detachable (void *(*work)(void *), void *args) {

    u8 retval = 1;

    pthread_attr_t attr;
    pthread_t thread;

    int rc = pthread_attr_init (&attr);
    rc = pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create (&thread, &attr, work, args) != THREAD_OK)
        cengine_log_msg (stderr, LOG_ERROR, LOG_NO_TYPE, "Failed to create detachable thread!");
    else retval = 0;
   
    return retval;

}

// sets thread name from inisde it
int thread_set_name (const char *name) {

    int retval = 1;

    if (name) {
        retval = prctl (PR_SET_NAME, name);
    }

    return retval;

}