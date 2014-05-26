/* vim: set ai sw=4 sts=4 : */

#include <gdp_nexus.h>
#include <gdp_log.h>
#include <gdp_stat.h>
#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <event2/event.h>

/*
**  This is currently a crappy implementation.  It's slow and not very
**  flexible and doesn't even start to consider distributed access.
**  XXX: Multiple writers don't work properly: since the message number is
**	local to a nexdle, two writers will allocate the same msgno.
**  XXX: Doesn't work with threaded processes: no in-process locking.
**
**  Nexuses are represented as files on disk in the following format:
**
**	#!GDP-Nexus <ver>\n
**	<metadata>\n
**	0*<message>
**
**  Messages are represented as:
**
**	\n#!MSG <msgno> <length>\n
**	<length bytes of binary data>
**
**  <msgno> starts at 1. XXX Why not zero???
**
**  Note that there is a blank line between the metadata and the
**  first message.
*/

/************************  PRIVATE  ************************/

static EP_DBG	Dbg = EP_DBG_INIT("gdp.nexus", "Nexus interface to GDP");

#define NEXUS_MAX_LINE		128	// max length of text line in nexus
#define NEXUS_PATH_LEN		200	// max length of pathname
#define NEXUS_DIR		"/var/tmp/nexus"
#define NEXUS_VERSION		0
#define NEXUS_MAGIC		"#!GDP-Nexus "
#define MSG_MAGIC		"\n#!MSG "


/*
**  A handle on an open nexus.
**
**	We cache the offsets of messages we have seen.  It's not clear
**	how useful this is, but without it moving backward through a
**	log is excrutiatingly slow.  The array is dynamically allocated,
**	so cachesize is the number of entries allocated and maxmsgno is
**	the last one actually known.
*/

#define INITIAL_MSG_OFFCACHE	256	// initial size of offset cache

struct nexdle_t
{
    nname_t	nname;		// the internal name
    gdpiomode_t	iomode;		// read only or append only
    long	ver;		// version number of on-disk file
    FILE	*fp;		// the stdio file pointer
    long	msgno;		// last msgno actually seen
    long	maxmsgno;	// maximum msgno that we have read/written
    off_t	*offcache;	// offsets of records we have seen
    long	cachesize;	// size of allocated offcache array
    uint32_t	flags;		// see below
};

// Nexdle flags
#define NEXDLE_ASYNC	    0x00000001	// do async I/O on this nexdle

#define NEXDLE_NEXT_MSG	    -1		// sentinel for next available message


/*
**  GDP_INIT --- initialize this library
*/

EP_STAT
gdp_init(void)
{
    EP_STAT estat = EP_STAT_OK;

    if (GdpEventBase == NULL)
    {
	// Initialize the EVENT library
	struct event_config *ev_cfg = event_config_new();

	event_config_require_features(ev_cfg, EV_FEATURE_FDS);
	GdpEventBase = event_base_new_with_config(ev_cfg);
	if (GdpEventBase == NULL)
	{
	    estat = ep_stat_from_errno(errno);
	    ep_app_error("could not create event base: %s", strerror(errno));
	}
	event_config_free(ev_cfg);
    }

    return estat;
}


/*
**  CREATE_NEXUS_NAME --- create a name for a new nexus
*/

static void
create_nexus_name(nname_t np)
{
    uuid_generate(np);
}

/*
**  NEXDLE_NEW --- create a new nexdle & initialize
**
**  Returns:
**	New nexdle if possible
**	NULL otherwise
*/

static EP_STAT
nexdle_new(nexdle_t **pnexdle)
{
    EP_STAT estat = EP_STAT_OK;
    nexdle_t *nexdle;

    // allocate the memory to hold the nexdle
    nexdle = ep_mem_zalloc(sizeof *nexdle);
    if (nexdle == NULL)
	goto fail1;

    // allocate an initial set of offset pointers
    nexdle->cachesize = ep_adm_getintparam("swarm.gdp.cachesize",
					    INITIAL_MSG_OFFCACHE);
    if (nexdle->cachesize < INITIAL_MSG_OFFCACHE)
	nexdle->cachesize = INITIAL_MSG_OFFCACHE;
    nexdle->offcache = calloc(nexdle->cachesize, sizeof nexdle->offcache[0]);
    if (nexdle->offcache == NULL)
	goto fail1;

    // success
    *pnexdle = nexdle;
    return estat;

fail1:
    estat = ep_stat_from_errno(errno);
    return estat;
}

EP_STAT
gdp_nexus_print(
	    const nexdle_t *nexdle,
	    FILE *sp,
	    int detail,
	    int indent)
{
    nexus_pname_t nbuf;

    EP_ASSERT_POINTER_VALID(nexdle);

    gdp_nexus_printable_name(nexdle->nname, nbuf);
    fprintf(sp, "NEXDLE: %s\n", nbuf);
    return EP_STAT_OK;
}

/*
**  GET_NEXUS_PATH --- get the pathname to an on-disk version of the nexus
*/

static EP_STAT
get_nexus_path(nexdle_t *nexdle, char *pbuf, int pbufsiz)
{
    nexus_pname_t pname;
    int i;

    EP_ASSERT_POINTER_VALID(nexdle);
    uuid_unparse(nexdle->nname, pname);
    i = snprintf(pbuf, pbufsiz, "%s/%s", NEXUS_DIR, pname);
    if (i < pbufsiz)
	return EP_STAT_OK;
    else
	return EP_STAT_ERROR;
}

/*
**  NEXDLE_SAVE_OFFSET --- save the offset of a message number
**
**	This fills in an ongoing buffer in memory that has the offsets
**	of the message numbers we have seen.  The buffer is expanded
**	if necessary.
*/

static EP_STAT
nexdle_save_offset(nexdle_t *nexdle,
	    long msgno,
	    off_t off)
{
    if (msgno >= nexdle->cachesize)
    {
	// have to allocate more space
	while (msgno >= nexdle->cachesize)	// hack, but failure rare
	    nexdle->cachesize += nexdle->cachesize / 2;
	nexdle->offcache = realloc(nexdle->offcache,
			    nexdle->cachesize * sizeof nexdle->offcache[0]);
	if (nexdle->offcache == NULL)
	{
	    // oops, no memory
	    return ep_stat_from_errno(errno);
	}
    }

    if (off != nexdle->offcache[msgno] && nexdle->offcache[msgno] != 0)
    {
	// somehow offset has moved.  not a happy camper.
	fprintf(stderr,
		"nexdle_save_offset: offset for message %ld has moved from %lld to %lld\n",
		msgno,
		(long long) nexdle->offcache[msgno],
		(long long) off);
	return EP_STAT_ERROR;	    // TODO: should be more specific
    }

    nexdle->offcache[msgno] = off;
    ep_dbg_printf("Caching offset for msgno %ld = %lld\n", msgno, off);
    return EP_STAT_OK;
}


/************************  PUBLIC  ************************/

/*
**  GDP_NEXUS_CREATE --- create a new nexus
**
**	Right now we cheat.  No network nexus need apply.
*/

EP_STAT
gdp_nexus_create(nexus_t *nexus_type,
		nexdle_t **pnexdle)
{
    nexdle_t *nexdle = NULL;
    EP_STAT estat = EP_STAT_OK;
    int fd = -1;
    FILE *fp;
    char pbuf[NEXUS_PATH_LEN];

    // allocate the memory to hold the nexdle
    //	    Note that ep_mem_* always returns, hence no test here
    estat = nexdle_new(&nexdle);
    EP_STAT_CHECK(estat, goto fail0);

    // allocate a name
    create_nexus_name(nexdle->nname);

    // create a file node representing the nexus
    // XXX: this is where we start to seriously cheat
    estat = get_nexus_path(nexdle, pbuf, sizeof pbuf);
    EP_STAT_CHECK(estat, goto fail1);
    if ((fd = open(pbuf, O_WRONLY|O_CREAT|O_APPEND|O_EXCL, 0644)) < 0 ||
	(flock(fd, LOCK_EX) < 0))
    {
	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "gdp_nexus_create: cannot create %s: %s",
		pbuf, strerror(errno));
	goto fail1;
    }
    nexdle->fp = fp = fdopen(fd, "a");
    if (nexdle->fp == NULL)
    {
	estat = ep_stat_from_errno(errno);
	gdp_log(estat, "gdp_nexus_create: cannot fdopen %s: %s",
		pbuf, strerror(errno));
	goto fail1;
    }

    // write the header metadata
    fprintf(fp, "%s%d\n", NEXUS_MAGIC, NEXUS_VERSION);
    
    // TODO: will probably need creation date or some such later

    // success!
    fflush(fp);
    nexdle_save_offset(nexdle, 0, ftell(fp));
    flock(fd, LOCK_UN);
    *pnexdle = nexdle;
    if (ep_dbg_test(&Dbg, 10))
    {
	nexus_pname_t pname;

	gdp_nexus_printable_name(nexdle->nname, pname);
	ep_dbg_printf("Created nexdle %s\n", pname);
    }
    return estat;

fail1:
    if (fd >= 0)
	    close(fd);
    ep_mem_free(nexdle);
fail0:
    if (ep_dbg_test(&Dbg, 8))
    {
	char ebuf[100];

	ep_dbg_printf("Could not create nexdle: %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}


/*
**  GDP_NEXUS_OPEN --- open a nexus for reading or further appending
**
**	XXX: Should really specify whether we want to start reading:
**	(a) At the beginning of the log (easy).  This includes random
**	    access.
**	(b) Anything new that comes into the log after it is opened.
**	    To do this we need to read the existing log to find the end.
*/

EP_STAT
gdp_nexus_open(nname_t nname,
	    gdpiomode_t mode,
	    nexdle_t **pnexdle)
{
    EP_STAT estat = EP_STAT_OK;
    nexdle_t *nexdle = NULL;
    FILE *fp;
    char pbuf[NEXUS_PATH_LEN];
    char nbuf[NEXUS_MAX_LINE];

    // XXX determine if name exists and is a nexus
    estat = nexdle_new(&nexdle);
    EP_STAT_CHECK(estat, goto fail1);
    memcpy(nexdle->nname, nname, sizeof nexdle->nname);
    nexdle->iomode = mode;
    estat = get_nexus_path(nexdle, pbuf, sizeof pbuf);
    EP_STAT_CHECK(estat, goto fail0);
    if ((fp = fopen(pbuf, "r")) == NULL ||
        (flock(fileno(fp), LOCK_SH) < 0))
    {
	estat = ep_stat_from_errno(errno);
	goto fail0;
    }
    nexdle->fp = fp;

    // check nexus header
    if (fgets(nbuf, sizeof nbuf, fp) == NULL)
    {
	if (ferror(fp))
	{
	    estat = ep_stat_from_errno(errno);
	}
	else
	{
	    // must be end of file (bad format nexus)
	    estat = ep_stat_from_errno(EINVAL);	    // XXX: should be more specific
	}
	flock(fileno(fp), LOCK_UN);
	goto fail0;
    }
    if (strncmp(nbuf, NEXUS_MAGIC, sizeof NEXUS_MAGIC - 1) != 0)
    {
	estat = ep_stat_from_errno(EINVAL);
	gdp_log(estat, "gdp_nexus_open: invalid format %*s",
		(int) (sizeof NEXUS_MAGIC - 1), nbuf);
	flock(fileno(fp), LOCK_UN);
	goto fail0;
    }
    nexdle->ver = atoi(&nbuf[sizeof NEXUS_MAGIC - 1]);

    // skip possible metadata
    while (fgets(nbuf, sizeof nbuf, fp) != NULL)
    {
	if (nbuf[0] == '\n')
	    break;
    }
    ungetc('\n', fp);
    flock(fileno(fp), LOCK_UN);

    // we should now be pointed at the first message (or end of file)
    nexdle_save_offset(nexdle, 0, ftell(fp));

    // if we are appending, we have to find the last message number
    if (mode == GDP_MODE_AO)
    {
	// populate the offset cache
	nexmsg_t msg;
	char mbuf[1000];

	gdp_nexus_read(nexdle, INT32_MAX, &msg, mbuf, sizeof mbuf);
	nexdle->msgno = msg.msgno;
    }

    // success!
    *pnexdle = nexdle;
    if (ep_dbg_test(&Dbg, 10))
    {
	nexus_pname_t pname;

	gdp_nexus_printable_name(nexdle->nname, pname);
	ep_dbg_printf("Opened nexus %s\n", pname);
    }
    return estat;

fail1:
    estat = ep_stat_from_errno(errno);

fail0:
    if (nexdle == NULL)
	ep_mem_free(nexdle);

    {
	char ebuf[100];

	//XXX should log
	fprintf(stderr, "gdp_nexus_open: Couldn't open nexus: %s\n",
		ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    if (ep_dbg_test(&Dbg, 10))
    {
	nexus_pname_t pname;
	char ebuf[100];

	gdp_nexus_printable_name(nname, pname);
	ep_dbg_printf("Couldn't open nexus %s: %s\n",
		pname, ep_stat_tostr(estat, ebuf, sizeof ebuf));
    }
    return estat;
}

/*
**  GDP_NEXUS_CLOSE --- close an open nexus
*/

EP_STAT
gdp_nexus_close(nexdle_t *nexdle)
{
    EP_STAT estat = EP_STAT_OK;

    EP_ASSERT_POINTER_VALID(nexdle);
    if (nexdle->fp == NULL)
    {
	estat = EP_STAT_ERROR;
	gdp_log(estat, "gdp_nexus_close: null fp");
    }
    else
    {
	fclose(nexdle->fp);
    }
    ep_mem_free(nexdle);

    return estat;
}

/*
**  GDP_NEXUS_APPEND --- append a message to a writable nexus
*/

EP_STAT
gdp_nexus_append(nexdle_t *nexdle,
	    nexmsg_t *msg)
{
    EP_STAT estat = EP_STAT_OK;

    EP_ASSERT_POINTER_VALID(nexdle);

    // XXX: check that nexdle is writable

    // make sure no write conflicts
    flock(fileno(nexdle->fp), LOCK_EX);
    fprintf(stdout, "Back from locking for message %ld, cachesize %ld\n",
	    msg->msgno, nexdle->cachesize);

    // XXX: should see what the actual new msgno and offset are.
    // XXX: without this you can't have multiple simultaneous writers

    // see what the offset is and cache that
    estat = nexdle_save_offset(nexdle, ++nexdle->msgno, ftell(nexdle->fp));
    EP_STAT_CHECK(estat, goto fail0);
    fprintf(stdout, "Saved offset for msgno %ld\n", nexdle->msgno);

    // write the message out
    // TODO: check for errors
    fprintf(nexdle->fp, "%s%ld %zu\n", MSG_MAGIC, nexdle->msgno, msg->len);
    fwrite(msg->data, msg->len, 1, nexdle->fp);
    msg->msgno = nexdle->msgno;
    fflush(nexdle->fp);
    //XXX

    // ok, done!
fail0:
    flock(fileno(nexdle->fp), LOCK_UN);

    return estat;
}

/*
**  GET_NEXUS_REC --- get the next nexus record
**
**	This implements a sliding window on the input to search for
**	the message magic string.  When the magic is found it
**	unmarshalls the message.
*/

static EP_STAT
get_nexus_rec(FILE *fp,
	    nexmsg_t *msg,
	    char *buf,
	    size_t buflen)
{
    long offset;

    /*
    **  Part 1: find the message magic string
    */

    {
	int tx;			// index into target (pattern)
	int bx;			// index into magicbuf
	int bstart;		// buffer offset for start of test string
	int bend;		// buffer offset for end of test string
	int c = 0;		// current test character
	char magicbuf[16];    // must be larger than target pattern

	// scan the stream for the message marker
	//	tx counts through the target pattern, bx through the buffer
	tx = bx = bstart = bend = 0;
	offset = ftell(fp);
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
		return ep_stat_from_errno(errno);
	    else
		return EP_STAT_END_OF_FILE;
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

	// read in the rest of the record header
	i = fscanf(fp, "%ld %ld\n", &msgno, &msglen);
	if (i < 2)
	{
	    // apparently we can't read the metadata
	    return GDP_STAT_MSGFMT;
	}
	memset(msg, 0, sizeof *msg);
	msg->msgno = msgno;
	msg->len = msglen;
	msg->data = buf;
	msg->offset = offset;

	// be sure to not overflow buffer
	if (msglen > buflen)
	    msg->len = buflen;
	sz = fread(buf, 1, msg->len, fp);
	if (sz < msg->len)
	{
	    // short read
	    msg->len = sz;
	    return GDP_STAT_SHORTMSG;
	}

	// if we have more data to read do so now to keep stream in sync
	for (; sz < msglen; sz++)
	    (void) fgetc(fp);
    }

    return EP_STAT_OK;
}


/*
**  GDP_NEXUS_READ --- read a message from a nexus
**
**	In theory we should be positioned at the head of the next message.
**	But that might not be the correct message.  If we have specified a
**	message number there are two cases:
**	    (a) We've already seen the message -> use the cached offset.
**		Of course we check to see if we got the record we expected.
**	    (b) We haven't (yet) seen the message in this nexdle -> starting
**		from the last message we have seen, read up to this message,
**		assuming it exists.
**	If we have said "read the last message" XXX TODO
*/

EP_STAT
gdp_nexus_read(nexdle_t *nexdle,
	    long msgno,
	    nexmsg_t *msg,
	    char *buf,
	    size_t buflen)
{
    EP_STAT estat = EP_STAT_OK;
    long seekmsgno;

    EP_ASSERT_POINTER_VALID(nexdle);

    // if we don't have a specific msgno, read the next one since before
    if (msgno < 0)
	msgno = ++nexdle->msgno;

    // don't want to read a partial record
    flock(fileno(nexdle->fp), LOCK_SH);

    // if we have to go backwards we'll need to start from zero
    seekmsgno = msgno;
    if (msgno < nexdle->msgno)
	seekmsgno = 0;

    // if we already have a seek offset, use it
    if (seekmsgno < nexdle->maxmsgno && nexdle->offcache[seekmsgno] > 0)
    {
	ep_dbg_printf("DBG Using cached offset %lld for msgno %ld\n",
		nexdle->offcache[msgno], msgno);
	if (fseek(nexdle->fp, nexdle->offcache[seekmsgno], SEEK_SET) < 0)
	{
	    estat = ep_stat_from_errno(errno);
	    goto fail0;
	}
    }

    // we may have to skip ahead, hence the do loop
    ep_dbg_printf("Looking for msgno %ld\n", msgno);
    do
    {
	if (EP_UT_BITSET(NEXDLE_ASYNC, nexdle->flags))
	{
	    fd_set inset;
	    struct timeval zerotime = { 0, 0 };
	    int i;

	    // make sure we have something to read
	    FD_ZERO(&inset);
	    FD_SET(fileno(nexdle->fp), &inset);
	    i = select(fileno(nexdle->fp), &inset, NULL, NULL, &zerotime);
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

	estat = get_nexus_rec(nexdle->fp, msg, buf, buflen);
	EP_STAT_CHECK(estat, goto fail0);
	nexdle_save_offset(nexdle, msg->msgno, msg->offset);
	if (msg->msgno > msgno)
	{
	    // oh bugger, we've already read too far
	    ep_app_error("gdp_nexus_read: read too far (cur=%ld, target=%ld)",
		    msg->msgno, msgno);
	    estat = GDP_STAT_READ_OVERFLOW;
	}
	if (msg->msgno < msgno)
	    printf("Skipping msg->msgno %ld\n", msg->msgno);
    } while (msg->msgno < msgno);

    // ok, done!
fail0:
    flock(fileno(nexdle->fp), LOCK_UN);

    return estat;
}


/*
**  GDP_NEXUS_SUBSCRIBE --- subscribe to a nexus
*/

struct gdp_cb_arg
{
    struct event	    *event;	// event triggering this callback
    nexdle_t		    *nexdle;	// nexdle triggering this callback
    gdp_nexus_sub_cbfunc_t  cbfunc;	// function to call
    void		    *cbarg;	// argument to pass to cbfunc
    void		    *buf;	// space to put the message
    size_t		    bufsiz;	// size of buf
};


static void
nexus_sub_event_cb(evutil_socket_t fd,
	short what,
	void *cbarg)
{
    nexmsg_t msg;
    EP_STAT estat;
    struct gdp_cb_arg *cba = cbarg;

    // get the next message from this nexdle
    estat = gdp_nexus_read(cba->nexdle, NEXDLE_NEXT_MSG, &msg,
	    cba->buf, cba->bufsiz);
    if (EP_STAT_ISOK(estat))
	(*cba->cbfunc)(cba->nexdle, &msg, cba->cbarg);
    //XXX;
}


EP_STAT
gdp_nexus_subscribe(nexdle_t *nexdle,
	gdp_nexus_sub_cbfunc_t cbfunc,
	long offset,
	void *buf,
	size_t bufsiz,
	void *cbarg)
{
    EP_STAT estat = EP_STAT_OK;
    struct gdp_cb_arg *cba;
    struct timeval timeout = { 0, 100000 };	// 100 msec

    EP_ASSERT_POINTER_VALID(nexdle);
    EP_ASSERT_POINTER_VALID(cbfunc);

    cba = ep_mem_zalloc(sizeof *cba);
    cba->nexdle = nexdle;
    cba->cbfunc = cbfunc;
    cba->buf = buf;
    cba->bufsiz = bufsiz;
    cba->cbarg = cbarg;
    cba->event = event_new(GdpEventBase, fileno(nexdle->fp),
	    EV_READ|EV_PERSIST, &nexus_sub_event_cb, cba);
    event_add(cba->event, &timeout);
    //XXX;
    return estat;
}

#if 0
EP_STAT
gdp_nexus_unsubscribe(nexdle_t *nexdle,
	void (*cbfunc)(nexdle_t *, void *),
	void *arg)
{
    EP_ASSERT_POINTER_VALID(nexdle);
    EP_ASSERT_POINTER_VALID(cbfunc);

    XXX;
}
#endif



/*
**  GDP_NEXUS_GETNAME --- get the name of a nexus
*/

const nname_t *
gdp_nexus_getname(const nexdle_t *nexdle)
{
    return &nexdle->nname;
}

/*
**  GDP_NEXUS_MSG_PRINT --- print a message (for debugging)
*/

void
gdp_nexus_msg_print(const nexmsg_t *msg,
	    FILE *fp)
{
    int i;

    fprintf(fp, "Nexus Message %ld, len %zu", msg->msgno, msg->len);
    if (msg->ts_valid)
	fprintf(fp, ", timestamp %ld.%09ld-%ld.%09ld\n",
		msg->ts.earliest.tv_sec,
		msg->ts.earliest.tv_nsec,
		msg->ts.latest.tv_sec,
		msg->ts.latest.tv_nsec);
    else
	fprintf(fp, ", no timestamp\n");
    i = msg->len;
    fprintf(fp, "  <%.*s>\n", i, msg->data);
}
