/* vim: set ai sw=8 sts=8 ts=8 :*/

#include <ep.h>
#include <ep_mem.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static struct ep_malloc_functions	DefaultMallocFunctions =
{
	&malloc,
	&realloc,
	&valloc,
	&free,
};

static struct ep_malloc_functions	*SystemMallocFunctions =
						&DefaultMallocFunctions;


/*
**  EP_SET_MALLOC_FUNCTIONS --- set the malloc functions
**
*/


void
ep_mem_set_malloc_functions(struct ep_malloc_functions *mf)
{
	SystemMallocFunctions = mf;
}



/*
**  SYSTEM_MALLOC -- access system memory allocator
**
**	Parameters:
**		nbytes -- the number of bytes to allocate
**		curmem -- a current pointer (for realloc)
**		aligned -- set if we want page-aligned memory
**
**	Returns:
**		A pointer to that memory; NULL if not available
*/

static void *
system_malloc(
	size_t nbytes,
	void *curmem,
	bool aligned)
{
	void *p;

	if (curmem != NULL)
		p = (SystemMallocFunctions->m_realloc)(curmem, nbytes);
#ifdef EP_OSCF_SYSTEM_VALLOC
	else if (aligned)
		p = (SystemMallocFunctions->m_valloc)(nbytes);
#endif
	else
		p = (SystemMallocFunctions->m_malloc)(nbytes);
	return p;
}


/*
**  SYSTEM_MFREE -- access system memory deallocator
**
**	Parameters:
**		p -- the memory to free
**
**	Returns:
**		none.
*/

static void
system_mfree(void *p)
{
	// we assume system deallocator does its own mutex
	(*SystemMallocFunctions->m_free)(p);
}


#if 0
/*
**  BUNNY_MALLOC -- it keeps on trying, and trying, and trying...
**
**	Parameters:
**		nbytes -- the number of bytes to allocate
**		curmem -- a pointer to current memory (for realloc)
**		flags -- behavior modifier bits
**
**	Returns:
**		A pointer to memory (cannot return NULL).
*/

static void *
bunny_malloc(
	size_t nbytes,
	void *curmem,
	uint32_t flags)
{
	EP_STAT stat = EP_STAT_OK;
	int triesleft = N_MALLOC_TRIES;

	while (triesleft-- > 0)
	{
		void *p;
		bool aligned = EP_UT_BITSET(EP_MEM_F_ALIGN, flags);
		char e1buf[64];

		// try allocating the memory (assume it does its own mutex)
		p = system_malloc(nbytes, curmem, aligned);

		// did it work?
		if (p != NULL)
		{
			// yep, return that memory
			return p;
		}

		// no, post an error
		// XXX errno shouldn't be visible at this level
		stat = ep_stat_post(EP_STAT_MEM_NOMEM,
				"ep_mem_malloc: out of memory: %1",
				ep_unix_errno_to_str(errno,
						e1buf, sizeof e1buf),
				NULL);

		// if we did some recovery, try again
		if (!EP_STAT_IS_SAME(stat, EP_STAT_MEM_TRYAGAIN))
			break;
	}

	// we are out of luck after a couple of tries
	if (!EP_UT_BITSET(EP_MEM_F_FAILOK, flags))
		ep_assert_abort("ep_mem_malloc: Out of memory: aborting");

	return NULL;
}

#endif


/*
**  EP_MEM_IALLOC -- allocate memory from the heap (internal)
**
**	This is the routine that does all the real work.
**
**	This always succeeds, unless the EP_MEM_F_FAILOK flag bit
**	is set.  If memory cannot be allocated, it posts an error
**	condition.  The condition handler has four choices:
**	it can do some memory cleanup and return
**	EP_STAT_MEM_TRYAGAIN to retry the allocation.  It can
**	clean up and abort the thread (i.e., not return) -- for
**	example, by dropping the current connection and releasing
**	any memory associated with it, presumably also terminating
**	the associated task.  Or it can return another status to
**	cause us to abort the process.
**
**	Parameters:
**		nbytes -- how much memory to allocate
**		curmem -- a current memory pointer (for realloc)
**		flags -- flags to modify operation
**		file -- file name of caller (for debugging)
**		line -- line number of caller (for debugging)
**
**	Returns:
**		the memory
*/

void *
ep_mem_ialloc(
	size_t nbytes,
	void *curmem,
	uint32_t flags,
	const char *file,
	int line)
{
	void *p;

	// always require at least one pointer worth of data
	if (nbytes < sizeof (void *))
		nbytes = sizeof (void *);

	// see if this request wants page alignment
//	if ((nbytes & (EP_OSCF_MEM_PAGESIZE - 1)) == 0)
//		flags |= EP_MEM_F_ALIGN;

	// get the memory itself
	p = system_malloc(nbytes, curmem, EP_UT_BITSET(EP_MEM_F_ALIGN, flags));

	if (p == NULL)
	{
		char e1buf[80];

		if (EP_UT_BITSET(EP_MEM_F_FAILOK, flags))
			goto done;

		strerror_r(errno, e1buf, sizeof e1buf);
		fprintf(stderr, "Out of memory: %s\n", e1buf);
		ep_assert_abort("Out of Memory");
		/*NOTREACHED*/
	}

	// zero or trash memory if requested
	if ( EP_UT_BITSET(EP_MEM_F_ZERO, flags))
		memset(p, 0, nbytes);
	else if (EP_UT_BITSET(EP_MEM_F_TRASH, flags))
		ep_mem_trash(p, nbytes);

	// return pointer to allocated memory
done:
	return p;
}




/*
**  EP_MEM_[MZR]ALLOC_F -- malloc to plug into function pointer
**
**	Used for other libraries that require a pointer to a routine
**	taking one size argument.  Essentially this is plain old malloc.
*/

void *
ep_mem_malloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, 0, NULL, 0);
}

void *
ep_mem_zalloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, EP_MEM_F_ZERO, NULL, 0);
}

void *
ep_mem_ralloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, EP_MEM_F_TRASH, NULL, 0);
}

void *
ep_mem_realloc_f(size_t nbytes,
	void *curmem)
{
	return ep_mem_ialloc(nbytes, curmem, 0, NULL, 0);
}


/*
**  EP_MEM_[MZR]ALLOC -- same thing, but just in the non-debugging case
*/

#undef ep_mem_malloc

void *
ep_mem_malloc(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, 0, NULL, 0);
}

#undef ep_mem_zalloc

void *
ep_mem_zalloc(size_t nbytes)
{
	void *p;

	p = ep_mem_ialloc(nbytes, NULL, EP_MEM_F_ZERO, NULL, 0);
	return p;
}

#undef ep_mem_ralloc

void *
ep_mem_ralloc(size_t nbytes)
{
	void *p;

	p = ep_mem_ialloc(nbytes, NULL, EP_MEM_F_TRASH, NULL, 0);
	return p;
}

#undef ep_mem_realloc

void *
ep_mem_realloc(
	void *curmem,
	size_t nbytes)
{
	return ep_mem_ialloc(nbytes, curmem, 0, NULL, 0);
}


/*
**  EP_MEM_TSTRDUP -- save a string onto a heap (tagged)
**
**	paramters:
**		s -- the string to duplicate
**		slen -- the maximum length of s to copy -- -1 => all
**		flags -- as usual
**		grp -- an arbitrary group number (for debugging)
**		file -- the file name (for debugging)
**		line -- the line number (for debugging)
*/

char *
ep_mem_istrdup(
	const char *s,
	ssize_t slen,
	uint32_t flags,
	const char *file,
	int line)
{
	ssize_t l;
	char *p;

	if (s == NULL)
		return NULL;
	l = strlen(s);
	if (slen >= 0 && l > slen)
		l = slen;
	EP_ASSERT_REQUIRE(l + 1 > l);
	p = ep_mem_ialloc(l + 1, NULL, flags, file, line);
	if (p != NULL)
	{
		memcpy(p, s, l);
		p[l] = '\0';
	}
	return p;
}

/*
**  EP_MEM_STRDUP -- only used if memory debugging is not compiled in
*/

#undef ep_mem_strdup

char *
ep_mem_strdup(const char *s)
{
	return ep_mem_istrdup(s, -1, 0, NULL, 0);
}

/*
**  EP_MEM_TRASH -- set memory to random values
**
**	No gurantees here: the memory might be set to pseudo-random
**	bytes, it might be set to 0xDEADBEEF, or might be untouched.
**	If you require specific semantics, take care of it yourself.
**
**	Parameters:
**		p -- pointer to the memory
**		nbytes -- size of memory
**
**	Returns:
**		nothing
*/

void
ep_mem_trash(void *p,
	size_t nbytes)
{
	// for now, do nothing
}



/*
**  EP_MEM_FREE -- free allocated memory
**
**	Parameters:
**		p -- the memory to free
**
**	Returns:
**		void
*/

void
ep_mem_free(void *p)
{
	system_mfree(p);
}
