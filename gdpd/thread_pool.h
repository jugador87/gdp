/* vim: set ai sw=4 sts=4 ts=4 :*/

#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <ep/ep_thr.h>
#include <sys/queue.h>

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

LIST_HEAD(job_list, thread_pool_job);

typedef void	(*thread_pool_job_callback)(void *);

typedef struct thread_pool_job
{
	EP_THR_MUTEX				mutex;		// free list lock
	LIST_ENTRY(thread_pool_job)	list;		// free list
	thread_pool_job_callback	callback;	// func to call on launch
	void						*data;		// arg to callback
	void						(*freefunc)(void *); // call to free data
} thread_pool_job;

thread_pool		*thread_pool_new(int num_threads);

void			thread_pool_init(thread_pool *tp);

void			thread_pool_add_job(thread_pool *tp, thread_pool_job *new_job);

thread_pool_job	*thread_pool_job_new(thread_pool_job_callback callback,
						void (*freefunc)(void *),
						void *data);

void			thread_pool_job_free(thread_pool_job *j);

#endif // _THREAD_POOL_H_
