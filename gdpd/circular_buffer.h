#ifndef CIRCULAR_BUFFER_H_INCLUDED
#define CIRCULAR_BUFFER_H_INCLUDED

#include <stddef.h>

typedef struct LONG_LONG_PAIR
{
	long long key;
	long long value;
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
circular_buffer_search(CIRCULAR_BUFFER *circular_buffer, long long key);

void
circular_buffer_foreach(
	CIRCULAR_BUFFER *circular_buffer,
	CIRCULAR_BUFFER_FOREACH_FUNC foreach_func);

#endif
