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
	char nbuf[40];

	strerror_r(err, nbuf, sizeof nbuf);
	ep_dbg_cprintf(Dbg, 4, "ep_thr_%s: %s\n", where, nbuf);
}

static void
printtrace(void *lock, const char *where)
{
	pthread_t self = pthread_self();

	ep_dbg_printf("ep_thr_%s %p [%p]\n", where, lock, self);
}

#define TRACE(lock, where)	\
		if (ep_dbg_test(Dbg, 90)) printtrace(lock, where)

void
_ep_thr_init(void)
{
	_EpThrUsePthreads = true;
}


/*
**  Mutex implementation
*/

int
ep_thr_mutex_init(EP_THR_MUTEX *mtx, int type)
{
	int err;
	pthread_mutexattr_t attr;

	TRACE(mtx, "mutex_init");
	if (!_EpThrUsePthreads)
		return 0;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, type);
	if ((err = pthread_mutex_init(mtx, &attr)) != 0)
		diagnose_thr_err(err, "mutex_init");
	pthread_mutexattr_destroy(&attr);
	return err;
}

int
ep_thr_mutex_destroy(EP_THR_MUTEX *mtx)
{
	int err;

	TRACE(mtx, "mutex_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_destroy(mtx)) != 0)
		diagnose_thr_err(err, "mutex_destroy");
	return err;
}

int
ep_thr_mutex_lock(EP_THR_MUTEX *mtx)
{
	int err;

	TRACE(mtx, "mutex_lock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_lock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_lock");
	return err;
}

int
ep_thr_mutex_trylock(EP_THR_MUTEX *mtx)
{
	int err;

	TRACE(mtx, "mutex_trylock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_trylock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_trylock");
	return err;
}

int
ep_thr_mutex_unlock(EP_THR_MUTEX *mtx)
{
	int err;

	TRACE(mtx, "mutex_unlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_unlock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_unlock");
	return err;
}


/*
**  Condition Variable implementation
*/

int
ep_thr_cond_init(EP_THR_COND *cv)
{
	int err;

	TRACE(cv, "cond_init");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_init(cv, NULL)) != 0)
		diagnose_thr_err(err, "cond_init");
	return err;
}

int
ep_thr_cond_destroy(EP_THR_COND *cv)
{
	int err;

	TRACE(cv, "cond_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_destroy(cv)) != 0)
		diagnose_thr_err(err, "cond_destroy");
	return err;
}

int
ep_thr_cond_signal(EP_THR_COND *cv)
{
	int err;

	TRACE(cv, "cond_signal");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_signal(cv)) != 0)
		diagnose_thr_err(err, "cond_signal");
	return err;
}

int
ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx)
{
	int err;

	TRACE(cv, "cond_wait");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_wait(cv, mtx)) != 0)
		diagnose_thr_err(err, "cond_wait");
	return err;
}

int
ep_thr_cond_broadcast(EP_THR_COND *cv)
{
	int err;

	TRACE(cv, "cond_broadcast");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_broadcast(cv)) != 0)
		diagnose_thr_err(err, "cond_broadcast");
	return err;
}


/*
**  Read/Write Lock implementation
*/

int
ep_thr_rwlock_init(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_init");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_init(rwl, NULL)) != 0)
		diagnose_thr_err(err, "rwlock_init");
	return err;
}

int
ep_thr_rwlock_destroy(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_destroy(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_destroy");
	return err;
}

int
ep_thr_rwlock_rdlock(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_rdlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_rdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_rdlock");
	return err;
}

int
ep_thr_rwlock_tryrdlock(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_tryrdlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_tryrdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_tryrdlock");
	return err;
}

int
ep_thr_rwlock_wrlock(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_wrlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_wrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_wrlock");
	return err;
}

int
ep_thr_rwlock_trywrlock(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_tryrwlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_trywrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_trywrlock");
	return err;
}

int
ep_thr_rwlock_unlock(EP_THR_RWLOCK *rwl)
{
	int err;

	TRACE(rwl, "rwlock_unlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_unlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_unlock");
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
