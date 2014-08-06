/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#ifndef _EP_THR_H_
#define _EP_THR_H_

# include <ep.h>

# if EP_OSCF_USE_PTHREADS
#  include <pthread.h>

typedef pthread_mutex_t		EP_THR_MUTEX;
#  define	EP_THR_MUTEX_INITIALIZER	= PTHREAD_MUTEX_INITIALIZER
extern int	ep_thr_mutex_init(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_destroy(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_lock(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_trylock(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_unlock(EP_THR_MUTEX *mtx);

typedef pthread_cond_t		EP_THR_COND;
#  define	EP_THR_COND_INITIALIZER		= PTHREAD_COND_INITIALIZER
extern int	ep_thr_cond_init(EP_THR_COND *cv);
extern int	ep_thr_cond_destroy(EP_THR_COND *cv);
extern int	ep_thr_cond_signal(EP_THR_COND *cv);
extern int	ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx);
extern int	ep_thr_cond_broadcast(EP_THR_COND *cv);

typedef pthread_rwlock_t	EP_THR_RWLOCK;
#  define	EP_THR_RWLOCK_INITIALIZER	= PTHREAD_RWLOCK_INITIALIZER
extern int	ep_thr_rwlock_init(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_destroy(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_rdlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_tryrdlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_wrlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_trywrlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_unlock(EP_THR_RWLOCK *rwl);

# else // ! EP_OSCF_USE_PTHREADS

typedef int	EP_THR_MUTEX;
#  define	EP_THR_MUTEX_INITIALIZER
#  define	ep_thr_mutex_init(mtx)		0
#  define	ep_thr_mutex_destroy(mtx)	0
#  define	ep_thr_mutex_lock(mtx)		0
#  define	ep_thr_mutex_trylock(mtx)	0
#  define	ep_thr_mutex_unlock(mtx)	0

typedef int	EP_THR_COND;
#  define	EP_THR_COND_INITIALIZER
#  define	ep_thr_cond_init(cv)		0
#  define	ep_thr_cond_destroy(cv)		0
#  define	ep_thr_cond_signal(cv)		0
#  define	ep_thr_cond_wait(cv)		0
#  define	ep_thr_cond_broadcast(cv)	0

typedef int	EP_THR_RWLOCK;
#  define	EP_THR_RWLOCK_INITIALIZER
#  define	ep_thr_rwlock_init(rwl)		0
#  define	ep_thr_rwlock_destroy(rwl)	0
#  define	ep_thr_rwlock_rdlock(rwl)	0
#  define	ep_thr_rwlock_tryrdlock(rwl)	0
#  define	ep_thr_rwlock_wrlock(rwl)	0
#  define	ep_thr_rwlock_trywrlock(rwl)	0
#  define	ep_thr_rwlock_unlock(rwl)	0

# endif // EP_OSCF_USE_PTHREADS

#endif // _EP_THR_H_
