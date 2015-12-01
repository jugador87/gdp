/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
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

#ifndef _CIRCULAR_BUFFER_H_
#define _CIRCULAR_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

typedef struct LONG_LONG_PAIR
{
	int64_t key;
	int64_t value;
} LONG_LONG_PAIR;

typedef struct CIRCULAR_BUFFER
{
	size_t max_size;
	size_t current_size;
	LONG_LONG_PAIR *next_append;
	LONG_LONG_PAIR data[];
} CIRCULAR_BUFFER;

typedef void (*CIRCULAR_BUFFER_FOREACH_FUNC)(size_t, LONG_LONG_PAIR);

CIRCULAR_BUFFER *
circular_buffer_new(size_t max_size);

void
circular_buffer_free(CIRCULAR_BUFFER *circular_buffer);

void circular_buffer_append(
	CIRCULAR_BUFFER *circular_buffer,
	LONG_LONG_PAIR new_entry);

LONG_LONG_PAIR *
circular_buffer_search(CIRCULAR_BUFFER *circular_buffer, int64_t key);

void
circular_buffer_foreach(
	CIRCULAR_BUFFER *circular_buffer,
	CIRCULAR_BUFFER_FOREACH_FUNC foreach_func);

#endif // _CIRCULAR_BUFFER_H_
