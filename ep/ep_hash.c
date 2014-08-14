/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**	Copyright (c) 2008-2010, Eric P. Allman.  All rights reserved.
**	$Id: ep_hash.c 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_rpool.h>
#include <ep_stat.h>
#include <ep_assert.h>
#include <ep_hash.h>
#include <ep_string.h>
#include <ep_thr.h>

EP_SRC_ID("@(#)$Id: ep_hash.c 286 2014-04-29 18:15:22Z eric $");


/***********************************************************************
**
**  HASH HANDLING
*/

/**************************  BEGIN PRIVATE  **************************/

struct node
{
	size_t			keylen;		// length of key
	void			*key;		// actual key
	void			*val;		// value
	struct node		*next;		// next in chain
};

struct EP_HASH
{
	EP_RPOOL		*rpool;		// resource pool
	uint32_t		flags;		// flags -- see below
	EP_THR_MUTEX		mutex;		// lock on this object
	EP_HASH_HASH_FUNCP	hfunc;		// hashing function
	int			tabsize;	// size of hash tab
	struct node		*tab[0];	// actually longer
};

// no flags at this time


int
def_hfunc(
	const EP_HASH *const hp,
	const size_t keysize,
	const void *key)
{
	int hfunc = 0;
	size_t i;

	for (i = 0; i < keysize; i++)
	{
		hfunc = ((hfunc << 1) ^ (*(uint8_t*)key++ & 0xff)) % hp->tabsize;
	}
	return hfunc;
}

/***************************  END PRIVATE  ***************************/

EP_HASH *
ep_hash_new(
	const char *name,
	EP_HASH_HASH_FUNCP hfunc,
	int tabsize)
{
	uint32_t flags = 0;
	EP_HASH *hash;
	EP_RPOOL *rp;
	size_t xtra;

	if (tabsize == 0)
		tabsize = ep_adm_getintparam("libep.hash.tabsize", 2003);
	if (name == NULL)
		name = "<hash>";
	if (hfunc == NULL)
		hfunc = def_hfunc;

	EP_ASSERT(tabsize >= 2);

	rp = ep_rpool_new(name, (size_t) 0);

	// figure how much extra space we need to allocate for table
	xtra = tabsize * sizeof hash->tab[0];

	hash = ep_rpool_zalloc(rp, sizeof *hash + xtra);
	hash->rpool = rp;
	hash->hfunc = hfunc;
	hash->flags = flags;
	hash->tabsize = tabsize;
	ep_thr_mutex_init(&hash->mutex);

	return hash;
}


void
ep_hash_free(EP_HASH *hash)
{
	EP_ASSERT_POINTER_VALID(hash);
	ep_thr_mutex_destroy(&hash->mutex);
	ep_rpool_free(hash->rpool);	// also frees hash
}


struct node **
find_node_ptr(EP_HASH *hp,
	size_t keylen,
	const void *key)
{
	int indx;
	struct node **npp;
	struct node *n;

	indx = hp->hfunc(hp, keylen, key);
	npp = &hp->tab[indx];

	for (n = *npp; n != NULL; npp = &n->next, n = *npp)
	{
		if (keylen == n->keylen &&
		    memcmp(key, n->key, keylen) == 0)
		{
			// match
			break;
		}
	}

	return npp;
}


void *
ep_hash_search(
	EP_HASH *hp,
	size_t keylen,
	const void *key)
{
	struct node **npp;
	void *val;

	EP_ASSERT_POINTER_VALID(hp);

	ep_thr_mutex_lock(&hp->mutex);
	npp = find_node_ptr(hp, keylen, key);
	if (*npp == NULL)
		val = NULL;
	else
		val = (*npp)->val;
	ep_thr_mutex_unlock(&hp->mutex);
	return val;
}


void *
ep_hash_insert(
	EP_HASH *hp,
	size_t keylen,
	const void *key,
	void *val)
{
	struct node **npp;
	struct node *n;
	void *kp;

	EP_ASSERT_POINTER_VALID(hp);

	ep_thr_mutex_lock(&hp->mutex);
	npp = find_node_ptr(hp, keylen, key);
	if (*npp != NULL)
	{
		// there is an existing value; replace it
		void *oldval;

		n = *npp;
		oldval = n->val;
		n->val = val;
		ep_thr_mutex_unlock(&hp->mutex);
		return oldval;
	}

	// not found -- insert it
	n = ep_rpool_malloc(hp->rpool, sizeof *n);
	n->keylen = keylen;
	kp = ep_rpool_malloc(hp->rpool, keylen);
	memcpy(kp, key, keylen);
	n->key = kp;
	n->val = val;
	n->next = NULL;
	*npp = n;
	ep_thr_mutex_unlock(&hp->mutex);
	return NULL;
}


void *
ep_hash_delete(
	EP_HASH *hp,
	size_t keylen,
	const void *key)
{
	struct node **npp;
	struct node *n;
	void *v;

	EP_ASSERT_POINTER_VALID(hp);

	ep_thr_mutex_lock(&hp->mutex);
	npp = find_node_ptr(hp, keylen, key);
	if (*npp == NULL)
	{
		// entry does not exist
		ep_thr_mutex_unlock(&hp->mutex);
		return NULL;
	}

	n = *npp;
	*npp = n->next;
	v = n->val;
	ep_rpool_mfree(hp->rpool, n->key);
	ep_rpool_mfree(hp->rpool, n);
	ep_thr_mutex_unlock(&hp->mutex);
	return v;
}


/*
**  EP_HASH_FORALL -- apply a function to all nodes
*/

void
ep_hash_forall(
	EP_HASH *hp,
	EP_HASH_FORALL_FUNCP func,
	...)
{
	va_list av;
	struct node *n;
	int i;

	va_start(av, func);

	EP_ASSERT_POINTER_VALID(hp);

	ep_thr_mutex_lock(&hp->mutex);
	for (i = 0; i < hp->tabsize; i++)
	{
		for (n = hp->tab[i]; n != NULL; n = n->next)
		{
			va_list lav;

			va_copy(lav, av);
			(*func)(n->keylen, n->key, n->val, lav);
		}
	}

	va_end(av);
	ep_thr_mutex_unlock(&hp->mutex);
}
