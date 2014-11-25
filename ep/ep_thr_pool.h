/* vim: set ai sw=8 sts=8 ts=8 :*/

#ifndef _EP_THR_POOL_H_
#define _EP_THR_POOL_H_

#include <ep/ep_thr.h>

typedef struct ep_thr_pool	EP_THR_POOL;

// initialize a thread pool
EP_THR_POOL	*ep_thr_pool_new(
			int min_threads,	// min number of threads
			int max_threads);	// max number of threads

// run a function in a worker thread from the pool
void		ep_thr_pool_run(
			EP_THR_POOL *tp,	// the pool
			void (*func)(void *),	// the function
			void *arg);		// passed to func

#endif // _EP_THR_POOL_H_
