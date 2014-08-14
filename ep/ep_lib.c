/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include <ep.h>

/*
**  EP_LIB_INIT --- initialize the library
*/

extern void	_ep_stat_init(void);
extern void	_ep_thr_init(void);

EP_STAT
ep_lib_init(uint32_t flags)
{
	if (EP_UT_BITSET(EP_LIB_USEPTHREADS, flags))
		_ep_thr_init();
	_ep_stat_init();
	return EP_STAT_OK;
}
