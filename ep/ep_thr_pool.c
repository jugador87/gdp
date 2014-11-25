/* vim: set ai sw=8 sts=8 ts=8 :*/

/*
**  Thread Pools
*/

#include <ep.h>
#include <ep_dbg.h>
#include <ep_thr.h>

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

struct thr_pool
{
	EP_THR_MUTEX	mutex;
	EP_THR_COND	has_work;	// signaled when work becomes available
	int		free_threads;	// number of idle threads
	int		num_threads;	// number of running threads
	int		min_threads;	// minimum number of running threads
	int		max_threads;	// maximum number of running threads
	struct tworkq	work;		// active work list
};

static struct thr_pool		Pool;	// the pool!

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
worker_thread(void *a)
{
	// start searching for work
	for (;;)
	{
		struct twork *tw;

		// see if there is anything to do
		ep_thr_mutex_lock(&Pool.mutex);
		Pool.free_threads++;
		if (STAILQ_FIRST(&Pool.work) == NULL)
		{
			// no, wait for something
			ep_thr_cond_wait(&Pool.has_work, &Pool.mutex);
			ep_thr_mutex_unlock(&Pool.mutex);
			continue;
		}

		// yes, please run it! (but don't stay locked)
		tw = STAILQ_FIRST(&Pool.work);
		STAILQ_REMOVE_HEAD(&Pool.work, next);
		Pool.free_threads--;
		ep_thr_mutex_unlock(&Pool.mutex);

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
tp_add_thread(void)
{
	pthread_t thread;
	int err;

	ep_dbg_cprintf(Dbg, 18, "Adding thread to pool\n");
	err = pthread_create(&thread, NULL, &worker_thread, NULL);
	if (err != 0)
	{
		fprintf(stderr,
			"ep_thr_pool_init: pthread_create failed (%d)\n",
			err);
	}
	else
	{
		Pool.num_threads++;
	}
}


/*
**  EP_THR_POOL_INIT --- initialize the thread pool
**
**	min_threads threads will be created immediately (may be zero),
**	and threads will be created dynamically up to max_threads as
**	needed.  We'll also implement thread shutdown later, so that
**	idle threads eventually go away.
*/

void
ep_thr_pool_init(int min_threads, int max_threads, uint32_t flags)
{
	int i;

	Pool.min_threads = min_threads;
	Pool.max_threads = max_threads;
	ep_thr_mutex_init(&Pool.mutex, EP_THR_MUTEX_DEFAULT);
	ep_thr_cond_init(&Pool.has_work);
	STAILQ_INIT(&Pool.work);

	for (i = 0; i < min_threads; i++)
		tp_add_thread();
}


/*
**  EP_THR_POOL_RUN --- run function in worker thread
**
**	This basically just calls a function in a worker thread.
**	We make it a point to run these in a FIFO order.
*/

void
ep_thr_pool_run(void (*func)(void *), void *arg)
{
	struct twork *tw = twork_new();

	ep_thr_mutex_lock(&Pool.mutex);
	tw->func = func;
	tw->arg = arg;
	STAILQ_INSERT_TAIL(&Pool.work, tw, next);

	// start up a new thread if needed and permitted
	if (Pool.free_threads == 0 && Pool.num_threads < Pool.max_threads)
		tp_add_thread();
	else
		ep_thr_cond_signal(&Pool.has_work);
	ep_thr_mutex_unlock(&Pool.mutex);
}
