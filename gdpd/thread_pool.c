/* vim: set ai sw=4 sts=4 ts=4 :*/

#include "thread_pool.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void *
worker_thread_routine(void *tp_)
{
	thread_pool *tp = tp_;
	thread_pool_job *new_job;

	while (true)
	{
		ep_thr_mutex_lock(&tp->mutex);
		while (tp->new_job == NULL)
		{
			ep_thr_cond_wait(&tp->is_full, &tp->mutex);
		}
		new_job = tp->new_job;
		tp->new_job = NULL;
		ep_thr_cond_signal(&tp->is_empty);
		ep_thr_mutex_unlock(&tp->mutex);

		new_job->callback(new_job->data);

		thread_pool_job_free(new_job);
	}

	return 0;
}

thread_pool *
thread_pool_new(int num_threads)
{
	thread_pool *new_tp = malloc(sizeof(thread_pool));

	new_tp->new_job = NULL;
	new_tp->num_threads = num_threads;
	ep_thr_mutex_init(&new_tp->mutex);
	ep_thr_cond_init(&new_tp->is_empty);
	ep_thr_cond_init(&new_tp->is_full);

	return new_tp;
}


void
thread_pool_init(thread_pool *pool)
{
	pthread_t thread;
	int err;
	int i;

	for (i = 0; i < pool->num_threads; ++i)
	{
		if ((err = pthread_create(&thread, NULL, &worker_thread_routine, pool))
		        != 0)
		{
			fprintf(stderr,
			        "thread_pool_init: pthread_create failed, err = %d\n", err);
		}
	}
}

void
thread_pool_add_job(thread_pool *tp, thread_pool_job *new_job)
{
	ep_thr_mutex_lock(&tp->mutex);
	while (tp->new_job != NULL)
	{
		ep_thr_cond_wait(&tp->is_empty, &tp->mutex);
	}
	tp->new_job = new_job;
	ep_thr_cond_signal(&tp->is_full);
	ep_thr_mutex_unlock(&tp->mutex);
}


LIST_HEAD(thread_list_head, thread_pool_job);

static EP_THR_MUTEX			FreeThreadMutex EP_THR_MUTEX_INITIALIZER;
static struct thread_list_head	FreeThreads = LIST_HEAD_INITIALIZER(FreeThreads);

thread_pool_job *
thread_pool_job_new(thread_pool_job_callback callback,
		void (*freefunc)(void *),
		void *data)
{
	thread_pool_job *new_job;
	
	// try to get one from the free list
	ep_thr_mutex_lock(&FreeThreadMutex);
	if ((new_job = LIST_FIRST(&FreeThreads)) != NULL)
	{
		LIST_REMOVE(new_job, list);
	}
	ep_thr_mutex_unlock(&FreeThreadMutex);

	if (new_job == NULL)
	{
		// free list empty; allocate a new one
		new_job = ep_mem_zalloc(sizeof *new_job);
		ep_thr_mutex_init(&new_job->mutex);
	}

	new_job->callback = callback;
	new_job->freefunc = freefunc;
	new_job->data = data;

	return new_job;
}


void
thread_pool_job_free(thread_pool_job *j)
{
	if (j->freefunc != NULL)
		j->freefunc(j->data);
	ep_thr_mutex_lock(&FreeThreadMutex);
	LIST_INSERT_HEAD(&FreeThreads, j, list);
	ep_thr_mutex_unlock(&FreeThreadMutex);
}
