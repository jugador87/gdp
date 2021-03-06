/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  GDP Library Version
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

/**********************************************************************
**
**  VERSION NUMBER
**
**	Patches shouldn't include new functionality, just bug fixes.
**
**********************************************************************/

#ifndef _GDP_VERSION_H_
#define _GDP_VERSION_H_

#include <ep/ep.h>

// change these as necessary
#define GDP_VERSION_MAJOR	0
#define GDP_VERSION_MINOR	6
#define GDP_VERSION_PATCH	1

#define __GDP_STRING(x)		#x
#define __GDP_VER_CONCAT(a, b, c)							\
				__GDP_STRING(a) "." __GDP_STRING(b) "." __GDP_STRING(c)
#define GDP_VER_STRING										\
				__GDP_VER_CONCAT(GDP_VERSION_MAJOR,			\
						GDP_VERSION_MINOR,					\
						GDP_VERSION_PATCH)

#define GDP_LIB_VERSION										\
				((GDP_VERSION_MAJOR << 16) |				\
				 (GDP_VERSION_MINOR << 8) |					\
				 (GDP_VERSION_PATCH      ))

extern const char	GdpVersion[];

#endif
