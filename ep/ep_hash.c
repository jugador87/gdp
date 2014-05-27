/* vim: set ai sw=8 sts=8 :*/

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

EP_SRC_ID("@(#)$Id: ep_hash.c 286 2014-04-29 18:15:22Z eric $");


/***********************************************************************
**
**  HASH HANDLING
*/

/**************************  BEGIN PRIVATE  **************************/

struct node
{
	int			keylen;		// length of key
	const void		*key;		// actual key
	void			*val;		// value
	struct node		*next;		// next in chain
};

struct EP_HASH
{
	EP_RPOOL		*rpool;		// resource pool
	uint32_t		flags;		// flags -- see below
	EP_MUTEX		mutex;		// lock on this object
	EP_HASH_HASH_FUNCP	hfunc;		// hashing function
	int			tabsize;	// size of hash tab
	struct node		*tab[0];	// actually longer
};

// no flags at this time


int
def_hfunc(
	const EP_HASH *const hp,
	const int keysize,
	const void *key)
{
	int hfunc = 0;
	int i;

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
	int xtra;

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

	hash = ep_rpool_zalloc(rp, sizeof *hash);
	hash->rpool = rp;
	hash->hfunc = hfunc;
	hash->flags = flags;
	hash->tabsize = tabsize;

	return hash;
}


void
ep_hash_free(EP_HASH *hash)
{
	EP_ASSERT_POINTER_VALID(hash);
	ep_rpool_free(hash->rpool);	// also frees hash
}


void *
ep_hash_search(
	const EP_HASH *hp,
	int keylen,
	const void *key)
{
	int indx;
	struct node *n;

	EP_ASSERT_POINTER_VALID(hp);

	indx = hp->hfunc(hp, keylen, key);

	for (n = hp->tab[indx]; n != NULL; n = n->next)
	{
		if (keylen == n->keylen &&
		    memcmp(key, n->key, keylen) == 0)
		{
			// match
			return n->val;
		}
	}
	return NULL;
}


void *
ep_hash_insert(
	EP_HASH *hp,
	int keylen,
	const void *key,
	void *val)
{
	int indx;
	struct node *n;
	void *kp;

	EP_ASSERT_POINTER_VALID(hp);

	indx = hp->hfunc(hp, keylen, key);

	for (n = hp->tab[indx]; n != NULL; n = n->next)
	{
		if (keylen == n->keylen &&
		    memcmp(key, n->key, keylen) == 0)
		{
			// match
			void *oldval;

			oldval = n->val;
			n->val = val;
			return oldval;
		}
	}

	// not found -- insert it
	n = ep_rpool_malloc(hp->rpool, sizeof *n);
	n->keylen = keylen;
	kp = ep_rpool_malloc(hp->rpool, keylen);
	memcpy(kp, key, keylen);
	n->key = kp;
	n->val = val;
	n->next = hp->tab[indx];
	hp->tab[indx] = n;
	return NULL;
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
}
