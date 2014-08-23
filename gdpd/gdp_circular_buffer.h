/* vim: set ai sw=4 sts=4 ts=4 : */

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
