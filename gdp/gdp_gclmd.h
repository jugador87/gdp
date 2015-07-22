/* vim: set ai sw=4 sts=4 ts=4 :*/


/*
**  GDP_GCLMD.H --- headers for GCL metadata
**
**		THESE DEFINITIONS ARE PRIVATE!
**
**		Although the md_len field is (at least) 32 bits, only the
**		bottom 24 bits are used for the length.  The top eight bits
**		are reserved for future use (probably for tagging or typing).
**		Those bits must be zero on send and ignored on receive.
**
**		This data structure contains the data in two representations.
**		One is the on-wire format in network byte order.  The other
**		is an internal array of name/len/value triplets.  The value
**		part of the triplet is actually a pointer into the on-wire
**		format.
*/


struct metadatum
{
	gdp_gclmd_id_t		md_id;			// identifier (uint32_t)
	size_t				md_len;			// data length (only 24 bits!)
	void				*md_data;		// pointer to data
	uint32_t			md_flags;		// see below
};

#define MDF_OWNDATA		0x00000001		// we own the data field


struct gdp_gclmd
{
	int					nalloc;			// number of datums allocated
	int					nused;			// number of datums in use
	struct metadatum	*mds;			// array of metadata items
	void				*databuf;		// data buffer space (optional)
	uint32_t			flags;			// see below
};

#define GCLMDF_READONLY	0x00000001		// metadata list cannot be modified


//XXX this doesn't belong here
#define GDP_GCLMD_EOLIST	0			// id: end of metadata list

// serialize an internal data structure to a network buffer
size_t			_gdp_gclmd_serialize(
					gdp_gclmd_t *gmd,
					struct evbuffer *evb);

// deserialize a network buffer to an internal data structure
gdp_gclmd_t		*_gdp_gclmd_deserialize(
					struct evbuffer *evb);
