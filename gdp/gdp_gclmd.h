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
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
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


struct metadatum
{
	gdp_gclmd_id_t		md_id;			// identifier (uint32_t)
	uint32_t			md_len;			// data length (only 24 bits!)
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

// bulk load data into a metadata structure using existing names & lengths
void			_gdp_gclmd_adddata(
					gdp_gclmd_t *gmd,
					void *data);
