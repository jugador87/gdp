/* vim: set ai sw=4 sts=4 : */

#define GDP_MSG_EXTRA \
	    off_t offset;

#include <gdpd/gdpd_router.h>
#include <gdp/gdp.h>
#include <gdp/gdp_log.h>
#include <gdp/gdp_protocol.h>
#include <gdp/gdp_stat.h>
#include <gdp/gdp_priv.h>
#include <pthread.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_mem.h>
#include <ep/ep_string.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gdpd", "GDP Daemon");

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
**  A handle on an open connection.
*/

typedef struct
{
    int	    dummy;	    // unused
} conn_t;


EP_STAT
implement_me(char *s)
{
    ep_app_error("Not implemented: %s", s);
    return GDP_STAT_NOT_IMPLEMENTED;
}


/*
**  GCL_NEW --- create a new GCL Handle & initialize
**
**  Returns:
**	New GCL Handle if possible
**	NULL otherwise
*/

static EP_STAT
gcl_handle_new(gcl_handle_t **pgclh)
{
    EP_STAT estat = EP_STAT_OK;
    gcl_handle_t *gclh;

    // allocate the memory to hold the GCL Handle
    gclh = ep_mem_zalloc(sizeof *gclh);
    if (gclh == NULL)
	goto fail1;

    // allocate an initial set of offset pointers
    gclh->cachesize = ep_adm_getintparam("swarm.gdp.cachesize",
					    INITIAL_MSG_OFFCACHE);
    if (gclh->cachesize < INITIAL_MSG_OFFCACHE)
	gclh->cachesize = INITIAL_MSG_OFFCACHE;
    gclh->offcache = ep_mem_zalloc(gclh->cachesize * sizeof gclh->offcache[0]);
    if (gclh->offcache == NULL)
	goto fail1;

    // get space to store output, e.g., for processing reads
    gclh->revb = evbuffer_new();
    if (gclh->revb == NULL)
	goto fail1;

    // success
    *pgclh = gclh;
    return estat;

fail1:
    estat = ep_stat_from_errno(errno);
    return estat;
}


/*
**  CREATE_GCL_NAME --- create a name for a new gcl
**
**	Probably not the best implementation.
*/

static void
create_gcl_name(gcl_name_t np)
{
    evutil_secure_rng_get_bytes(np, sizeof (gcl_name_t));
}


/*
**  GCL_PRINT --- print a GCL for debugging
*/

EP_STAT
gcl_print(
	    const gcl_handle_t *gclh,
	    FILE *fp,
	    int detail,
	    int indent)
{
    gcl_pname_t nbuf;

    EP_ASSERT_POINTER_VALID(gclh);

    gdp_gcl_printable_name(gclh->gcl_name, nbuf);
    fprintf(fp, "GCL: %s\n", nbuf);
    return EP_STAT_OK;
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
	    long msgno,
	    off_t off)
{
    ep_dbg_cprintf(Dbg, 8,
	    "gcl_handle_save_offset: caching msgno %ld offset %lld\n",
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
		"gcl_handle_save_offset: offset for message %ld has moved from %lld to %lld\n",
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

	    msg->ts_valid = true;
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
	    long msgno,
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
		"gcl_read: using cached offset %lld for msgno %ld\n",
		gclh->offcache[msgno], msgno);
	if (fseek(gclh->fp, gclh->offcache[msgno], SEEK_SET) < 0)
	{
	    estat = ep_stat_from_errno(errno);
	    goto fail0;
	}
    }

    // we may have to skip ahead, hence the do loop
    ep_dbg_cprintf(Dbg, 7, "gcl_read: looking for msgno %ld\n", msgno);
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
	    ep_app_error("gcl_read: read too far (cur=%ld, target=%ld)",
		    msg->msgno, msgno);
	    estat = GDP_STAT_READ_OVERFLOW;
	}
	if (msg->msgno < msgno)
	{
	    ep_dbg_cprintf(Dbg, 3, "Skipping msg->msgno %ld\n", msg->msgno);
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
    estat = gcl_handle_new(&gclh);
    EP_STAT_CHECK(estat, goto fail0);

    // allocate a name
    if (gdp_gcl_name_is_zero(gcl_name))
	create_gcl_name(gcl_name);
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
    estat = gcl_handle_new(&gclh);
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
    if (EP_STAT_ISOK(tt_now(&msg->ts)))
	msg->ts_valid = true;

    // XXX: check that GCL Handle is writable

    // make sure no write conflicts
    flock(fileno(gclh->fp), LOCK_EX);
    ep_dbg_cprintf(Dbg, 49,
	    "Back from locking for message %ld, cachesize %ld\n",
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
    ep_dbg_cprintf(Dbg, 18, "gcl_append: saved offset for msgno %ld\n", gclh->msgno);

    // write the message out
    // TODO: check for errors
    fprintf(gclh->fp, "%s%ld ", MSG_MAGIC, gclh->msgno);
    if (msg->ts_valid)
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



/*
**  GDP_GCL_GETNAME --- get the name of a gcl
*/

const gcl_name_t *
gcl_getname(const gcl_handle_t *gclh)
{
    return &gclh->gcl_name;
}


/*
**  GDP_GCL_MSG_PRINT --- print a message (for debugging)
*/

void
gcl_msg_print(const gdp_msg_t *msg,
	    FILE *fp)
{
    int i;

    fprintf(fp, "GCL Message %ld, len %zu", msg->msgno, msg->len);
    if (msg->ts_valid)
    {
	fprintf(fp, ", timestamp ");
	tt_print_interval(&msg->ts, fp, true);
    }
    else
    {
	fprintf(fp, ", no timestamp");
    }
    i = msg->len;
    fprintf(fp, "\n  %s%.*s%s\n", EpChar->lquote, i, msg->data, EpChar->rquote);
}


/*
**  GDPD_GCL_ERROR --- helper routine for returning errors
*/

EP_STAT
gdpd_gcl_error(gcl_name_t gcl_name, char *msg, EP_STAT logstat, int nak)
{
    gcl_pname_t pname;

    gdp_gcl_printable_name(gcl_name, pname);
    gdp_log(logstat, "%s: %s", msg, pname);
    return GDP_STAT_FROM_NAK(GDP_NAK_C_BADREQ);
}

/*
**  Command implementations
*/

EP_STAT
cmd_create(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    EP_STAT estat;
    gcl_handle_t *gclh;

    estat = gcl_create(cpkt->gcl_name, &gclh);
    if (EP_STAT_ISOK(estat))
    {
	// cache the open GCL Handle for possible future use
	EP_ASSERT_INSIST(!gdp_gcl_name_is_zero(gclh->gcl_name));
	gdp_gcl_cache_add(gclh, GDP_MODE_AO);

	// pass the name back to the caller
	memcpy(rpkt->gcl_name, gclh->gcl_name, sizeof rpkt->gcl_name);
    }
    return estat;
}


EP_STAT
cmd_open_xx(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt, int iomode)
{
    EP_STAT estat;
    gcl_handle_t *gclh;

    gclh = gdp_gcl_cache_get(cpkt->gcl_name, iomode);
    if (gclh != NULL)
    {
	if (ep_dbg_test(Dbg, 12))
	{
	    gcl_pname_t pname;

	    gdp_gcl_printable_name(cpkt->gcl_name, pname);
	    ep_dbg_printf("cmd_open_xx: using cached handle for %s\n", pname);
	}
	rewind(gclh->fp);	// make sure we can switch modes (read/write)
	return EP_STAT_OK;
    }
    if (ep_dbg_test(Dbg, 12))
    {
	gcl_pname_t pname;

	gdp_gcl_printable_name(cpkt->gcl_name, pname);
	ep_dbg_printf("cmd_open_xx: doing initial open for %s\n", pname);
    }
    estat = gcl_open(cpkt->gcl_name, iomode, &gclh);
    if (EP_STAT_ISOK(estat))
	gdp_gcl_cache_add(gclh, iomode);
    return estat;
}


EP_STAT
cmd_open_ao(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    return cmd_open_xx(bev, c, cpkt, rpkt, GDP_MODE_AO);
}


EP_STAT
cmd_open_ro(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    return cmd_open_xx(bev, c, cpkt, rpkt, GDP_MODE_RO);
}


EP_STAT
cmd_close(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    gcl_handle_t *gclh;

    gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_ANY);
    if (gclh == NULL)
    {
	return gdpd_gcl_error(cpkt->gcl_name, "cmd_close: GCL not open",
			    GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
    }
    return gcl_close(gclh);
}


EP_STAT
cmd_read(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    gcl_handle_t *gclh;
    gdp_msg_t msg;
    EP_STAT estat;

    gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_RO);
    if (gclh == NULL)
    {
	return gdpd_gcl_error(cpkt->gcl_name, "cmd_read: GCL not open",
			    GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
    }

    estat = gcl_read(gclh, cpkt->msgno, &msg, gclh->revb);
    EP_STAT_CHECK(estat, goto fail0);
    rpkt->dlen = EP_STAT_TO_LONG(estat);

fail0:
    if (EP_STAT_IS_SAME(estat, EP_STAT_END_OF_FILE))
	rpkt->cmd = GDP_NAK_C_NOTFOUND;
    return estat;
}


EP_STAT
cmd_publish(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    gcl_handle_t *gclh;
    char *dp;
    char dbuf[1024];
    gdp_msg_t msg;
    EP_STAT estat;
    int i;

    gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_AO);
    if (gclh == NULL)
    {
	return gdpd_gcl_error(cpkt->gcl_name, "cmd_publish: GCL not open",
			    GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
    }

    if (cpkt->dlen > sizeof dbuf)
	dp = ep_mem_malloc(cpkt->dlen);
    else
	dp = dbuf;

    // read in the data from the bufferevent
    i = evbuffer_remove(bufferevent_get_input(bev), dp, cpkt->dlen);
    if (i < cpkt->dlen)
    {
	return gdpd_gcl_error(cpkt->gcl_name, "cmd_publish: short read",
			    GDP_STAT_SHORTMSG, GDP_NAK_S_INTERNAL);
    }

    // create the message
    memset(&msg, 0, sizeof msg);
    msg.len = cpkt->dlen;
    msg.data = dp;
    msg.msgno = cpkt->msgno;
    memcpy(&msg.ts, &cpkt->ts, sizeof msg.ts);

    estat = gcl_append(gclh, &msg);

    if (dp != dbuf)
	ep_mem_free(dp);

    // fill in return information
    rpkt->msgno = msg.msgno;
    return estat;
}


EP_STAT
cmd_subscribe(struct bufferevent *bev, conn_t *c,
	gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    gcl_handle_t *gclh;

    gclh = gdp_gcl_cache_get(cpkt->gcl_name, GDP_MODE_RO);
    if (gclh == NULL)
    {
	return gdpd_gcl_error(cpkt->gcl_name, "cmd_subscribe: GCL not open",
			    GDP_STAT_NOT_OPEN, GDP_NAK_C_BADREQ);
    }
    return implement_me("cmd_subscribe");
}


EP_STAT
cmd_find_obj(struct bufferevent *bev, conn_t *c,
    gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    return router_find_obj(bev, c, cpkt, rpkt);
}


EP_STAT
cmd_not_implemented(struct bufferevent *bev, conn_t *c,
		gdp_pkt_hdr_t *cpkt, gdp_pkt_hdr_t *rpkt)
{
    //XXX print/log something here?

    rpkt->cmd = GDP_NAK_S_INTERNAL;
    gdp_pkt_out_hard(rpkt, bufferevent_get_output(bev));
    return GDP_STAT_NOT_IMPLEMENTED;
}


typedef EP_STAT cmdfunc_t(struct bufferevent *bev,
			conn_t *conn,
			gdp_pkt_hdr_t *cpkt,
			gdp_pkt_hdr_t *rpkt);

typedef struct
{
    cmdfunc_t	*func;		// function to call
    uint8_t	ok_stat;	// default OK ack/nak status
} dispatch_ent_t;



/*
**  Dispatch Table
**
**	One per possible command/ack/nak.
*/

#define NOENT	    { NULL, 0 }

dispatch_ent_t	DispatchTable[256] =
{
    NOENT,  // 0
    NOENT,                   // 1
    NOENT,                   // 2
    NOENT,                   // 3
    NOENT,                   // 4
    NOENT,                   // 5
    NOENT,                   // 6
    NOENT,                   // 7
    NOENT,                   // 8
    NOENT,                   // 9
    NOENT,                   // 10
    NOENT,                   // 11
    NOENT,                   // 12
    NOENT,                   // 13
    NOENT,                   // 14
    NOENT,                   // 15
    NOENT,                   // 16
    NOENT,                   // 17
    NOENT,                   // 18
    NOENT,                   // 19
    NOENT,                   // 20
    NOENT,                   // 21
    NOENT,                   // 22
    NOENT,                   // 23
    NOENT,                   // 24
    NOENT,                   // 25
    NOENT,                   // 26
    NOENT,                   // 27
    NOENT,                   // 28
    NOENT,                   // 29
    NOENT,                   // 30
    NOENT,                   // 31
    NOENT,                   // 32
    NOENT,                   // 33
    NOENT,                   // 34
    NOENT,                   // 35
    NOENT,                   // 36
    NOENT,                   // 37
    NOENT,                   // 38
    NOENT,                   // 39
    NOENT,                   // 40
    NOENT,                   // 41
    NOENT,                   // 42
    NOENT,                   // 43
    NOENT,                   // 44
    NOENT,                   // 45
    NOENT,                   // 46
    NOENT,                   // 47
    NOENT,                   // 48
    NOENT,                   // 49
    NOENT,                   // 50
    NOENT,                   // 51
    NOENT,                   // 52
    NOENT,                   // 53
    NOENT,                   // 54
    NOENT,                   // 55
    NOENT,                   // 56
    NOENT,                   // 57
    NOENT,                   // 58
    NOENT,                   // 59
    NOENT,                   // 60
    NOENT,                   // 61
    NOENT,                   // 62
    NOENT,                   // 63
    NOENT,                   // 64
    NOENT,                   // 65
    { cmd_create,	GDP_ACK_DATA_CREATED	},		// 66
    { cmd_open_ao,	GDP_ACK_SUCCESS		},		// 67
    { cmd_open_ro,	GDP_ACK_SUCCESS		},		// 68
    { cmd_close,	GDP_ACK_SUCCESS		},		// 69
    { cmd_read,		GDP_ACK_DATA_CONTENT	},		// 70
    { cmd_publish,	GDP_ACK_DATA_CREATED	},		// 71
    { cmd_subscribe,	GDP_ACK_SUCCESS		},		// 72
    { cmd_find_obj,     GDP_ACK_SUCCESS     },      // 73
    NOENT,                   // 74
    NOENT,                   // 75
    NOENT,                   // 76
    NOENT,                   // 77
    NOENT,                   // 78
    NOENT,                   // 79
    NOENT,                   // 80
    NOENT,                   // 81
    NOENT,                   // 82
    NOENT,                   // 83
    NOENT,                   // 84
    NOENT,                   // 85
    NOENT,                   // 86
    NOENT,                   // 87
    NOENT,                   // 88
    NOENT,                   // 89
    NOENT,                   // 90
    NOENT,                   // 91
    NOENT,                   // 92
    NOENT,                   // 93
    NOENT,                   // 94
    NOENT,                   // 95
    NOENT,                   // 96
    NOENT,                   // 97
    NOENT,                   // 98
    NOENT,                   // 99
    NOENT,                   // 100
    NOENT,                   // 101
    NOENT,                   // 102
    NOENT,                   // 103
    NOENT,                   // 104
    NOENT,                   // 105
    NOENT,                   // 106
    NOENT,                   // 107
    NOENT,                   // 108
    NOENT,                   // 109
    NOENT,                   // 110
    NOENT,                   // 111
    NOENT,                   // 112
    NOENT,                   // 113
    NOENT,                   // 114
    NOENT,                   // 115
    NOENT,                   // 116
    NOENT,                   // 117
    NOENT,                   // 118
    NOENT,                   // 119
    NOENT,                   // 120
    NOENT,                   // 121
    NOENT,                   // 122
    NOENT,                   // 123
    NOENT,                   // 124
    NOENT,                   // 125
    NOENT,                   // 126
    NOENT,                   // 127
    NOENT,                   // 128
    NOENT,                   // 129
    NOENT,                   // 130
    NOENT,                   // 131
    NOENT,                   // 132
    NOENT,                   // 133
    NOENT,                   // 134
    NOENT,                   // 135
    NOENT,                   // 136
    NOENT,                   // 137
    NOENT,                   // 138
    NOENT,                   // 139
    NOENT,                   // 140
    NOENT,                   // 141
    NOENT,                   // 142
    NOENT,                   // 143
    NOENT,                   // 144
    NOENT,                   // 145
    NOENT,                   // 146
    NOENT,                   // 147
    NOENT,                   // 148
    NOENT,                   // 149
    NOENT,                   // 150
    NOENT,                   // 151
    NOENT,                   // 152
    NOENT,                   // 153
    NOENT,                   // 154
    NOENT,                   // 155
    NOENT,                   // 156
    NOENT,                   // 157
    NOENT,                   // 158
    NOENT,                   // 159
    NOENT,                   // 160
    NOENT,                   // 161
    NOENT,                   // 162
    NOENT,                   // 163
    NOENT,                   // 164
    NOENT,                   // 165
    NOENT,                   // 166
    NOENT,                   // 167
    NOENT,                   // 168
    NOENT,                   // 169
    NOENT,                   // 170
    NOENT,                   // 171
    NOENT,                   // 172
    NOENT,                   // 173
    NOENT,                   // 174
    NOENT,                   // 175
    NOENT,                   // 176
    NOENT,                   // 177
    NOENT,                   // 178
    NOENT,                   // 179
    NOENT,                   // 180
    NOENT,                   // 181
    NOENT,                   // 182
    NOENT,                   // 183
    NOENT,                   // 184
    NOENT,                   // 185
    NOENT,                   // 186
    NOENT,                   // 187
    NOENT,                   // 188
    NOENT,                   // 189
    NOENT,                   // 190
    NOENT,                   // 191
    NOENT,                   // 192
    NOENT,                   // 193
    NOENT,                   // 194
    NOENT,                   // 195
    NOENT,                   // 196
    NOENT,                   // 197
    NOENT,                   // 198
    NOENT,                   // 199
    NOENT,                   // 200
    NOENT,                   // 201
    NOENT,                   // 202
    NOENT,                   // 203
    NOENT,                   // 204
    NOENT,                   // 205
    NOENT,                   // 206
    NOENT,                   // 207
    NOENT,                   // 208
    NOENT,                   // 209
    NOENT,                   // 210
    NOENT,                   // 211
    NOENT,                   // 212
    NOENT,                   // 213
    NOENT,                   // 214
    NOENT,                   // 215
    NOENT,                   // 216
    NOENT,                   // 217
    NOENT,                   // 218
    NOENT,                   // 219
    NOENT,                   // 220
    NOENT,                   // 221
    NOENT,                   // 222
    NOENT,                   // 223
    NOENT,                   // 224
    NOENT,                   // 225
    NOENT,                   // 226
    NOENT,                   // 227
    NOENT,                   // 228
    NOENT,                   // 229
    NOENT,                   // 230
    NOENT,                   // 231
    NOENT,                   // 232
    NOENT,                   // 233
    NOENT,                   // 234
    NOENT,                   // 235
    NOENT,                   // 236
    NOENT,                   // 237
    NOENT,                   // 238
    NOENT,                   // 239
    NOENT,                   // 240
    NOENT,                   // 241
    NOENT,                   // 242
    NOENT,                   // 243
    NOENT,                   // 244
    NOENT,                   // 245
    NOENT,                   // 246
    NOENT,                   // 247
    NOENT,                   // 248
    NOENT,                   // 249
    NOENT,                   // 250
    NOENT,                   // 251
    NOENT,                   // 252
    NOENT,                   // 253
    NOENT,                   // 254
    NOENT,                   // 255
};


/*
**  DISPATCHCMD --- dispatch a command via the DispatchTable
**
**  Parameters:
**	d --- a pointer to the dispatch table entry
**	conn --- the connection handle
**	cpkt --- the command packet
**	rpkt --- filled in with a response packet
*/

EP_STAT
dispatch_cmd(dispatch_ent_t *d,
	    conn_t *conn,
	    gdp_pkt_hdr_t *cpkt,
	    gdp_pkt_hdr_t *rpkt,
	    struct bufferevent *bev)
{
    EP_STAT estat;

    ep_dbg_cprintf(Dbg, 18, "dispatch_cmd: >>> command %d\n", cpkt->cmd);

    // set up response packet
    //	    XXX since we copy almost everything, should we just do it
    //		and skip the memset?
    memcpy(rpkt, cpkt, sizeof *rpkt);
    rpkt->cmd = d->ok_stat;
    rpkt->dlen = 0;

    if (d->func == NULL)
	estat = cmd_not_implemented(bev, conn, cpkt, rpkt);
    else
	estat = (*d->func)(bev, conn, cpkt, rpkt);

    if (!EP_STAT_ISOK(estat) && rpkt->cmd < GDP_NAK_MIN)
    {
	// function didn't specify return code; take a guess ourselves
//	if (EP_STAT_REGISTRY(estat) == EP_REGISTRY_UCB &&
//		EP_STAT_MODULE(estat) == GDP_MODULE &&
//		EP_STAT_DETAIL(estat) >= GDP_ACK_MIN &&
//		EP_STAT_DETAIL(estat) <= GDP_NAK_MAX)
//	    rpkt->cmd = EP_STAT_DETAIL(estat);
//	else
	    rpkt->cmd = GDP_NAK_S_INTERNAL;
    }

    if (ep_dbg_test(Dbg, 18))
    {
	char ebuf[200];

	ep_dbg_printf("dispatch_cmd: <<< %d, status %d (%s)\n",
		cpkt->cmd, rpkt->cmd, ep_stat_tostr(estat, ebuf, sizeof ebuf));
	if (ep_dbg_test(Dbg, 30))
	    gdp_pkt_dump_hdr(rpkt, ep_dbg_getfile());
    }
    return estat;
}



/*
**  LEV_READ_CB --- handle reads on command sockets
*/

void
lev_read_cb(struct bufferevent *bev, void *ctx)
{
    EP_STAT estat;
    gdp_pkt_hdr_t cpktbuf;
    gdp_pkt_hdr_t rpktbuf;
    dispatch_ent_t *d;
    struct evbuffer *evb = NULL;

    estat = gdp_pkt_in(&cpktbuf, bufferevent_get_input(bev));
    if (EP_STAT_IS_SAME(estat, GDP_STAT_KEEP_READING))
	return;

    // got the packet, dispatch it based on the command
    d = &DispatchTable[cpktbuf.cmd];
    estat = dispatch_cmd(d, ctx, &cpktbuf, &rpktbuf, bev);
    //XXX anything to do with estat here?

    // find the GCL handle, if any
    {
	gcl_handle_t *gclh;

	gclh = gdp_gcl_cache_get(rpktbuf.gcl_name, 0);
	if (gclh != NULL)
	{
	    evb = gclh->revb;
	    if (ep_dbg_test(Dbg, 40))
	    {
		gcl_pname_t pname;

		gdp_gcl_printable_name(gclh->gcl_name, pname);
		ep_dbg_printf("lev_read_cb: using evb %p from %s\n", evb, pname);
	    }
	}
    }

    // see if we have any return data
    rpktbuf.dlen = (evb == NULL ? 0 : evbuffer_get_length(evb));

    ep_dbg_cprintf(Dbg, 41, "lev_read_cb: sending %d bytes\n", rpktbuf.dlen);

    // make sure nothing sneaks in...
    if (rpktbuf.dlen > 0)
	evbuffer_lock(evb);

    // send the response packet header
    estat = gdp_pkt_out(&rpktbuf, bufferevent_get_output(bev));
    //XXX anything to do with estat here?

    // copy any data
    if (rpktbuf.dlen > 0)
    {
	// still experimental
	//evbuffer_add_buffer_reference(bufferevent_get_output(bev), evb);

	// slower, but works
	int left = rpktbuf.dlen;

	while (left > 0)
	{
	    uint8_t buf[1024];
	    int len = left;
	    int i;

	    if (len > sizeof buf)
		len = sizeof buf;
	    i = evbuffer_remove(evb, buf, len);
	    if (i < len)
	    {
		ep_dbg_cprintf(Dbg, 2,
			"lev_read_cb: short read (wanted %d, got %d)\n", len, i);
		estat = GDP_STAT_SHORTMSG;
	    }
	    if (i <= 0)
		break;
	    evbuffer_add(bufferevent_get_output(bev), buf, i);
	    if (ep_dbg_test(Dbg, 43))
	    {
		ep_hexdump(buf, i, ep_dbg_getfile(), 0);
	    }
	    left -= i;
	}
    }

    // we can now unlock
    if (rpktbuf.dlen > 0)
	    evbuffer_unlock(evb);
}


/*
**  LEV_EVENT_CB --- handle special events on socket
*/

void
lev_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (EP_UT_BITSET(BEV_EVENT_ERROR, events))
    {
	EP_STAT estat = ep_stat_from_errno(errno);
	int err = EVUTIL_SOCKET_ERROR();

	gdp_log(estat, "error on command socket: %d (%s)",
		err, evutil_socket_error_to_string(err));
    }

    if (EP_UT_BITSET(BEV_EVENT_ERROR | BEV_EVENT_EOF, events))
	bufferevent_free(bev);
}


/*
**  ACCEPT_CB --- called when a new connection is accepted
*/

void
accept_cb(struct evconnlistener *lev,
	evutil_socket_t sockfd,
	struct sockaddr *sa,
	int salen,
	void *ctx)
{
    struct event_base *evbase = evconnlistener_get_base(lev);
    struct bufferevent *bev = bufferevent_socket_new(evbase, sockfd,
		    BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    union sockaddr_xx saddr;
    socklen_t slen = sizeof saddr;

    if (bev == NULL)
    {
	gdp_log(ep_stat_from_errno(errno),
		"accept_cb: could not allocate bufferevent");
	return;
    }

    if (getpeername(sockfd, &saddr.sa, &slen) < 0)
    {
	gdp_log(ep_stat_from_errno(errno),
		"accept_cb: connection from unknown peer");
    }
    else if (ep_dbg_test(Dbg, 20))
    {
	char abuf[INET6_ADDRSTRLEN];

	switch (saddr.sa.sa_family)
	{
	case AF_INET:
	    inet_ntop(saddr.sa.sa_family, &saddr.sin.sin_addr,
		    abuf, sizeof abuf);
	    break;
	case AF_INET6:
	    inet_ntop(saddr.sa.sa_family, &saddr.sin6.sin6_addr,
		    abuf, sizeof abuf);
	    break;
	default:
	    strcpy("<unknown>", abuf);
	    break;
	}
	ep_dbg_printf("accept_cb: connection from %s\n", abuf);
    }

    bufferevent_setcb(bev, lev_read_cb, NULL, lev_event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}


/*
**  LISTENER_ERROR_CB --- called if there is an error when listening
*/

void
listener_error_cb(struct evconnlistener *lev, void *ctx)
{
    struct event_base *evbase = evconnlistener_get_base(lev);
    int err = EVUTIL_SOCKET_ERROR();
    EP_STAT estat;

    estat = ep_stat_from_errno(errno);
    gdp_log(estat, "listener error %d (%s)",
	    err, evutil_socket_error_to_string(err));
    event_base_loopexit(evbase, NULL);
}


/*
**  GDP_INIT --- initialize this library
**
**	We intentionally do not call gdp_init() because that opens
**	an outgoing socket, which we're not (yet) using.  We do set
**	up GdpEventBase, but with different options.
*/

static EP_STAT
init_error(const char *msg)
{
    int eno = errno;
    EP_STAT estat = ep_stat_from_errno(eno);

    gdp_log(estat, "gdpd_init: %s", msg);
    ep_app_error("gdpd_init: %s: %s", msg, strerror(eno));
    return estat;
}

EP_STAT
gdpd_init(int listenport)
{
    EP_STAT estat = EP_STAT_OK;
    struct evconnlistener *lev;

    if (evthread_use_pthreads() < 0)
    {
	estat = init_error("evthread_use_pthreads failed");
	goto fail0;
    }

    if (GdpEventBase == NULL)
    {
	// Initialize the EVENT library
	struct event_config *ev_cfg = event_config_new();

	event_config_require_features(ev_cfg, EV_FEATURE_FDS);
	GdpEventBase = event_base_new_with_config(ev_cfg);
	if (GdpEventBase == NULL)
	{
	    estat = ep_stat_from_errno(errno);
	    ep_app_error("gdpd_init: could not create event base: %s",
		    strerror(errno));
	}
	event_config_free(ev_cfg);
    }

    gdp_gcl_cache_init();

    // set up the incoming evconnlistener
    if (listenport <= 0)
	listenport = ep_adm_getintparam("swarm.gdp.controlport",
					GDP_PORT_DEFAULT);
    {
	union sockaddr_xx saddr;

	saddr.sin.sin_family = AF_INET;
	saddr.sin.sin_addr.s_addr = INADDR_ANY;
	saddr.sin.sin_port = htons(listenport);
	lev = evconnlistener_new_bind(GdpEventBase,
		accept_cb,
		NULL,	    // context
		LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE,
		-1,
		&saddr.sa,
		sizeof saddr.sin);
    }
    if (lev == NULL)
	estat = init_error("gdpd_init: could not create evconnlistener");
    else
	evconnlistener_set_error_cb(lev, listener_error_cb);
    EP_STAT_CHECK(estat, goto fail0);

    // create a thread to run the event loop
//    estat = _gdp_start_event_loop_thread(GdpEventBase);
//    EP_STAT_CHECK(estat, goto fail0);
    
    // initialize router
    // TODO add estat stuff
    // XXX should be here? Should call gdp_init as mentioned above?
    router_init()

    // success!
    ep_dbg_cprintf(Dbg, 1, "gdpd_init: listening on port %d\n", listenport);
    return estat;

fail0:
    {
	char ebuf[200];

	ep_dbg_cprintf(Dbg, 1, "gdpd_init: failed with stat %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}


int
main(int argc, char **argv)
{
    int opt;
    int listenport = -1;
    bool run_in_foreground = false;
    EP_STAT estat;

    while ((opt = getopt(argc, argv, "D:FP:")) > 0)
    {
	switch (opt)
	{
	case 'D':
	    run_in_foreground = true;
	    ep_dbg_set(optarg);
	    break;

	case 'F':
	    run_in_foreground = true;
	    break;

	case 'P':
	    listenport = atoi(optarg);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    // initialize GDP and the EVENT library
    estat = gdpd_init(listenport);
    if (!EP_STAT_ISOK(estat))
    {
	char ebuf[100]; 

	ep_app_abort("Cannot initialize gdp library: %s",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }

    // need to manually run the event loop
    gdp_run_event_loop(NULL);

    // should never get here
    ep_app_abort("Fell out of event loop");
}
