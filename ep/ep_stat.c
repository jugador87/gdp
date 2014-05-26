/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_stat.c 289 2014-05-11 04:50:04Z eric $
***********************************************************************/

/***********************************************************************
**
**  EP_STAT handling
**
***********************************************************************/

#include <ep.h>
#include <ep_stat.h>
#include <ep_pcvt.h>
#include <ep_dbg.h>
#include <ep_registry.h>
#include <string.h>

EP_SRC_ID("@(#)$Id: ep_stat.c 289 2014-05-11 04:50:04Z eric $");

/**************************  BEGIN PRIVATE  **************************/

struct EP_STAT_HANDLE
{
	EP_STAT_HANDLE		*next;	// next in the chain
	EP_STAT			stat;	// the status to look for
	EP_STAT			mask;	// a mask for that status
	EP_STAT_HANDLER_FUNCP	func;	// a function to call if found
};


//static EP_STAT_HANDLE	*StatFuncList;
//static EP_MUTEX		StatMutex	EP_MUTEX_INIT;

//static EP_DBG	Dbg = EP_DBG_INIT("libep.stat", "Status handling");

/***************************  END PRIVATE  ***************************/


#if 0

/***********************************************************************
**
**  EP_STAT_REGISTER -- register a status handler
**
**	Parameters:
**		stat -- the status to catch
**		mask -- bits in stat to match against
**		func -- function to invoke when this status matches
**
**	Returns:
**		A handle to refer to that handler (for deregistration).
*/

EP_STAT_HANDLE *
ep_stat_register(EP_STAT stat,
	EP_STAT mask,
	EP_STAT_HANDLER_FUNCP func)
{
	EP_STAT_HANDLE *c;

	EP_MUTEX_LOCK(StatMutex);

	// make sure we have a resource pool (this will never be freed)
	if (StatRpool == EP_NULL)
		StatRpool = ep_rpool_new("status list", 0);

	// search the current list to find a possible match
	for (c = StatFuncList; c != EP_NULL; c = c->next)
	{
		if (EP_STAT_IS_SAME(c->stat, stat) &&
		    EP_STAT_IS_SAME(c->mask, mask))
		{
			// exact match -- eliminate this slot
			ep_stat_unregister(c);
			c = EP_NULL;
			break;
		}
	}

	if (func == EP_NULL)
	{
		c = EP_NULL;
	}
	else if (c == EP_NULL)
	{
		// no slots -- allocate a new one
		c = ep_rpool_malloc(StatRpool, sizeof *c);

		c->next = StatFuncList;
		StatFuncList = c;
	}

	if (c != EP_NULL)
	{
		c->stat = stat;
		c->mask = mask;
		c->func = func;
	}

	EP_MUTEX_UNLOCK(StatMutex);

	return c;
}


/***********************************************************************
**
**  EP_STAT_UNREGISTER -- remove a previous registration
**
**	Currently uses a stupid O(N) algorithm.		XXX
**	Should save removed slots in a free list.	XXX
**
**	Parameters:
**		h -- the status handle to remove
**
**	Returns:
**		A status code telling whether it worked or not
*/

EP_STAT
ep_stat_unregister(EP_STAT_HANDLE *h)
{
	EP_STAT_HANDLE *c;
	EP_STAT_HANDLE **pc;
	EP_STAT stat = EP_STAT_OK;

	EP_MUTEX_LOCK(StatMutex);

	// search for the matching handle
	for (pc = &StatFuncList; (c = *pc) != EP_NULL && c != h; pc = &c->next)
		continue;

	if (c == EP_NULL)
	{
		char e1buf[40];

		stat = ep_stat_post(EP_STAT_STAT_NOTREGISTERED,
				"Unregistering unregistered status handler @0x%1",
				ep_pcvt_int(e1buf, sizeof e1buf, 16, (long) h),
				EP_NULL);
	}
	else
	{
		// remove from list
		// XXX should save this is a free list for reuse
		*pc = c->next;
	}

	EP_MUTEX_UNLOCK(StatMutex);
	return stat;
}


/***********************************************************************
**
**  EP_STAT_POST, EP_STAT_VPOST -- post a status
**
**	Parameters:
**		stat -- the status to post
**		defmsg -- the default message to use if you want to
**			print this and you can't find anything better
**		av, ... -- arguments of type char *
**
**	Returns:
**		The (possibly modified) status code to return
*/

EP_STAT
ep_stat_post(EP_STAT stat,
	const char *defmsg,
	...)
{
	va_list av;

	va_start(av, defmsg);
	stat = ep_stat_vpost(stat, defmsg, av);
	va_end(av);

	return stat;
}

EP_STAT
ep_stat_vpost(EP_STAT stat,
	const char *defmsg,
	va_list av)
{
	EP_STAT_HANDLE *c;

	if (ep_dbg_test(&Dbg, 1))
	{
		char sbuf[100];

		ep_stat_tostr(stat, sbuf, sizeof sbuf);
		ep_dbg_printf("ep_stat_vpost: posting %08x (%s)\n",
				EP_STAT_TO_INT(stat), sbuf);
	}

	EP_MUTEX_LOCK(StatMutex);

	for (c = StatFuncList; c != EP_NULL; c = c->next)
	{
		if (((EP_STAT_TO_INT(stat) ^ EP_STAT_TO_INT(c->stat)) &
		     EP_STAT_TO_INT(c->mask)) == 0)
			break;
	}

	if (c != EP_NULL)
	{
		// found -- call the function
		stat = (*c->func)(stat, defmsg, av);
		EP_MUTEX_UNLOCK(StatMutex);
	}
	else
	{
		// not found -- do the default
		EP_MUTEX_UNLOCK(StatMutex);
		if (EP_STAT_SEVERITY(stat) >
		    ep_adm_getintparam("libep.stat.post.warnsev",
					EP_STAT_SEV_WARN))
			ep_stat_vprint(stat, defmsg, EpStStderr, av);
	}

	if (EP_STAT_SEVERITY(stat) >=
	    ep_adm_getintparam("libep.stat.post.abortsev", EP_STAT_SEV_ABORT))
		ep_stat_abort(stat);

	return stat;
}



/***********************************************************************
**
**  EP_STAT_PRINT, EP_STAT_VPRINT -- print a status
**
**	Print a status to a given stream.  In the long run this
**	should use a message catalog, perhaps localized.  For now
**	it just uses the default (internal) message.
**
**	Parameters:
**		stat -- the status to print
**		defmsg -- the default message to use if nothing
**			found in catalog
**		av -- list of parameters to bind to the message
**		sp -- the stream on which to print
**
**	Returns:
**		none
*/

#define MAXARGS		10

void
ep_stat_print(EP_STAT stat,
	const char *defmsg,
	EP_STREAM *sp,
	...)
{
	va_list av;

	va_start(av, sp);
	ep_stat_vprint(stat, defmsg, sp, av);
	va_end(av);
}

void
ep_stat_vprint(EP_STAT stat,
	const char *defmsg,
	EP_STREAM *sp,
	va_list av)
{
	int i;
	const char *ap;
	const char *arglist[MAXARGS];
	char sbuf[100];

	i = 0;
	while (i < MAXARGS && (ap = va_arg(av, const char *)) != EP_NULL)
	{
		arglist[i++] = ap;
	}

	ep_st_fprintf(sp, "Status %s: ",
			ep_stat_tostr(stat, sbuf, sizeof sbuf));

	if (defmsg != EP_NULL)
		ep_st_pprint(sp, defmsg, i, arglist);
	else if (EP_STAT_IS_SAME(stat, EP_STAT_OK))
	{
		ep_st_fprintf(sp, "EP_ST_OK");
	}
	else
	{
		int j;

		ep_st_fprintf(sp, "Unknown stat message, args = ");
		if (i <= 0)
			ep_st_fprintf(sp, " <none>");
		for (j = 0; j < i; j++)
			ep_st_fprintf(sp, " %s", arglist[j]);
	}
	ep_st_putc(sp, '\n');
}

#endif // 0

/***********************************************************************
**
**  EP_STAT_ABORT -- abort process due to status
**
**	This should only be called after a call to ep_stat_[v]post
**	with an EP_STAT_SEV_ABORT or higher status.  It never returns.
**
**	Parameters:
**		stat -- the status code causing the abort
**
**	Returns:
**		never
*/

void
ep_stat_abort(EP_STAT stat)
{
	char sbuf[100];

	ep_st_fprintf(stderr,
		"Process aborted due to severe status: %s",
		ep_stat_tostr(stat, sbuf, sizeof sbuf));
	ep_assert_abort("Severe status");
	/*NOTREACHED*/
}


/***********************************************************************
**
**  EP_STAT_FROM_ERRNO -- create a status encoding errno
**
**  	Assumes errnos are positive integers.
**
**  	Parameters:
**  		uerrno -- the UNIX errno code
**
**  	Returns:
**  		An appropriately encoded status code
*/

EP_STAT
ep_stat_from_errno(int uerrno)
{
	return EP_STAT_NEW(EP_STAT_SEV_ERROR, EP_REGISTRY_EPLIB,
			EP_STAT_MOD_ERRNO, uerrno);
}


/***********************************************************************
**
**  EP_STAT_TOSTR -- return printable version of a status code
**
**	Currently has a few registries compiled in; these should be
**	driving in some other (extensible) way.
**
**	Parameters:
**		stat -- the status to convert
**		blen -- length of the output buffer
**		buf -- output buffer
**
**	Returns:
**		A string representation of the status.
**		(Will always be "buf")
*/

static char	*GenericErrors[] =
{
	"generic error",		// 0
	"out of memory",		// 1
	"arg out of range",		// 2
	"end of file",			// 3
};

char *
ep_stat_tostr(EP_STAT stat,
		char *buf,
		size_t blen)
{
	int reg = EP_STAT_REGISTRY(stat);
	char *pfx;
	char *detail = NULL;
	char *rname;
	char rbuf[50];

	// @@@ this really needs to be made configurable somehow
	switch (reg)
	{
	  case 0:
		rname = "INT-OK";
		break;

	  case EP_REGISTRY_NEOPHILIC:
		rname = "VND-neophilic";
		break;

	  case EP_REGISTRY_EPLIB:
		rname = "INT-eplib";
		switch (EP_STAT_MODULE(stat))
		{
		  case EP_STAT_MOD_ERRNO:
			detail = strerror(EP_STAT_DETAIL(stat));
			break;
		  case EP_STAT_MOD_GENERIC:
			if (EP_STAT_DETAIL(stat) <
			    (sizeof GenericErrors / sizeof *GenericErrors))
				detail = GenericErrors[EP_STAT_DETAIL(stat)];
		}
		break;

	  case EP_REGISTRY_USER:
		rname = "LOC-user";
		break;

	  default:
		if (reg <= 0x07f)
			pfx = "LOC";
		else if (reg <= 0x1ff)
			pfx = "ENT";
		else if (reg <= 0xeff)
			pfx = "VND";
		else
			pfx = "RSV";
		ep_st_sprintf(rbuf, sizeof rbuf, "%s-%d", pfx, reg);
		rname = rbuf;
		break;
	}

	if (EP_STAT_ISOK(stat))
	{
		ep_st_sprintf(buf, blen, "OK [%ld = 0x%lx]",
				EP_STAT_TO_INT(stat),
				EP_STAT_TO_INT(stat));
	}
	else if (detail != NULL)
	{
		ep_st_sprintf(buf, blen, "%s: %s",
				ep_stat_sev_tostr(EP_STAT_SEVERITY(stat)),
				detail);
	}
	else
	{
		ep_st_sprintf(buf, blen, "%s: [%s.%ld.%ld]",
				ep_stat_sev_tostr(EP_STAT_SEVERITY(stat)),
				rname,
				EP_STAT_MODULE(stat),
				EP_STAT_DETAIL(stat));
	}
	return buf;
}


/***********************************************************************
**
**  EP_STAT_SEV_TOSTR -- return printable version of a status severity
**
**	Parameters:
**		sev -- the severity to convert
**
**	Returns:
**		A string representation of the severity.
**
**	To Do:
**		Should be internationalized.
*/

const char *
ep_stat_sev_tostr(int sev)
{
	const char *const sevnames[8] =
	{
		"OK",			// 0
		"OK",			// 1
		"OK",			// 2
		"OK",			// 3
		"WARN",			// 4
		"ERROR",		// 5
		"SEVERE",		// 6
		"ABORT",		// 7
	};

	return sevnames[sev & 0x07];
}
