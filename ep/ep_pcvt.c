/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008, Eric P. Allman.  All rights reserved.
**	$Id: ep_pcvt.c 252 2008-09-16 21:24:42Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_string.h>

EP_SRC_ID("@(#)$Id: ep_pcvt.c 252 2008-09-16 21:24:42Z eric $");

/*
**  PRINT CONVERSION ROUTINES
**
**	These are conversions from various formats to string, intended
**	for short-term use.  In all cases, the caller passes in a
**	buffer in which to store the results.  The value will be
**	trimmed as necessary to fit that area.
*/

const char *
ep_pcvt_str(
	char *obuf,
	size_t osize,
	const char *val)
{
	int vl;

	if (val == NULL)
	{
		strlcpy(obuf, "<NULL>", osize);
		return obuf;
	}

	vl = strlen(val);
	if (vl < osize)
		return val;
	strncpy(obuf, val, osize - 7);
	strlcat(obuf, "...", osize);
	strlcat(obuf, &val[vl - 3], osize);
	return obuf;
}


const char *
ep_pcvt_int(
	char *obuf,
	size_t osize,
	int base,
	long val)
{
	char *fmtstr;

	switch (base)
	{
	  case 8:
	  case -8:
		fmtstr = "%o";
		break;

	  case 16:
	  case -16:
		fmtstr = "%X";
		break;

	  case 10:
	  default:
		fmtstr = "%l";
		break;

	  case -10:
		fmtstr = "%ul";
		break;
	}

	snprintf(obuf, osize, fmtstr, val);
	return obuf;
}
