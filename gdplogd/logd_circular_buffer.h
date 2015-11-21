/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**	----- BEGIN LICENSE BLOCK -----
**	GDPLOGD: Log Daemon for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
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
