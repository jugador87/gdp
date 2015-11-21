/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  Message management
**
**		Messages contain the header and data information for a single
**		message.
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

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.datum", "GDP datum processing");

static gdp_datum_t		*DatumFreeList;
static EP_THR_MUTEX		DatumFreeListMutex EP_THR_MUTEX_INITIALIZER;

gdp_datum_t *
gdp_datum_new(void)
{
	gdp_datum_t *datum;

	// get a message off the free list, if any
	ep_thr_mutex_lock(&DatumFreeListMutex);
	if ((datum = DatumFreeList) != NULL)
	{
		DatumFreeList = datum->next;
	}
	ep_thr_mutex_unlock(&DatumFreeListMutex);

	if (datum == NULL)
	{
		// nothing on the free list; allocate anew
		datum = ep_mem_zalloc(sizeof *datum);
		ep_thr_mutex_init(&datum->mutex, EP_THR_MUTEX_DEFAULT);
	}
	datum->next = NULL;

	EP_ASSERT(!datum->inuse);

	// initialize metadata
	datum->recno = GDP_PDU_NO_RECNO;
	EP_TIME_INVALIDATE(&datum->ts);
	if (datum->dbuf == NULL)
	{
		datum->dbuf = gdp_buf_new();
	}
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_new => %p\n", datum);
	datum->inuse = true;
	return datum;
}


void
gdp_datum_free(gdp_datum_t *datum)
{
	ep_dbg_cprintf(Dbg, 48, "gdp_datum_free(%p)\n", datum);
	EP_ASSERT(datum->inuse);
	datum->inuse = false;
	if (datum->dbuf != NULL)
	{
		size_t ndrain = evbuffer_get_length(datum->dbuf);
		ep_dbg_cprintf(Dbg, 50, "  ... draining %zd bytes\n", ndrain);
		if (ndrain > 0)
			evbuffer_drain(datum->dbuf, ndrain);
	}
	ep_thr_mutex_lock(&DatumFreeListMutex);
	datum->next = DatumFreeList;
	DatumFreeList = datum;
	ep_thr_mutex_unlock(&DatumFreeListMutex);
}


gdp_recno_t
gdp_datum_getrecno(const gdp_datum_t *datum)
{
	return datum->recno;
}

void
gdp_datum_getts(const gdp_datum_t *datum, EP_TIME_SPEC *ts)
{
	memcpy(ts, &datum->ts, sizeof *ts);
}

size_t
gdp_datum_getdlen(const gdp_datum_t *datum)
{
	return gdp_buf_getlength(datum->dbuf);
}

gdp_buf_t *
gdp_datum_getbuf(const gdp_datum_t *datum)
{
	return datum->dbuf;
}

gdp_buf_t *
gdp_datum_getsig(const gdp_datum_t *datum)
{
	return datum->sig;
}

short
gdp_datum_getsigmdalg(const gdp_datum_t *datum)
{
	return datum->sigmdalg;
}

/*
**	GDP_DATUM_PRINT --- print a datum (for debugging)
*/

void
gdp_datum_print(const gdp_datum_t *datum, FILE *fp, uint32_t flags)
{
	unsigned char *d;
	int l;

	flockfile(fp);
	if (EP_UT_BITSET(GDP_DATUM_PRDEBUG, flags))
		fprintf(fp, "datum @ %p: ", datum);
	if (datum == NULL)
	{
		fprintf(fp, "null datum\n");
		goto done;
	}

	fprintf(fp, "recno %" PRIgdp_recno ", ", datum->recno);

	if (datum->dbuf == NULL)
	{
		fprintf(fp, "no data");
		d = NULL;
		l = -1;
	}
	else
	{
		l = gdp_buf_getlength(datum->dbuf);
		fprintf(fp, "len %d", l);
		d = gdp_buf_getptr(datum->dbuf, l);
	}

	if (EP_TIME_ISVALID(&datum->ts))
	{
		fprintf(fp, ", ts ");
		ep_time_print(&datum->ts, fp, EP_TIME_FMT_HUMAN);
	}
	else
	{
		fprintf(fp, ", no timestamp");
	}

	fprintf(fp, "%s\n", datum->inuse ? "" : ", !inuse");

	if (l > 0)
	{
		if (EP_UT_BITSET(GDP_DATUM_PRTEXT, flags))
			fprintf(fp, "%.*s\n", l, d);
		else
			ep_hexdump(d, l, fp, EP_HEXDUMP_ASCII, 0);
	}

	if (EP_UT_BITSET(GDP_DATUM_PRSIG, flags))
	{
		fprintf(fp, "  Sig len = %d, digest alg = %d\n",
				datum->siglen, datum->sigmdalg);
		if (datum->sig != NULL && EP_UT_BITSET(GDP_DATUM_PRDEBUG, flags))
		{
			d = gdp_buf_getptr(datum->sig, datum->siglen);
			ep_hexdump(d, datum->siglen, fp, EP_HEXDUMP_HEX, 0);
		}
	}
done:
	funlockfile(fp);
}


void
_gdp_datum_dump(const gdp_datum_t *datum,
			FILE *fp)
{
	gdp_datum_print(datum, fp, GDP_DATUM_PRDEBUG);
}
