/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_adm.c 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

/***********************************************************************
**
**  Administration routines
**
**	The defaults database (global parameters database?  surely
**	a better name can be found) stored various tunable
**	parameters.  The namespace is just strings, structured as
**	system.subsystem...parameter (i.e., it can be a variable
**	number of components, kind of like a MIB).
**
**	My thought is to use this something like the X Resources
**	database -- i.e., there is a search path.  For now, they
**	always return exact matches only.  They should also allow
**	multiple data files so that different subsystems can
**	easily include their own defaults.
**
**	This may be a candidate for osdep implementation -- e.g.,
**	using the registry on Windows-based systems.
**
***********************************************************************/

#include <ep.h>
#include <ep_hash.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <string.h>

EP_SRC_ID("@(#)$Id: ep_adm.c 252 2008-09-16 21:24:42Z eric $");

static const char *
getparamval(
	const char *pname)
{
	static EP_HASH *phash = NULL;
	static bool recurse = false;

	// don't allow recursive calls into this routine
	if (recurse)
		return NULL;
	recurse = true;

	// if we have no data, read it in
	if (phash == NULL)
	{
		FILE *fp = NULL;
		char *paramfile = getenv("APPLICATION_PARAMS");
		char lbuf[200];

		if (paramfile == NULL ||
		    (fp = fopen(paramfile, "r")) == NULL)
			goto fail0;

		phash = ep_hash_new("ep_adm hash", NULL, 97);

		while (fgets(lbuf, sizeof lbuf, fp) != NULL)
		{
			char *p;
			char *np;		// pointer to name
			char *vp;		// pointer to value

			p = lbuf;
			if (*p == '#')
				continue;	// comment
			p += strspn(p, " \t\n");	// trim leading wsp
			np = p;
			p += strcspn(p, " \t\n=");	// find end of name
			if (*p == '\0')
				continue;		// syntax: no value
			else if (*p == '=')
				*p++ = '\0';	// no wsp at end of name
			else
			{
				// trim white space from end of name
				*p++ = '\0';
				p += strspn(p, " \t\n");
				if (*p++ != '=')
					continue;	// syntax: bad name
			}
			p += strspn(p, " \t\n");	// skip wsp before val
			vp = p;
			p += strcspn(p, "\n");
			*p = '\0';

			// store it into the hash table
			ep_hash_insert(phash, strlen(np), np, ep_mem_strdup(vp));
		}

		fclose(fp);
		recurse = false;
	}

	return ep_hash_search(phash, strlen(pname), pname);

fail0:
	// leave recurse = true so future calls will just return null
	return NULL;
}



/***********************************************************************
**
**  EP_ADM_GETINTPARAM -- get integer parameter from defaults database
**
**	Parameters:
**		pname -- the name of the parameter
**		def -- the default value if not specified in the
**			database
**
**	Returns:
**		The value of the parameter or def
*/

int
ep_adm_getintparam(
	const char *pname,
	int def)
{
	const char *p = getparamval(pname);

	if (p == NULL)
		return def;
	else
		return atoi(p);
}


/***********************************************************************
**
**  EP_ADM_GETLONGPARAM -- get long int parameter from defaults database
**
**	Parameters:
**		pname -- the name of the parameter
**		def -- the default value if not specified in the
**			database
**
**	Returns:
**		The value of the parameter or def
*/

long
ep_adm_getlongparam(
	const char *pname,
	long def)
{
	const char *p = getparamval(pname);

	if (p == NULL)
		return def;
	else
		return atol(p);
}


/***********************************************************************
**
**  EP_ADM_GETBOOLPARAM -- get Boolean parameter from defaults database
**
**	Parameters:
**		pname -- the name of the parameter
**		def -- the default value if not specified in the
**			database
**
**	Returns:
**		The value of the parameter or def
*/

bool
ep_adm_getboolparam(
	const char *pname,
	bool def)
{
	const char *p = getparamval(pname);

	if (p == NULL)
		return def;
	if (strchr("1tTyY", *p) != NULL)
		return true;
	else
		return false;
}


/***********************************************************************
**
**  EP_ADM_GETSTRPARAM -- get string parameter from defaults database
**
**	Parameters:
**		pname -- the name of the parameter
**		def -- the default value if not specified in the
**			database
**
**	Returns:
**		The value of the parameter or def
*/

const char *
ep_adm_getstrparam(
	const char *pname,
	const char *def)
{
	const char *p = getparamval(pname);

	if (p == NULL)
		return def;
	else
		return p;
}


/***********************************************************************
**
**  Currently known parameters
**
**	eplib.rpool.quantum		Size of minimum memory chunk for rpools
**	eplib.stream.hfile.bsize	Default buffer size for host file I/O
**					buffers
**	eplib.stream.netsock.XXX
**
***********************************************************************/
