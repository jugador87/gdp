/* vim: set ai sw=4 sts=4 : */

#include "gdpd.h"
#include "gdpd_gcl.h"

#include <ep/ep_mem.h>

#include <gdp/gdp_protocol.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h>

/*
**  This is currently a crappy implementation.  It's slow and not very
**  flexible and doesn't even start to consider distributed access.
**  XXX Multiple writers don't work properly if they are in different
**	instances of gdpd: since the message number is
**	local to a GCL Handle, two writers will allocate the same msgno.
**  XXX Doesn't work with threaded processes: no in-process locking.
**
**  GCLes are represented as files on disk in the following format:
**
**	#!GDP-GCL <ver>\n
**	<metadata>\n
**	0*<message>
**
**  Messages are represented as:
**
**	\n#!MSG <msgno> <timestamp> <length>\n
**	<length bytes of binary data>
**
**  <msgno> starts at 1. XXX Why not zero???
**
**  Note that there is a blank line between the metadata and the
**  first message.
*/

/************************  PRIVATE  ************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd.physlog",
				"GDP Daemon Physical Log Implementation");

#define GCL_MAX_LINE		128	// max length of text line in gcl
#define GCL_PATH_LEN		200	// max length of pathname
#define GCL_DIR			"/var/tmp/gcl"
#define GCL_VERSION		0
#define GCL_MAGIC		"#!GDP-GCL "
#define MSG_MAGIC		"\n#!MSG "

static const char	*GCLDir;	// the gcl data directory


/*
**  A handle on an open gcl.
**
**	We cache the offsets of messages we have seen.  It's not clear
**	how useful this is, but without it moving backward through a
**	log is excrutiatingly slow.  The array is dynamically allocated,
**	so cachesize is the number of entries allocated and maxmsgno is
**	the last one actually known.
*/

#define INITIAL_MSG_OFFCACHE	256	// initial size of offset cache

#define GCL_NEXT_MSG	    (-1)	// sentinel for next available message


/*
**  GCL_NEW --- create a new GCL Handle & initialize
**
**  Returns:
**	New GCL Handle if possible
**	NULL otherwise
*/

EP_STAT
gcl_offset_cache_init(gcl_handle_t *gclh)
{
    EP_STAT estat = EP_STAT_OK;

    // allocate an initial set of offset pointers
    gclh->cachesize = ep_adm_getintparam("swarm.gdp.cachesize",
					    INITIAL_MSG_OFFCACHE);
    if (gclh->cachesize < INITIAL_MSG_OFFCACHE)
	gclh->cachesize = INITIAL_MSG_OFFCACHE;
    gclh->offcache = ep_mem_zalloc(gclh->cachesize * sizeof gclh->offcache[0]);
    if (gclh->offcache == NULL)
	goto fail1;

    // success
    return estat;

fail1:
    estat = ep_stat_from_errno(errno);
    return estat;
}



/*
**  GET_GCL_PATH --- get the pathname to an on-disk version of the gcl
*/

static EP_STAT
get_gcl_path(gcl_handle_t *gclh, char *pbuf, int pbufsiz)
{
    gcl_pname_t pname;
    int i;

    EP_ASSERT_POINTER_VALID(gclh);

    if (GCLDir == NULL)
	GCLDir = ep_adm_getstrparam("swarm.gdp.gcl.dir", GCL_DIR);
    gdp_gcl_printable_name(gclh->gcl_name, pname);
    i = snprintf(pbuf, pbufsiz, "%s/%s", GCLDir, pname);
    if (i < pbufsiz)
	return EP_STAT_OK;
    else
	return EP_STAT_ERROR;
}

/*
**  GCL_SAVE_OFFSET --- save the offset of a message number
**
**	This fills in an ongoing buffer in memory that has the offsets
**	of the message numbers we have seen.  The buffer is expanded
**	if necessary.
*/

static EP_STAT
gcl_handle_save_offset(gcl_handle_t *gclh,
	    gdp_msgno_t msgno,
	    off_t off)
{
    ep_dbg_cprintf(Dbg, 8,
	    "gcl_handle_save_offset: caching msgno %d offset %lld\n",
	    msgno, off);
    if (msgno >= gclh->cachesize)
    {
	// have to allocate more space
	while (msgno >= gclh->cachesize)	// hack, but failure rare
	    gclh->cachesize += gclh->cachesize / 2;
	gclh->offcache = ep_mem_realloc(gclh->offcache,
			    gclh->cachesize * sizeof gclh->offcache[0]);
	if (gclh->offcache == NULL)
	{
	    // oops, no memory
	    ep_dbg_cprintf(Dbg, 7, "gcl_handle_save_offset: no memory\n");
	    return ep_stat_from_errno(errno);
	}
    }

    if (off != gclh->offcache[msgno] && gclh->offcache[msgno] != 0)
    {
	// somehow offset has moved.  not a happy camper.
	gdp_log(EP_STAT_ERROR,
		"gcl_handle_save_offset: offset for message %d has moved from %lld to %lld\n",
		msgno,
		(long long) gclh->offcache[msgno],
		(long long) off);
	return EP_STAT_ERROR;	    // TODO: should be more specific
    }

    gclh->offcache[msgno] = off;
    if (msgno > gclh->maxmsgno)
	gclh->maxmsgno = msgno;
    return EP_STAT_OK;
}

/*
**  GET_GCL_REC --- get the next gcl record
**
**	This implements a sliding window on the input to search for
**	the message magic string.  When the magic is found it
**	unmarshalls the message.
**
**	Parameters:
**	    fp --- the disk file from which to read
**	    msg --- a message header that will be filled in
**	    evb --- a buffer to receive the data; if NULL the data
**		is discarded
*/

static EP_STAT
get_gcl_rec(FILE *fp,
	    gdp_msg_t *msg,
	    struct evbuffer *evb)
{
    long offset;
    EP_STAT estat = EP_STAT_OK;

    /*
    **  Part 1: find the message magic string
    */

    {
	int tx;			// index into target (pattern)
	int bx;			// index into magicbuf
	int bstart;		// buffer offset for start of test string
	int bend;		// buffer offset for end of test string
	int c = 0;		// current test character
	char magicbuf[16];	// must be larger than target pattern

	// scan the stream for the message marker
	//	tx counts through the target pattern, bx through the buffer
	tx = bx = bstart = bend = 0;
	offset = ftell(fp);
	ep_dbg_cprintf(Dbg, 50,
		"get_gcl_rec: initial offset %ld\n",
		offset);
	while (tx < sizeof MSG_MAGIC - 1)
	{
	    // if we don't have any input buffered, get some more
	    if (bx == bend)
	    {
		c = fgetc(fp);
		if (c == EOF)
			break;
		magicbuf[bend++] = c;
		bend %= sizeof magicbuf;
	    }
	    if (c == MSG_MAGIC[tx++])
	    {
		// this character matches; try the next one
		bx = (bx + 1) % sizeof magicbuf;
		continue;
	    }

	    // they don't match; back up and start over
	    bx = bstart = (bstart + 1) % sizeof magicbuf;
	    tx = 0;
	    offset++;
	}

	// we've either matched or are at end of file
	if (tx < sizeof MSG_MAGIC - 1)
	{
	    // at end of file or I/O error
	    if (ferror(fp))
		estat = ep_stat_from_errno(errno);
	    else
		estat = EP_STAT_END_OF_FILE;
	    ep_dbg_cprintf(Dbg, 4,
		    "get_gcl_rec: no msg magic (error=%d, offset=%ld)\n",
		    ferror(fp), ftell(fp));
	    goto fail0;
	}
    }

    /*
    **  Part 2: read in the message metadata and data
    */

    {
	int i;
	long msgno;
	long msglen;
	size_t sz;
	char tsbuf[80];

	// read in the rest of the record header
	i = fscanf(fp, "%ld %79s %ld\n", &msgno, tsbuf, &msglen);
	if (i < 3)
	{
	    // apparently we can't read the metadata
	    ep_dbg_cprintf(Dbg, 4, "get_gcl_rec: missing msg metadata (%d)\n",
		    i);
	    estat = GDP_STAT_MSGFMT;
	    goto fail0;
	}
	memset(msg, 0, sizeof *msg);
	msg->msgno = msgno;
	msg->len = msglen;
	msg->offset = offset;
	if (tsbuf[0] != '-')
	{
	    estat = tt_parse_interval(tsbuf, &msg->ts);
	    if (!EP_STAT_ISOK(estat))
	    {
		ep_dbg_cprintf(Dbg, 4, "get_gcl_rec: cannot parse timestamp\n");
		goto fail0;
	    }
	}

	// be sure to not overflow buffer
	while (msglen > 0)
	{
	    char dbuf[1024];
	    size_t l = msglen;

	    if (l > sizeof dbuf)
		l = sizeof dbuf;
	    sz = fread(dbuf, 1, l, fp);
	    if (sz < l)
	    {
		// short read
		ep_dbg_cprintf(Dbg, 4,
			"get_gcl_rec: short read (wanted %zu, got %zu)\n",
			l, sz);
		estat = GDP_STAT_SHORTMSG;
		goto fail0;
	    }
	    if (evb != NULL)
		evbuffer_add(evb, dbuf, l);
	    msglen -= l;
	}
    }

fail0:
    if (ep_dbg_test(Dbg, 4))
    {
	char ebuf[100];

	ep_dbg_printf("get_gcl_rec => %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}


/*
**  GCL_READ --- read a message from a gcl
**
**	In theory we should be positioned at the head of the next message.
**	But that might not be the correct message.  If we have specified a
**	message number there are two cases:
**	    (a) We've already seen the message -> use the cached offset.
**		Of course we check to see if we got the record we expected.
**	    (b) We haven't (yet) seen the message in this GCL Handle -> starting
**		from the last message we have seen, read up to this message,
**		assuming it exists.
*/

EP_STAT
gcl_read(gcl_handle_t *gclh,
	    gdp_msgno_t msgno,
	    gdp_msg_t *msg,
	    struct evbuffer *evb)
{
    EP_STAT estat = EP_STAT_OK;

    EP_ASSERT_POINTER_VALID(gclh);

    // don't want to read a partial record
    flock(fileno(gclh->fp), LOCK_SH);

    // if we already have a seek offset, use it
    if (msgno < gclh->maxmsgno && gclh->offcache[msgno] > 0)
    {
	ep_dbg_cprintf(Dbg, 18,
		"gcl_read: using cached offset %lld for msgno %d\n",
		gclh->offcache[msgno], msgno);
	if (fseek(gclh->fp, gclh->offcache[msgno], SEEK_SET) < 0)
	{
	    estat = ep_stat_from_errno(errno);
	    goto fail0;
	}
    }

    // we may have to skip ahead, hence the do loop
    ep_dbg_cprintf(Dbg, 7, "gcl_read: looking for msgno %d\n", msgno);
    do
    {
	if (EP_UT_BITSET(GCLH_ASYNC, gclh->flags))
	{
	    fd_set inset;
	    struct timeval zerotime = { 0, 0 };
	    int i;

	    // make sure we have something to read
	    //XXX should be done with libevent
	    FD_ZERO(&inset);
	    FD_SET(fileno(gclh->fp), &inset);
	    i = select(fileno(gclh->fp), &inset, NULL, NULL, &zerotime);
	    if (i <= 0)
	    {
		if (errno == EINTR)
		    continue;
		if (i < 0)
		{
		    //XXX print or log something here?
		    estat = ep_stat_from_errno(errno);
		}
		else
		{
		    // no data to read and no error
		    estat = EP_STAT_END_OF_FILE;
		}
		goto fail0;
	    }
	}

	estat = get_gcl_rec(gclh->fp, msg, evb);
	EP_STAT_CHECK(estat, goto fail0);
	gcl_handle_save_offset(gclh, msg->msgno, msg->offset);
	if (msg->msgno > msgno)
	{
	    // oh bugger, we've already read too far
	    ep_app_error("gcl_read: read too far (cur=%d, target=%d)",
		    msg->msgno, msgno);
	    estat = GDP_STAT_READ_OVERFLOW;
	}
	if (msg->msgno < msgno)
	{
	    ep_dbg_cprintf(Dbg, 3, "Skipping msg->msgno %d\n", msg->msgno);
	    if (evb != NULL)
		(void) evbuffer_drain(evb, evbuffer_get_length(evb));
	}
    } while (msg->msgno < msgno);

    // ok, done!
fail0:
    flock(fileno(gclh->fp), LOCK_UN);

    if (ep_dbg_test(Dbg, 4))
    {
	char sbuf[100];

	ep_dbg_printf("gcl_read => %s\n",
		ep_stat_tostr(estat, sbuf, sizeof sbuf));
    }
    return estat;
}


/************************  PUBLIC  ************************/

/*
**  GCL_CREATE --- create a new gcl
*/

EP_STAT
gcl_create(gcl_name_t gcl_name,
	gcl_handle_t **pgclh)
{
    gcl_handle_t *gclh = NULL;
    EP_STAT estat = EP_STAT_OK;
    int fd = -1;
    FILE *fp;
    char pbuf[GCL_PATH_LEN];

    // allocate the memory to hold the GCL Handle
    //	    Note that ep_mem_* always returns, hence no test here
    estat = gdpd_gclh_new(&gclh);
    EP_STAT_CHECK(estat, goto fail0);

    // allocate a name
    if (gdp_gcl_name_is_zero(gcl_name))
	gdp_gcl_newname(gcl_name);
    memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);

    // create a file node representing the gcl
    // XXX: this is where we start to seriously cheat
    estat = get_gcl_path(gclh, pbuf, sizeof pbuf);
    EP_STAT_CHECK(estat, goto fail1);
    if ((fd = open(pbuf, O_RDWR|O_CREAT|O_APPEND|O_EXCL, 0644)) < 0 ||
	(flock(fd, LOCK_EX) < 0))
    {
	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "gcl_create: cannot create %s: %s",
		pbuf, strerror(errno));
	goto fail1;
    }
    gclh->fp = fp = fdopen(fd, "a+");
    if (gclh->fp == NULL)
    {
	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "gcl_create: cannot fdopen %s: %s",
		pbuf, strerror(errno));
	goto fail1;
    }

    // write the header metadata
    fprintf(fp, "%s%d\n", GCL_MAGIC, GCL_VERSION);
    
    // TODO: will probably need creation date or some such later

    // success!
    fflush(fp);
    gcl_handle_save_offset(gclh, 0, ftell(fp));
    flock(fd, LOCK_UN);
    *pgclh = gclh;
    if (ep_dbg_test(Dbg, 10))
    {
	gcl_pname_t pname;

	gdp_gcl_printable_name(gclh->gcl_name, pname);
	ep_dbg_printf("Created GCL Handle %s\n", pname);
    }
    return estat;

fail1:
    if (fd >= 0)
	    close(fd);
    ep_mem_free(gclh);
fail0:
    if (ep_dbg_test(Dbg, 8))
    {
	char ebuf[100];

	ep_dbg_printf("Could not create GCL Handle: %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}


/*
**  GDP_GCL_OPEN --- open a gcl for reading or further appending
**
**	XXX: Should really specify whether we want to start reading:
**	(a) At the beginning of the log (easy).  This includes random
**	    access.
**	(b) Anything new that comes into the log after it is opened.
**	    To do this we need to read the existing log to find the end.
*/

EP_STAT
gcl_open(gcl_name_t gcl_name,
	    gdp_iomode_t mode,
	    gcl_handle_t **pgclh)
{
    EP_STAT estat = EP_STAT_OK;
    gcl_handle_t *gclh = NULL;
    FILE *fp;
    char pbuf[GCL_PATH_LEN];
    char nbuf[GCL_MAX_LINE];

    // XXX determine if name exists and is a gcl
    estat = gdpd_gclh_new(&gclh);
    EP_STAT_CHECK(estat, goto fail1);
    memcpy(gclh->gcl_name, gcl_name, sizeof gclh->gcl_name);
    gclh->iomode = mode;
    {
	const char *openmode;

	estat = get_gcl_path(gclh, pbuf, sizeof pbuf);
	EP_STAT_CHECK(estat, goto fail0);
	// temporarily open everything read/append
//	if (mode == GDP_MODE_RO)
//	    openmode = "r";
//	else
	    openmode = "a+";
//	openmode = "rw";
	if ((fp = fopen(pbuf, openmode)) == NULL ||
	    (flock(fileno(fp), LOCK_SH) < 0))
	{
	    estat = ep_stat_from_errno(errno);
	    goto fail0;
	}
    }
    gclh->fp = fp;

    // check gcl header
    rewind(fp);
    if (fgets(nbuf, sizeof nbuf, fp) == NULL)
    {
	if (ferror(fp))
	{
	    estat = ep_stat_from_errno(errno);
	}
	else
	{
	    // must be end of file (bad format gcl)
	    estat = ep_stat_from_errno(EINVAL);	    // XXX: should be more specific
	}
	flock(fileno(fp), LOCK_UN);
	goto fail0;
    }
    if (strncmp(nbuf, GCL_MAGIC, sizeof GCL_MAGIC - 1) != 0)
    {
	estat = ep_stat_from_errno(EINVAL);
	gdp_log(estat, "gcl_open: invalid format %*s",
		(int) (sizeof GCL_MAGIC - 1), nbuf);
	flock(fileno(fp), LOCK_UN);
	goto fail0;
    }
    gclh->ver = atoi(&nbuf[sizeof GCL_MAGIC - 1]);

    // skip possible metadata
    while (fgets(nbuf, sizeof nbuf, fp) != NULL)
    {
	if (nbuf[0] == '\n')
	    break;
    }
    ungetc('\n', fp);
    flock(fileno(fp), LOCK_UN);

    // we should now be pointed at the first message (or end of file)
    gcl_handle_save_offset(gclh, 0, ftell(fp));

    // if we are appending, we have to find the last message number
    if (mode == GDP_MODE_AO)
    {
	// populate the offset cache
	gdp_msg_t msg;

	gcl_read(gclh, INT32_MAX, &msg, NULL);
	gclh->msgno = msg.msgno;
    }

    // success!
    *pgclh = gclh;
    if (ep_dbg_test(Dbg, 10))
    {
	gcl_pname_t pname;

	gdp_gcl_printable_name(gclh->gcl_name, pname);
	ep_dbg_printf("Opened gcl %s\n", pname);
    }
    return estat;

fail1:
    estat = ep_stat_from_errno(errno);

fail0:
    if (gclh == NULL)
	ep_mem_free(gclh);

    {
	char ebuf[100];

	//XXX should log
	fprintf(stderr, "gcl_open: Couldn't open gcl: %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    if (ep_dbg_test(Dbg, 10))
    {
	gcl_pname_t pname;
	char ebuf[100];

	gdp_gcl_printable_name(gcl_name, pname);
	ep_dbg_printf("Couldn't open gcl %s: %s\n",
		pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}

/*
**  GDP_GCL_CLOSE --- close an open gcl
*/

EP_STAT
gcl_close(gcl_handle_t *gclh)
{
    EP_STAT estat = EP_STAT_OK;

    EP_ASSERT_POINTER_VALID(gclh);
    if (gclh->fp == NULL)
    {
	estat = EP_STAT_ERROR;
	gdp_log(estat, "gcl_close: null fp");
    }
    else
    {
	fclose(gclh->fp);
    }
    evbuffer_free(gclh->revb);
    ep_mem_free(gclh->offcache);
    gdp_gcl_cache_drop(gclh->gcl_name, 0);
    ep_mem_free(gclh);

    return estat;
}

/*
**  GDP_GCL_APPEND --- append a message to a writable gcl
*/

EP_STAT
gcl_append(gcl_handle_t *gclh,
	    gdp_msg_t *msg)
{
    EP_STAT estat = EP_STAT_OK;

    EP_ASSERT_POINTER_VALID(gclh);
    EP_ASSERT_POINTER_VALID(msg);

    // get a timestamp
    (void) tt_now(&msg->ts);

    // XXX: check that GCL Handle is writable

    // make sure no write conflicts
    flock(fileno(gclh->fp), LOCK_EX);
    ep_dbg_cprintf(Dbg, 49,
	    "Back from locking for message %d, cachesize %ld\n",
	    msg->msgno, gclh->cachesize);

    // read any messages that may have been added by another writer
    {
	gdp_msg_t xmsg;

	gcl_read(gclh, INT32_MAX, &xmsg, NULL);
    }

    // see what the offset is and cache that
    estat = gcl_handle_save_offset(gclh, ++gclh->msgno,
				ftell(gclh->fp));
    EP_STAT_CHECK(estat, goto fail0);
    ep_dbg_cprintf(Dbg, 18, "gcl_append: saved offset for msgno %d\n",
	    gclh->msgno);

    // write the message out
    // TODO: check for errors
    fprintf(gclh->fp, "%s%d ", MSG_MAGIC, gclh->msgno);
    if (msg->ts.stamp.tv_sec != TT_NOTIME)
	tt_print_interval(&msg->ts, gclh->fp, false);
    else
	fprintf(gclh->fp, "-");
    fprintf(gclh->fp, " %zu\n", msg->len);
    fwrite(msg->data, msg->len, 1, gclh->fp);
    msg->msgno = gclh->msgno;
    fflush(gclh->fp);
    //XXX

    // ok, done!
fail0:
    flock(fileno(gclh->fp), LOCK_UN);

    return estat;
}


/*
**  GDP_GCL_SUBSCRIBE --- subscribe to a gcl
*/

struct gdp_cb_arg
{
    struct event	    *event;	// event triggering this callback
    gcl_handle_t	    *gclh;	// GCL Handle triggering this callback
    gdp_gcl_sub_cbfunc_t    cbfunc;	// function to call
    void		    *cbarg;	// argument to pass to cbfunc
    void		    *buf;	// space to put the message
    size_t		    bufsiz;	// size of buf
};


static void
gcl_sub_event_cb(evutil_socket_t fd,
	short what,
	void *cbarg)
{
    gdp_msg_t msg;
    EP_STAT estat;
    struct gdp_cb_arg *cba = cbarg;

    // get the next message from this GCL Handle
    estat = gcl_read(cba->gclh, GCL_NEXT_MSG, &msg, cba->gclh->revb);
    if (EP_STAT_ISOK(estat))
	(*cba->cbfunc)(cba->gclh, &msg, cba->cbarg);
    //XXX;
}


EP_STAT
gcl_subscribe(gcl_handle_t *gclh,
	gdp_gcl_sub_cbfunc_t cbfunc,
	long offset,
	void *buf,
	size_t bufsiz,
	void *cbarg)
{
    EP_STAT estat = EP_STAT_OK;
    struct gdp_cb_arg *cba;
    struct timeval timeout = { 0, 100000 };	// 100 msec

    EP_ASSERT_POINTER_VALID(gclh);
    EP_ASSERT_POINTER_VALID(cbfunc);

    cba = ep_mem_zalloc(sizeof *cba);
    cba->gclh = gclh;
    cba->cbfunc = cbfunc;
    cba->buf = buf;
    cba->bufsiz = bufsiz;
    cba->cbarg = cbarg;
    cba->event = event_new(GdpEventBase, fileno(gclh->fp),
	    EV_READ|EV_PERSIST, &gcl_sub_event_cb, cba);
    event_add(cba->event, &timeout);
    //XXX;
    return estat;
}

#if 0
EP_STAT
gcl_unsubscribe(gcl_handle_t *gclh,
	void (*cbfunc)(gcl_handle_t *, void *),
	void *arg)
{
    EP_ASSERT_POINTER_VALID(gclh);
    EP_ASSERT_POINTER_VALID(cbfunc);

    XXX;
}
#endif
