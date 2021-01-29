#ifndef _CLIENT_THREADS_JOBS_H_
#define _CLIENT_THREADS_JOBS_H_

#include <pthread.h>

#include "client/collections/dlist.h"

#include "client/config.h"

#include "client/threads/bsem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Job {

	// struct Job *prev;
	void (*method) (void *args);
	void *args;

} Job;

CLIENT_PUBLIC Job *job_new (void);

CLIENT_PUBLIC void job_delete (void *job_ptr);

CLIENT_PUBLIC Job *job_create (
	void (*method) (void *args), void *args
);

typedef struct JobQueue {

	// Job *front;
	// Job *rear;

	// size_t size;

	DoubleList *queue;

	pthread_mutex_t *rwmutex;             // used for queue r/w access
	bsem *has_jobs;

} JobQueue;

CLIENT_PUBLIC JobQueue *job_queue_new (void);

CLIENT_PUBLIC void job_queue_delete (void *job_queue_ptr);

CLIENT_PUBLIC JobQueue *job_queue_create (void);

// add a new job to the queue
// returns 0 on success, 1 on error
CLIENT_PUBLIC int job_queue_push (JobQueue *job_queue, Job *job);

// get the job at the start of the queue
CLIENT_PUBLIC Job *job_queue_pull (JobQueue *job_queue);

// clears the job queue -> destroys all jobs
CLIENT_PUBLIC void job_queue_clear (JobQueue *job_queue);

#ifdef __cplusplus
}
#endif

#endif