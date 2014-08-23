/* vim: set ai sw=4 sts=4 ts=4 : */

#include "gdp_circular_buffer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Specialized circular buffer implementation for an index cache that only
 * gets updated when a new record is appended to the GCL. This maintains the
 * invariant that the circular buffer is always sorted, assuming the key
 * (recno in this case) is always increasing.
 *
 */

CIRCULAR_BUFFER *
circular_buffer_new(size_t max_size)
{
	assert(max_size > 0);

	CIRCULAR_BUFFER *new_circular_buffer = malloc(
	        sizeof(CIRCULAR_BUFFER) + (max_size * sizeof(LONG_LONG_PAIR)));
	if (new_circular_buffer == NULL)
	{
		return NULL;
	}
	new_circular_buffer->max_size = max_size;
	new_circular_buffer->current_size = 0;
	new_circular_buffer->next_append = new_circular_buffer->data;

	return new_circular_buffer;
}

void
circular_buffer_free(CIRCULAR_BUFFER *circular_buffer)
{
	free(circular_buffer);
	return;
}

void circular_buffer_append(CIRCULAR_BUFFER *circular_buffer,
        LONG_LONG_PAIR new_entry)
{
	if (circular_buffer->current_size == circular_buffer->max_size)
	{
		*circular_buffer->next_append = new_entry;
		if (circular_buffer->next_append
		        == &circular_buffer->data[circular_buffer->max_size - 1])
		{
			circular_buffer->next_append = circular_buffer->data;
		}
		else
		{
			++circular_buffer->next_append;
		}
	}
	else
	{
		circular_buffer->data[circular_buffer->current_size] = new_entry;
		++circular_buffer->current_size;
	}
	return;
}

int
circular_buffer_compar(const void *key, const void *element)
{
	return (*(int64_t*) key - ((LONG_LONG_PAIR *) element)->key);
}

/*
 * circular_buffer_search() assumes that the buffer is sorted.
 *
 */
LONG_LONG_PAIR *
circular_buffer_search(CIRCULAR_BUFFER *circular_buffer, int64_t key)
{
	LONG_LONG_PAIR *found_ptr = NULL;

	if (circular_buffer->current_size == circular_buffer->max_size)
	{
		if (circular_buffer->next_append != circular_buffer->data)
		{
			size_t front_half_size = (circular_buffer->next_append
			        - circular_buffer->data);
			found_ptr = bsearch(&key, circular_buffer->data, front_half_size,
			        sizeof(LONG_LONG_PAIR), &circular_buffer_compar);
			if (found_ptr == NULL)
			{
				found_ptr = bsearch(&key, circular_buffer->next_append,
				        circular_buffer->max_size - front_half_size,
				        sizeof(LONG_LONG_PAIR), &circular_buffer_compar);
			}
		}
		else
		{
			found_ptr = bsearch(&key, circular_buffer->data,
			        circular_buffer->max_size, sizeof(LONG_LONG_PAIR),
			        &circular_buffer_compar);
		}
	}
	else
	{
		found_ptr = bsearch(&key, circular_buffer->data,
		        circular_buffer->current_size, sizeof(LONG_LONG_PAIR),
		        &circular_buffer_compar);
	}

	return found_ptr;
}

void
circular_buffer_foreach(CIRCULAR_BUFFER *circular_buffer,
        CIRCULAR_BUFFER_FOREACH_FUNC foreach_func)
{
	size_t i;
	size_t end;
	for (i = 0, end = circular_buffer->max_size; i < end; ++i)
	{
		foreach_func(i, circular_buffer->data[i]);
	}
	return;
}
