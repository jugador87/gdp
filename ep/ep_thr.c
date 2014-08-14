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

static bool	_EpThrUsePthreads = false;

/*
**  Helper routines
*/

static void
diagnose_thr_err(int err, const char *where)
{
	ep_dbg_cprintf(Dbg, 4, "ep_thr_%s: %s\n", where, strerror(err));
}

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

	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_mutex_init(mtx, NULL)) != 0)
		diagnose_thr_err(err, "mutex_init");
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
