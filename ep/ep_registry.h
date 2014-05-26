/* vim: set ai sw=8 sts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_registry.h 289 2014-05-11 04:50:04Z eric $
***********************************************************************/

/***********************************************************************
**
**  VENDOR REGISTRY
**
**	Get your vendor registrations here....
**
**	There are 8192 vendor registries possible, allocated as shown
**	below (all numbers in hex):
**
**	  0000		reserved for generic status codes (e.g. "OK")
**	  0001		single use (one application, conflicts expected)
**	  0002-007F	available for local, unregistered use (e.g.,
**			separate modules within one large application)
**	  0080-01FF	available for internal corporate registry
**			(not registered here; may conflict between
**			organizations but not within an organization)
**	  0200-1EFF	available for global registry (this file)
**	  1F00		eplib itself
**	  1F01-FFE	reserved
**	  1FFF		must not be used: represents "all registries"
**			(EP_STAT_REGISTRY_ALL)
**
**	If you are not producing libraries for external use (i.e., where
**	conflicts might actually appear), please use the range from
**	0002-01FF.
**
**	To reserve a code from the global registry, send mail to
**	eplib-registry (AT) neophilic.com.
**
***********************************************************************/

#define EP_REGISTRY_GENERIC		0x0000	// reserved for generic codes
#define EP_REGISTRY_USER		0x0001	// reserved for user apps

#define EP_REGISTRY_NEOPHILIC		0x0200	// Neophilic Systems
#define EP_REGISTRY_SENDMAIL		0x0201	// Sendmail, Inc.
#define EP_REGISTRY_UCB			0x0202	// U.C. Berkeley
#define EP_REGISTRY_EPLIB		0x1F00	// for eplib
