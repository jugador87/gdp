/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2014, Eric P. Allman.  All rights reserved.
***********************************************************************/

#include <ep.h>
#include <ep_string.h>

/*
**  EP_LIB_INIT --- initialize the library
*/

extern void	_ep_stat_init(void);
extern void	_ep_thr_init(void);

bool	_EpLibInitialized = false;

EP_STAT
ep_lib_init(uint32_t flags)
{
	if (_EpLibInitialized)
		return EP_STAT_OK;
	if (EP_UT_BITSET(EP_LIB_USEPTHREADS, flags))
		_ep_thr_init();
	_ep_stat_init();
	ep_adm_readparams("defaults");
	ep_str_vid_set(NULL);
	ep_str_char_set(NULL);
	_EpLibInitialized = true;
	return EP_STAT_OK;
}
