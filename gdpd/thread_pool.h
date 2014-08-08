#ifndef THREAD_POOL_H_INCLUDED
#define THREAD_POOL_H_INCLUDED

#include <ep/ep_thr.h>

//XXX This forward declaration is C11 only.  We are using C99.
//typedef struct thread_pool_job thread_pool_job;

typedef struct thread_pool
{
	EP_THR_MUTEX mutex;
	EP_THR_COND is_full;
	EP_THR_COND is_empty;
	int num_threads;
	struct thread_pool_job *new_job;
} thread_pool;

typedef void (*thread_pool_job_callback)(void *);

typedef struct thread_pool_job
{
	thread_pool_job_callback callback;
	void *data;
} thread_pool_job;

thread_pool *
thread_pool_new(int num_threads);

void
thread_pool_init(thread_pool *tp);

void
thread_pool_add_job(thread_pool *tp, thread_pool_job *new_job);

thread_pool_job *
thread_pool_job_new(thread_pool_job_callback callback, void *data);

#endif
