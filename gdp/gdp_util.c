/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP Utility routines
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

#include "gdp.h"
#include "gdp_gclmd.h"

#include <event2/event.h>

#include <ep/ep_crypto.h>
#include <ep/ep_dbg.h>

//static EP_DBG	Dbg = EP_DBG_INIT("gdp.util", "GDP utility routines");


/*
**   _GDP_NEWNAME --- create a new GDP name
**
**		Really just creates a random number (for now).
*/

void
_gdp_newname(gdp_name_t gname, gdp_gclmd_t *gmd)
{
	if (gmd == NULL)
	{
		// last resort: use random bytes
		evutil_secure_rng_get_bytes(gname, sizeof (gdp_name_t));
	}
	else
	{
		gdp_buf_t *gbuf = gdp_buf_new();
		size_t glen = _gdp_gclmd_serialize(gmd, gbuf);

		ep_crypto_md_sha256(gdp_buf_getptr(gbuf, glen), glen, gname);
		gdp_buf_free(gbuf);
	}
}
