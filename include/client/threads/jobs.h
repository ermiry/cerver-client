#ifndef _CLIENT_THREADS_JOBS_H_
#define _CLIENT_THREADS_JOBS_H_

#include <pthread.h>

#include "client/types/types.h"

#include "client/collections/dlist.h"
#include "client/collections/pool.h"

#include "client/config.h"

#include "client/threads/bsem.h"

#define JOB_QUEUE_POOL_INIT				16

#ifdef __cplusplus
extern "C" {
#endif

struct _Cerver;
struct _Connection;

struct _JobQueue;

typedef struct Job {

	u64 id;
	void (*work) (void *args);
	void *args;

} Job;

CLIENT_PUBLIC void *job_new (void);

CLIENT_PUBLIC void job_delete (void *job_ptr);

CLIENT_PUBLIC int job_comparator (
	const void *a, const void *b
);

CLIENT_PUBLIC Job *job_create (
	void (*work) (void *args), void *args
);

CLIENT_PUBLIC Job *job_get (struct _JobQueue *job_queue);

CLIENT_PUBLIC void job_reset (Job *job);

CLIENT_PUBLIC void job_return (
	struct _JobQueue *job_queue, Job *job
);

typedef struct JobHandler {

	struct _Cerver *cerver;
	struct _Connection *connection;

	pthread_cond_t *cond;
	pthread_mutex_t *mutex;

	bool done;

	void *data;
	void (*data_delete) (void *data_ptr);

} JobHandler;

CLIENT_PUBLIC void *job_handler_new (void);

CLIENT_PUBLIC void job_handler_delete (void *job_handler_ptr);

CLIENT_PUBLIC void *job_handler_create (void);

CLIENT_PUBLIC JobHandler *job_handler_get (struct _JobQueue *job_queue);

CLIENT_PUBLIC void job_handler_reset (JobHandler *job_handler);

// wake up waiting thread
CLIENT_PUBLIC void job_handler_signal (JobHandler *handler);

CLIENT_PUBLIC void job_handler_return (
	struct _JobQueue *job_queue, JobHandler *job_handler
);

CLIENT_PUBLIC void job_handler_wait (
	struct _JobQueue *job_queue,
	void *data, void (*data_delete) (void *data_ptr)
);

#define JOB_QUEUE_TYPE_MAP(XX)				\
	XX(0,	NONE, 		None)				\
	XX(1,	JOBS, 		Jobs)				\
	XX(2,	HANDLERS, 	Handlers)

typedef enum JobQueueType {

	#define XX(num, name, string) JOB_QUEUE_TYPE_##name = num,
	JOB_QUEUE_TYPE_MAP (XX)
	#undef XX

} JobQueueType;

struct _JobQueue {

	JobQueueType type;

	Pool *pool;

	DoubleList *queue;

	pthread_mutex_t *rwmutex;		// used for queue r/w access
	bsem *has_jobs;

	bool waiting;
	u64 requested_id;
	void *requested_job;

	bool running;
	pthread_t handler_thread_id;
	void (*handler) (void *data);

};

typedef struct _JobQueue JobQueue;

CLIENT_PUBLIC JobQueue *job_queue_new (void);

CLIENT_PUBLIC void job_queue_delete (void *job_queue_ptr);

CLIENT_PUBLIC JobQueue *job_queue_create (
	const JobQueueType type
);

CLIENT_PUBLIC void job_queue_set_handler (
	JobQueue *queue, void (*handler) (void *data)
);

// adds a new job to the queue
// returns 0 on success, 1 on error
CLIENT_PUBLIC unsigned int job_queue_push (
	JobQueue *job_queue, void *job_ptr
);

// creates & adds a new job to the queue
// returns 0 on success, 1 on error
CLIENT_PUBLIC unsigned int job_queue_push_job (
	JobQueue *job_queue,
	void (*work) (void *args), void *args
);

CLIENT_PUBLIC unsigned int job_queue_push_job_with_id (
	JobQueue *job_queue,
	const u64 job_id,
	void (*work) (void *args), void *args
);

// creates & adds a new handler to the queue
// returns 0 on success, 1 on error
CLIENT_PUBLIC unsigned int job_queue_push_handler (
	JobQueue *job_queue,
	struct _Cerver *cerver,
	struct _Connection *connection,
	void *data, void (*data_delete) (void *data_ptr)
);

// get the job at the start of the queue
CLIENT_PUBLIC void *job_queue_pull (JobQueue *job_queue);

// requests to get an specific job from the queue by matching id
// blocks and waits until the requested job is available
CLIENT_PUBLIC void *job_queue_request (
	JobQueue *job_queue, const u64 job_id
);

CLIENT_PUBLIC unsigned int job_queue_start (JobQueue *job_queue);

CLIENT_PUBLIC unsigned int job_queue_stop (JobQueue *job_queue);

// clears the job queue -> destroys all jobs
CLIENT_PUBLIC void job_queue_clear (JobQueue *job_queue);

#ifdef __cplusplus
}
#endif

#endif