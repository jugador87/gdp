/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**  ----- END LICENSE BLOCK -----
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
#include <ep_hash.h>
#include <ep_thr.h>
#include <string.h>



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

	fprintf(stderr,
		"Process aborted due to severe status: %s",
		ep_stat_tostr(stat, sbuf, sizeof sbuf));
	ep_assert_abort("Severe status");
	/*NOTREACHED*/
}


/***********************************************************************
**
**  EP_STAT_FROM_ERRNO -- create a status encoding errno
**
**	Assumes errnos are positive integers < 2^10 (the size of
**		the detail part of a status code).
**	Should probably turn some errnos into temporary failures.
**
**	Parameters:
**		uerrno -- the UNIX errno code
**
**	Returns:
**		An appropriately encoded status code
*/

EP_STAT
ep_stat_from_errno(int uerrno)
{
	if (uerrno == 0)
		return EP_STAT_OK;
	return EP_STAT_NEW(EP_STAT_SEV_ERROR, EP_REGISTRY_EPLIB,
			EP_STAT_MOD_ERRNO, (unsigned long) uerrno);
}


/***********************************************************************
**
**  EP_STAT_REG_STRINGS -- register status code strings (for printing)
*/

static EP_HASH	*EpStatStrings;

void
ep_stat_reg_strings(struct ep_stat_to_string *r)
{
	if (EpStatStrings == NULL)
	{
		EpStatStrings = ep_hash_new("EP_STAT to string", NULL, 173);
		if (EpStatStrings == NULL)
			return;
	}

	while (r->estr != NULL)
	{
		(void) ep_hash_insert(EpStatStrings,
			sizeof r->estat, &r->estat, r->estr);
		r++;
	}
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
	char *module = NULL;
	char *rname;
	char rbuf[50];
	char mbuf[30];

	// dispose of OK status quickly
	if (EP_STAT_ISOK(stat))
	{
		if (EP_STAT_TO_INT(stat) == 0)
			snprintf(buf, blen, "OK");
		else
			snprintf(buf, blen, "OK [%d = 0x%x]",
				EP_STAT_TO_INT(stat),
				EP_STAT_TO_INT(stat));
		return buf;
	}

	// @@@ this really needs to be made configurable somehow
	switch (reg)
	{
	  case 0:
		rname = "GENERIC";
		break;

	  case EP_REGISTRY_NEOPHILIC:
		rname = "Neophilic";
		break;

	  case EP_REGISTRY_UCB:
		rname = "Berkeley";
		break;

	  case EP_REGISTRY_EPLIB:
		rname = "EPLIB";
		switch (EP_STAT_MODULE(stat))
		{
		  case EP_STAT_MOD_ERRNO:
			module = "errno";
			strerror_r(EP_STAT_DETAIL(stat), rbuf, sizeof rbuf);
			detail = rbuf;
			break;

		  case EP_STAT_MOD_GENERIC:
			module = "generic";
			if (EP_STAT_DETAIL(stat) <
			    (sizeof GenericErrors / sizeof *GenericErrors))
				detail = GenericErrors[EP_STAT_DETAIL(stat)];
			break;
		}
		break;

	  case EP_REGISTRY_USER:
		rname = "USER";
		break;

	  default:
		if (reg <= 0x07f)
			pfx = "LOCAL";
		else if (reg <= 0x0ff)
			pfx = "ENTERPRISE";
		else if (reg <= 0x6ff)
			pfx = "VND";
		else
			pfx = "RESERVED";
		snprintf(rbuf, sizeof rbuf, "%s-%d", pfx, reg);
		rname = rbuf;
		break;
	}

	// check to see if there is a string already
	if (module == NULL && EpStatStrings != NULL && !EP_STAT_ISOK(stat))
	{
		EP_STAT xstat;
		char *s;

		xstat = EP_STAT_NEW(EP_STAT_SEV_OK,
				EP_STAT_REGISTRY(stat),
				EP_STAT_MODULE(stat),
				0);
		module = ep_hash_search(EpStatStrings, sizeof xstat, &xstat);

		s = ep_hash_search(EpStatStrings, sizeof stat, &stat);
		if (s != NULL)
			detail = s;
	}
	if (module == NULL)
	{
		snprintf(mbuf, sizeof mbuf, "%d", EP_STAT_MODULE(stat));
		module = mbuf;
	}

	if (detail != NULL)
	{
		snprintf(buf, blen, "%s: %s [%s:%s:%d]",
				ep_stat_sev_tostr(EP_STAT_SEVERITY(stat)),
				detail,
				rname,
				module,
				EP_STAT_DETAIL(stat));
	}
	else
	{
		snprintf(buf, blen, "%s: [%s:%s:%d]",
				ep_stat_sev_tostr(EP_STAT_SEVERITY(stat)),
				rname,
				module,
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
		"WARNING",		// 4
		"ERROR",		// 5
		"SEVERE",		// 6
		"ABORT",		// 7
	};

	return sevnames[sev & 0x07];
}
