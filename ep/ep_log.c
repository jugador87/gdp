/***********************************************************************
**	Copyright (c) 2008-2014, Eric P. Allman.  All rights reserved.
**	$Id: ep_log.c 286 2014-04-29 18:15:22Z eric $
***********************************************************************/

#include <ep.h>
#include <ep_rpool.h>
#include <ep_xlate.h>
#include <ep_stat.h>
#include <ep_log.h>

#include <unistd.h>
#include <time.h>
#include <sys/time.h>

EP_SRC_ID("@(#)$Id: ep_log.c 286 2014-04-29 18:15:22Z eric $");


/**************************  BEGIN PRIVATE  **************************/

/*
**  EP_LOG_GET_DEFAULT -- return the default log
**
**	Is there any reason for this to be private?
**
**	Parameters:
**		None.
**
**	Returns:
**		The current default log.
*/

static EP_LOG	*EpDefaultLog;		// default log if none specified

static EP_LOG *
ep_log_get_default(void)
{
	if (EpDefaultLog == EP_NULL)
		EpDefaultLog = ep_log_open(&EpLogTypeSyslog,
					"{System Default Log}",
					EpLibProgname,
					EP_LOG_NORMAL,
					"user",
					EP_NULL);
	return EpDefaultLog;
}

/***************************  END PRIVATE  ***************************/


/*********************  BASIC LOG IMPLEMENTATION  ********************/

/*
**  EP_LOG_OPEN -- open a new log reference
**
**	Logs should be pretty generic.  Apps can have several open at
**	one time: for example, having an error log and an accounting log
**	open at the same time is perfectly reasonable.
**
**	Every log has a type.  For example, EpLogTypeFile logs to an
**	on-disk file, and EpLogTypeSyslog logs to the system log
**	database.
**
**
**	Parameters:
**		ltype -- the type of this log
**		lname -- the name of this log (for some types)
**		app -- a string to use for identifying log entries that
**			corresponds to the application name.
**		flags -- modify the function
**		priv -- other log-type specific data
**		pstat -- pointer to status variable for the open
**
**	Returns:
**		a pointer to the log object
**		EP_NULL on failure, see *pstat
*/

EP_LOG *
ep_log_open(const EP_LOG_TYPE *ltype,
	const char *lname,
	const char *app,
	uint32_t flags,
	const void *priv,
	EP_STAT *pstat)
{
	EP_STAT stat = EP_STAT_OK;
	EP_LOG *log;
	EP_RPOOL *rp;

	EP_ASSERT_POINTER_VALID(ltype);
	EP_ASSERT(app != EP_NULL);

	rp = ep_rpool_new(app, 0);
	log = EP_OBJ_CREATE(EP_LOG, &EpClassLog, app, rp, 0);
	log->ltype = ltype;
	log->name = lname;
	log->app = app;
	log->priv = priv;
	log->flags = flags;

	stat = ltype->open(log);
	if (EP_STAT_SEV_ISFAIL(stat))
	{
		EP_OBJ_DROPREF(log);
		log = EP_NULL;
		stat = ep_stat_post(EP_STAT_LOG_CANTOPEN,
				"Cannot open log: type %1 name %2",
				ltype->obj_name,
				lname,
				EP_NULL);
	}

	if (EpDefaultLog == EP_NULL &&
	    !EP_UT_BITSET(EP_LOG_NODEFAULT, flags) &&
	    EP_STAT_SEV_ISOK(stat))
		EpDefaultLog = log;

	if (pstat != EP_NULL)
		*pstat = stat;
	return log;
}


/*
**  EP_LOG_CLOSE -- close a log
**
**	Parameters:
**		log -- the log to close.
**
**	Returns:
**		None.
*/

void
ep_log_close(
	EP_LOG *log)
{
	EP_ASSERT_POINTER_VALID(log);

	if (log->obj_refcount-- > 0)
		return;

	if (log->ltype->close != EP_NULL)
		log->ltype->close(log);
}


/*
**  EP_LOG_PROPL, EP_LOG_PROPV -- log a property list/vector
**
**	Property lists are pairs of {tag, value} strings.
**
**	Parameters:
**		log -- a pointer to the particular log to use.
**			If NULL, the system default log is used.
**		sev -- the severity of the message.  This is the same
**			set used by status codes.
**		msgid -- a string that can be used to identify the
**			message.  It should generally be unique to this
**			application, as identified by the tag presented
**			to ep_log_open.
**		av, ... -- the list of alternative tags and values,
**			terminated by two EP_NULL pointers.
**			Tags may be EP_NULL to indicate positional
**			values.
**
**	Returns:
**		none.
*/

void
ep_log_propl(
	EP_LOG *log,
	int sev,
	const char *msgid,
	...)
{
	va_list av;

	va_start(av, msgid);
	ep_log_propv(log, sev, msgid, av);
	va_end(av);
}


void
ep_log_propv(
	EP_LOG *log,
	int sev,
	const char *msgid,
	va_list av)
{
	if (log == EP_NULL)
		log = ep_log_get_default();
	EP_ASSERT_POINTER_VALID(log);
	EP_ASSERT(log->ltype != EP_NULL);
	EP_ASSERT(msgid != EP_NULL);

	log->ltype->log(log, sev, msgid, av);
}


/*
**  EP_LOG_SET_DEFAULT -- set the log to use if none specified
**
**	This routine takes a reference to the log (so it should be
**	dereferenced using ep_log_close unless it is going to be
**	reused).  The old default log is closed.
**
**	Parameters:
**		log -- the log to use as the default.
**
**	Returns:
**		none.
*/

void
ep_log_set_default(
	EP_LOG *log)
{
	EP_ASSERT_POINTER_VALID(log);
	EP_ASSERT(log->ltype != EP_NULL);

	ep_log_close(EpDefaultLog);

	EpDefaultLog = log;
	log->obj_refcount++;
}



/***********************  LOG-TO-STREAM LOG TYPE  **********************/

/**************************  BEGIN PRIVATE  **************************/

/*
**  STREAM_LOG_OPEN -- open stacked log
**
**	The private data for this log type is the underlying log
**	stream.
*/

static EP_STAT
stream_log_open(EP_LOG *log)
{
	((EP_STREAM *) log->priv)->obj_refcount++;
	return EP_STAT_OK;
}


/*
**  STREAM_LOG_CLOSE -- close stacked log
**
**	Also closes the underlying file
*/

static EP_STAT
stream_log_close(EP_LOG *log)
{
	ep_st_close((EP_STREAM *) log->priv);

	// any error will have already been posted
	return EP_STAT_OK;
}


/*
**  STREAM_LOG_LOG -- actually do the logging
*/

static EP_STAT
stream_log_log(
	EP_LOG *log,
	int sev,
	const char *msgid,
	va_list av)
{
	EP_STREAM *sp = (EP_STREAM *) log->priv;
	const char *app;
	struct tm tm;
	struct timeval now;

	app = log->app;
	if (app == NULL)
		app = "";

	(void) gettimeofday(&now, NULL);
	(void) localtime_r((time_t *) &now.tv_sec, &tm);

	ep_st_fprintf(sp, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s[%d] ",
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, now.tv_usec / 1000,
		log->app, getpid());
	ep_log_tostr(sp, sev, msgid, av);
	ep_st_puts(sp, ep_st_eolstring(sp));

	return EP_STAT_OK;
}

/***************************  END PRIVATE  ***************************/

const EP_LOG_TYPE	EpLogTypeStream =
{
	EP_OBJ_OBJECT_INITIALIZER(EpClassLogType, "log:stream")
	stream_log_open,		// openfunc
	stream_log_close,		// closefunc
	stream_log_log,			// logfunc
};

/**************************  UTILITY ROUTINES  *************************/

/*
**  EP_LOG_TOSTR -- convert a log input to string format
**
**	This can be used when logging to a text file or writing to a
**	text-based format such as UNIX syslog.
**
**	Output Format:
**		severity:msgid tag=value; tag=value; ...
**		Characters listed in eplib.log.forbidchars (default:
**			"=;") , non-printable characters,
**			and the "+" character are encoded using +XX
**			syntax.
**
**	Parameters:
**		logsp -- the stream pointer for the log (or temp buffer).
**		sev -- the severity of the message
**		msgid -- the message name
**		av -- the property (name, value) list
**
**	Returns:
**		None (but you should call ep_st_stat(logsp) to see if
**		the data was properly output).
*/

void
ep_log_tostr(
	EP_STREAM *logsp,
	int sev,
	const char *msgid,
	va_list av)
{
	int argno = 0;
	char const *sevname;
	bool firstparam = true;
	int xlatemode = EP_XLATE_PLUS | EP_XLATE_NPRINT;
	static const char *forbidchars = EP_NULL;

	if (forbidchars == NULL)
		forbidchars = ep_adm_getstrparam("eplib.log.forbidchars", "=;");

	sevname = ep_stat_sev_tostr(sev);
	(void) ep_xlate_out(sevname,
			strlen(sevname),
			logsp,
			forbidchars,
			xlatemode);
	(void) ep_xlate_out(":",
			1,
			logsp,
			forbidchars,
			xlatemode);
	(void) ep_xlate_out(msgid,
			strlen(msgid),
			logsp,
			forbidchars,
			xlatemode);

	// scan the arguments
	for (;;)
	{
		const char *apn;
		const char *apv;

		argno++;
		apn = va_arg(av, const char *);
		if ((apv = va_arg(av, const char *)) == EP_NULL)
			break;

		if (!firstparam)
			ep_st_putc(logsp, ';');
		ep_st_putc(logsp, ' ');
		firstparam = false;

		if (apn != EP_NULL)
		{
			(void) ep_xlate_out(apn,
					strlen(apn),
					logsp,
					forbidchars,
					xlatemode);

			ep_st_putc(logsp, '=');
		}

		(void) ep_xlate_out(apv,
				strlen(apv),
				logsp,
				forbidchars,
				xlatemode);
	}
}

/***********************************************************************/

#if 0

void
ep_log_text(
	EP_STAT stat,
	const char *defmsgid,
	const char *text)
{
	EP_STAT rstat = EP_STAT_OK;
	EP_STREAM *vbuf;		// result buffer
	char *msgid = EP_NULL;
	char *msgtag = EP_NULL;
	int pass;
	char kbuf[1024];		// should be dynamic
	char msgidbuf[1024];		// probably dynamic

	vbuf = ep_st_open(&EpStType_DMem,
			"ep_log_text:vbuf",
			EP_ST_MODE_WR,
			&rstat,
			0,
			EP_NULL);
	if (EP_STAT_SEV_ISFAIL(rstat))
	{
		rstat = ep_stat_post(rstat,
			"ep_log_text:no-dmem Cannot open dynamic memory",
			EP_NULL);
		ep_assert_abort("ep_log_text: cannot create dynamic sorytring");
	}

	/*
	**  Look up the message id based on the status code.  If
	**  not found, use the default that has been passed in.
	*/

	for (pass = 0; pass < 2; pass++)
	{
		switch (pass)
		{
		  case 0:
			ep_st_sprintf(kbuf, sizeof kbuf,
				"STAT:%d:%d",
				EP_STAT_MODULE(stat),
				EP_STAT_DETAIL(stat));
			break;

		  case 1:
			ep_st_sprintf(kbuf, sizeof kbuf,
				"STAT:%d",
				EP_STAT_MODULE(stat));
			break;

		  default:
			ep_assert_abort("ep_log_text: illegal pass");
		}

		if (!EP_STAT_SEV_ISFAIL(ep_mcat_get(kbuf, EP_NULL, vbuf)) &&
		    !EP_STAT_SEV_ISFAIL(ep_st_getknob(kbuf, EP_ST_KNOB_GETBUF, &msgid)))
			break;
	}

	if (msgid == EP_NULL)
	{
		msgid = defmsgid;
	}
	else
	{
		// save the message id if necessary
		strlcpy(msgidbuf, msgid, sizeof msgidbuf);
		// XXX should check for overflow here
		msgid = msgidbuf;
	}

	/*
	**  Given the message-id, find the tag associated with it.
	*/

	ep_st_sprintf(kbuf, sizeof kbuf,
		"MSGID:TAG:%s:%s",
		"",				// XXX language code
		msgid);

	(void) ep_st_setknob(vbuf, EP_ST_KNOB_REWIND, EP_NULL);
	if (EP_STAT_SEV_ISFAIL(ep_mcat_get(kbuf, EP_NULL, vbuf)) ||
	    EP_STAT_SEV_ISFAIL(ep_st_getknob(kbuf, EP_ST_KNOB_GETBUF, &msgtag)))
	{
		// no tag: use the id as the tag
		msgtag = msgid;
	}

	/*
	**  Done with vbuf.
	*/

	ep_st_close(vbuf);

	/*
	**  Now do the actual logging.
	**
	**	XXX ugh.  Given that ep_log_propl is going to do a
	**	XXX redundant lookup of stat to get the message id, can
	**	XXX we somehow short circuit?
	*/

	ep_log_propl(stat,
		msgid,
		"tag",		msgtag,
		"text",		text,
		EP_NULL);
}


void
ep_log_stat(
	EP_STAT stat,
	const char *defmsg,
	...)
{
	va_list av;

	va_start(av, defmsg);
	ep_log_propv(stat, defmsg, av);
	va_end(av);
}


void
ep_log_statv(
	EP_STAT stat,
	const char *defmsg,
	va_list av)
{
	ep_log_propv(DefLog, sev, ms
}

#endif /* 0 */
