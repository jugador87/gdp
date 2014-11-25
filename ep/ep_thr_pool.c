/* vim: set ai sw=8 sts=8 ts=8 :*/

/*
**  Thread Pools
*/

#include <ep.h>
#include <ep_dbg.h>
#include <ep_thr_pool.h>

#include <string.h>
#include <sys/queue.h>

static EP_DBG	Dbg = EP_DBG_INIT("libep.thr.pool", "Thread Pool");

struct twork
{
	STAILQ_ENTRY(twork)
			next;			// next work in list
	void		(*func)(void *);	// function to run
	void		*arg;			// argument to pass
};

static EP_THR_MUTEX	FreeTWorkMutex	EP_THR_MUTEX_INITIALIZER;
static STAILQ_HEAD(tworkq, twork)
			FreeTWork = STAILQ_HEAD_INITIALIZER(FreeTWork);

/*
**  Allocate/free work requests.
**
**	These are reused for efficiency, since they probably turn over
**	quickly.
*/

static struct twork *
twork_new(void)
{
	struct twork *tw;

	ep_thr_mutex_lock(&FreeTWorkMutex);
	if ((tw = STAILQ_FIRST(&FreeTWork)) != NULL)
		STAILQ_REMOVE_HEAD(&FreeTWork, next);
	ep_thr_mutex_unlock(&FreeTWorkMutex);
	if (tw == NULL)
		tw = ep_mem_zalloc(sizeof *tw);
	return tw;
}

static void
twork_free(struct twork *tw)
{
	ep_thr_mutex_lock(&FreeTWorkMutex);
	STAILQ_INSERT_HEAD(&FreeTWork, tw, next);
	ep_thr_mutex_unlock(&FreeTWorkMutex);
}



/*
**  Implementation of thread pool.
*/

struct ep_thr_pool
{
	EP_THR_MUTEX	mutex;
	EP_THR_COND	has_work;	// signaled when work becomes available
	int		free_threads;	// number of idle threads
	int		num_threads;	// number of running threads
	int		min_threads;	// minimum number of running threads
	int		max_threads;	// maximum number of running threads
	struct tworkq	work;		// active work list
};


/*
**  Worker thread
**
**	These wait for work, and when found they do something.
**	There's nothing particularly fancy here other than
**	counting the number of free (unused) threads so we can
**	implement the hysteresis in the size of the pool.
**
**	Note that when a work function returns the thread
**	immediately looks for more work.  It's better to not
**	sleep at all if possible to keep our cache hot.
*/

static void *
worker_thread(void *tp_)
{
	EP_THR_POOL *tp = tp_;

	// start searching for work
	for (;;)
	{
		struct twork *tw;

		// see if there is anything to do
		ep_thr_mutex_lock(&tp->mutex);
		tp->free_threads++;
		if (STAILQ_FIRST(&tp->work) == NULL)
		{
			// no, wait for something
			ep_thr_cond_wait(&tp->has_work, &tp->mutex);
			ep_thr_mutex_unlock(&tp->mutex);
			continue;
		}

		// yes, please run it! (but don't stay locked)
		tw = STAILQ_FIRST(&tp->work);
		STAILQ_REMOVE_HEAD(&tp->work, next);
		tp->free_threads--;
		ep_thr_mutex_unlock(&tp->mutex);

		tw->func(tw->arg);
		twork_free(tw);
	}

	// will never get here
	return NULL;
}


/*
**  TP_ADD_THREAD --- add a new thread to the pool
**
**  	Must be called when the pool is quiescent, that is, when
**  	being initialized or when locked.
*/

static void
tp_add_thread(EP_THR_POOL *tp)
{
	pthread_t thread;
	int err;

	ep_dbg_cprintf(Dbg, 18, "Adding thread to pool %p\n", tp);
	err = pthread_create(&thread, NULL, &worker_thread, tp);
	if (err != 0)
	{
		fprintf(stderr,
			"ep_thr_pool_init: pthread_create failed (%d)\n",
			err);
	}
	else
	{
		tp->num_threads++;
	}
}


/*
**  EP_THR_POOL_NEW --- create a new thread pool
**
**	min_threads threads will be created immediately (may be zero),
**	and threads will be created dynamically up to max_threads as
**	needed.  We'll also implement thread shutdown later, so that
**	idle threads eventually go away.
*/

EP_THR_POOL *
ep_thr_pool_new(int min_threads, int max_threads)
{
	EP_THR_POOL *tp;
	int i;

	tp = ep_mem_zalloc(sizeof *tp);
	tp->min_threads = min_threads;
	tp->max_threads = max_threads;
	ep_thr_mutex_init(&tp->mutex, EP_THR_MUTEX_DEFAULT);
	ep_thr_cond_init(&tp->has_work);
	STAILQ_INIT(&tp->work);

	for (i = 0; i < min_threads; i++)
		tp_add_thread(tp);

	return tp;
}


/*
**  EP_THR_POOL_RUN --- run function in worker thread
**
**	This basically just calls a function in a worker thread.
**	We make it a point to run these in a FIFO order.
*/

void
ep_thr_pool_run(EP_THR_POOL *tp, void (*func)(void *), void *arg)
{
	struct twork *tw = twork_new();

	ep_thr_mutex_lock(&tp->mutex);
	tw->func = func;
	tw->arg = arg;
	STAILQ_INSERT_TAIL(&tp->work, tw, next);

	// start up a new thread if needed and permitted
	if (tp->free_threads == 0 && tp->num_threads < tp->max_threads)
		tp_add_thread(tp);
	else
		ep_thr_cond_signal(&tp->has_work);
	ep_thr_mutex_unlock(&tp->mutex);
}
