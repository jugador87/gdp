/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp.h>
#include <gdp_priv.h>
#include <gdp_gclmd.h>

#include <ep/ep_dbg.h>

#include <string.h>

/*
**  GDP GCL Metadata
*/


#define INITIALMDS		4		// number of initial metadata entries (must be > 3)

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.metadata", "GCL metadata processing");


/*
**  GDP_GCLMD_NEW --- allocate space for in-client metadata
*/

gdp_gclmd_t *
gdp_gclmd_new(void)
{
	gdp_gclmd_t *gmd;
	size_t len = sizeof *gmd;

	gmd = ep_mem_zalloc(len);
	gmd->nalloc = INITIALMDS;
	gmd->nused = 0;
	gmd->mds = ep_mem_zalloc(gmd->nalloc * sizeof *gmd->mds);
	ep_dbg_cprintf(Dbg, 21, "gdp_gclmd_new() => %p\n", gmd);
	return gmd;
}


/*
**  GDP_GCLMD_FREE --- deallocate space for in-client metadata
*/

void
gdp_gclmd_free(gdp_gclmd_t *gmd)
{
	int i;

	ep_dbg_cprintf(Dbg, 21, "gdp_gclmd_free(%p)\n", gmd);
	if (gmd->databuf != NULL)
		ep_mem_free(gmd->databuf);
	for (i = 0; i < gmd->nused; i++)
	{
		if (EP_UT_BITSET(MDF_OWNDATA, gmd->mds[i].md_flags))
			ep_mem_free(gmd->mds[i].md_data);
	}
	ep_mem_free(gmd->mds);
	ep_mem_free(gmd);
}


/*
**  GDP_GCLMD_ADD --- add a single metadatum to a metadata list
*/

EP_STAT
gdp_gclmd_add(gdp_gclmd_t *gmd,
			gdp_gclmd_id_t id,
			size_t len,
			const void *data)
{
	EP_ASSERT_POINTER_VALID(gmd);

	if (EP_UT_BITSET(GCLMDF_READONLY, gmd->flags))
		return GDP_STAT_READONLY;

	// end of list is implicit; just ignore explicit ones
	//XXX: return OK or INFO status code?
	if (id == GDP_GCLMD_EOLIST)
		return EP_STAT_OK;

	// see if we have room for another entry
	if (gmd->nused >= gmd->nalloc)
	{
		// no room; get some more (allocate 50% more than we have)
		gmd->nalloc = (gmd->nalloc / 2) * 3;
		gmd->mds = ep_mem_realloc(gmd->mds, gmd->nalloc * sizeof *gmd->mds);
	}

	gmd->mds[gmd->nused].md_id = id;
	gmd->mds[gmd->nused].md_len = len;
	gmd->mds[gmd->nused].md_data = ep_mem_malloc(len);
	gmd->mds[gmd->nused].md_flags = MDF_OWNDATA;
	memcpy(gmd->mds[gmd->nused].md_data, data, len);

	return EP_STAT_OK;
}


/*
**  GDP_GCLMD_GET --- get metadata from an existing GCL
*/

EP_STAT
gdp_gclmd_get(gdp_gclmd_t *gmd,
			int indx,
			gdp_gclmd_id_t *id,
			size_t *len,
			const void **data)
{
	EP_STAT estat = EP_STAT_OK;

	// see if this id is allocated
	if (indx >= gmd->nused)
		return GDP_STAT_NOTFOUND;

	// copy out any requested fields
	if (id != NULL)
		*id = gmd->mds[indx].md_id;
	if (len != NULL)
		*len = gmd->mds[indx].md_len;
	if (data != NULL)
		*data = gmd->mds[indx].md_data;

	return estat;
}


/*
**  _GDP_GCLMD_SERIALIZE --- serialize metadata into network order
*/

void
_gdp_gclmd_serialize(gdp_gclmd_t *gmd, struct evbuffer *evb)
{
	int i;

	// write the number of entries
	{
		uint16_t t16 = htons(gmd->nused);
		evbuffer_add(evb, &t16, sizeof t16);
	}

	// for each metadata item, write the header
	for (i = 0; i < gmd->nused; i++)
	{
		uint32_t t32;

		t32 = htonl(gmd->mds[i].md_id);
		evbuffer_add(evb, &t32, sizeof t32);
		t32 = htonl(gmd->mds[i].md_len);
		evbuffer_add(evb, &t32, sizeof t32);
	}

	// now write out all the data
	for (i = 0; i < gmd->nused; i++)
	{
		if (gmd->mds[i].md_len > 0)
			evbuffer_add(evb, gmd->mds[i].md_data, gmd->mds[i].md_len);
	}
}


/*
**  _GDP_GCLMD_DESERIALIZE --- crack open an evbuffer and store the metadata
**
**		The data is serialized as a count of entries, that number
**		of metadata headers (contains the id and data length),
**		and finally the metadata itself.  This order is so that it's
**		easy to read it in without having to reallocate space.
*/

gdp_gclmd_t *
_gdp_gclmd_deserialize(struct evbuffer *evb)
{
	int nmd = 0;		// number of metadata entries
	int tlen = 0;		// total data length
	gdp_gclmd_t *gmd;
	int i;

	// get the number of metadata entries
	{
		uint16_t t16;

		if (evbuffer_get_length(evb) < sizeof t16)
			return NULL;
		evbuffer_remove(evb, &t16, sizeof t16);
		nmd = ntohs(t16);
		if (nmd == 0)
			return NULL;
	}

	// make sure we have at least all the headers
	if (evbuffer_get_length(evb) < (2 * sizeof (uint32_t)))
		return NULL;

	// allocate and populate the header
	gmd = ep_mem_zalloc(sizeof *gmd);
	gmd->flags = GCLMDF_READONLY;
	gmd->nalloc = gmd->nused = nmd;

	// allocate and read in the metadata headers
	gmd->mds = ep_mem_malloc(nmd * sizeof *gmd->mds);
	for (i = 0; i < nmd; i++)
	{
		uint32_t t32;

		evbuffer_remove(evb, &t32, sizeof t32);
		gmd->mds[i].md_id = ntohl(t32);
		evbuffer_remove(evb, &t32, sizeof t32);
		tlen += gmd->mds[i].md_len = ntohl(t32);
	}

	// and now for the data....
	gmd->databuf = ep_mem_malloc(tlen);
	if (evbuffer_remove(evb, gmd->databuf, tlen) != tlen)
	{
		// bad news
		ep_mem_free(gmd->mds);
		ep_mem_free(gmd);
		return NULL;
	}

	// we can now insert the pointers into the data
	void *dbuf = &gmd->databuf;
	for (i = 0; i < nmd; i++)
	{
		gmd->mds[i].md_data = dbuf;
		dbuf += gmd->mds[i].md_len;
	}

	return gmd;
}
