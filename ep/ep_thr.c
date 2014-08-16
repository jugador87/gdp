/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include <ep.h>
#include <ep_dbg.h>
#include <ep_thr.h>
#include <stdio.h>
#include <string.h>

#if EP_OSCF_USE_PTHREADS

static EP_DBG	Dbg = EP_DBG_INIT("ep.thr", "Threading support");

bool	_EpThrUsePthreads = false;	// also used by ep_dbg_*

/*
**  Helper routines
*/

static void
diagnose_thr_err(int err, const char *where)
{
	ep_dbg_cprintf(Dbg, 4, "ep_thr_%s: %s\n", where, strerror(err));
}

static void
printtrace(void *lock, int err, const char *where)
{
	pthread_t self = pthread_self();

	ep_dbg_printf("ep_thr_%s %p [%p]: %d\n", where, lock, self, err);
}

#define TRACE(lock, where, res)	\
		if (ep_dbg_test(Dbg, 60)) printtrace(lock, res, where)

void
_ep_thr_init(void)
{
	_EpThrUsePthreads = true;
}


/*
**  Mutex implementation
*/

int
ep_thr_mutex_init(EP_THR_MUTEX *mtx)
{
	int err;
	pthread_mutexattr_t attr;

	if (!_EpThrUsePthreads)
		return 0;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if ((err = pthread_mutex_init(mtx, &attr)) != 0)
		diagnose_thr_err(err, "mutex_init");
	TRACE(mtx, "mutex_init", err);
	return err;
}

int
ep_thr_mutex_destroy(EP_THR_MUTEX *mtx)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_destroy(mtx)) != 0)
		diagnose_thr_err(err, "mutex_destroy");
	TRACE(mtx, "mutex_destroy", err);
	return err;
}

int
ep_thr_mutex_lock(EP_THR_MUTEX *mtx)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_lock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_lock");
	TRACE(mtx, "mutex_lock", err);
	return err;
}

int
ep_thr_mutex_trylock(EP_THR_MUTEX *mtx)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_trylock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_trylock");
	TRACE(mtx, "mutex_trylock", err);
	return err;
}

int
ep_thr_mutex_unlock(EP_THR_MUTEX *mtx)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_unlock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_unlock");
	TRACE(mtx, "mutex_unlock", err);
	return err;
}


/*
**  Condition Variable implementation
*/

int
ep_thr_cond_init(EP_THR_COND *cv)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_init(cv, NULL)) != 0)
		diagnose_thr_err(err, "cond_init");
	TRACE(cv, "cond_init", err);
	return err;
}

int
ep_thr_cond_destroy(EP_THR_COND *cv)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_destroy(cv)) != 0)
		diagnose_thr_err(err, "cond_destroy");
	TRACE(cv, "cond_destroy", err);
	return err;
}

int
ep_thr_cond_signal(EP_THR_COND *cv)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_signal(cv)) != 0)
		diagnose_thr_err(err, "cond_signal");
	TRACE(cv, "cond_signal", err);
	return err;
}

int
ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_wait(cv, mtx)) != 0)
		diagnose_thr_err(err, "cond_wait");
	TRACE(cv, "cond_wait", err);
	return err;
}

int
ep_thr_cond_broadcast(EP_THR_COND *cv)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_broadcast(cv)) != 0)
		diagnose_thr_err(err, "cond_broadcast");
	TRACE(cv, "cond_broadcast", err);
	return err;
}


/*
**  Read/Write Lock implementation
*/

int
ep_thr_rwlock_init(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_init(rwl, NULL)) != 0)
		diagnose_thr_err(err, "rwlock_init");
	TRACE(rwl, "rwlock_init", err);
	return err;
}

int
ep_thr_rwlock_destroy(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_destroy(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_destroy");
	TRACE(rwl, "rwlock_destroy", err);
	return err;
}

int
ep_thr_rwlock_rdlock(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_rdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_rdlock");
	TRACE(rwl, "rwlock_rdlock", err);
	return err;
}

int
ep_thr_rwlock_tryrdlock(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_tryrdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_tryrdlock");
	TRACE(rwl, "rwlock_tryrdlock", err);
	return err;
}

int
ep_thr_rwlock_wrlock(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_wrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_wrlock");
	TRACE(rwl, "rwlock_wrlock", err);
	return err;
}

int
ep_thr_rwlock_trywrlock(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_trywrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_trywrlock");
	TRACE(rwl, "rwlock_tryrwlock", err);
	return err;
}

int
ep_thr_rwlock_unlock(EP_THR_RWLOCK *rwl)
{
	int err;

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_unlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_unlock");
	TRACE(rwl, "rwlock_unlock", err);
	return err;
}

#else // !EP_OSCF_USE_PTHREADS

void
_ep_thr_init(void)
{
	ep_dbg_printf("WARNING: initializing pthreads, "
			"but pthreads compiled out\n");
}

#endif // EP_OSCF_USE_PTHREADS
