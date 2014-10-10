/* vim: set ai sw=8 sts=8 ts=8 : */

#include <ep.h>
#include <ep_syslog.h>

#include <strings.h>

struct code
{
	char	*c_name;
	int	c_val;
};

static struct code	PriNames[] =
{
	{ "alert",	LOG_ALERT	},
	{ "crit",	LOG_CRIT	},
	{ "debug",	LOG_DEBUG	},
	{ "emerg",	LOG_EMERG	},
	{ "err",	LOG_ERR		},
	{ "error",	LOG_ERR		},		/* DEPRECATED */
	{ "info",	LOG_INFO	},
	{ "notice",	LOG_NOTICE	},
	{ "panic",	LOG_EMERG	},		/* DEPRECATED */
	{ "warn",	LOG_WARNING	},		/* DEPRECATED */
	{ "warning",	LOG_WARNING	},
	{ NULL,		-1		}
};

struct code     FacNames[] =
{
	{ "auth",	LOG_AUTH	},
#ifdef LOG_AUTHPRIV
	{ "authpriv",	LOG_AUTHPRIV	},
#endif
	{ "cron",	LOG_CRON	},
	{ "daemon",	LOG_DAEMON	},
#ifdef LOG_FTP
	{ "ftp",	LOG_FTP		},
#endif
	{ "kern",	LOG_KERN	},
	{ "lpr",	LOG_LPR		},
	{ "mail",	LOG_MAIL	},
	{ "news",	LOG_NEWS	},
	{ "security",	LOG_AUTH	},		/* DEPRECATED */
	{ "syslog",	LOG_SYSLOG	},
	{ "user",	LOG_USER	},
	{ "uucp",	LOG_UUCP	},
	{ "local0",	LOG_LOCAL0	},
	{ "local1",	LOG_LOCAL1	},
	{ "local2",	LOG_LOCAL2	},
	{ "local3",	LOG_LOCAL3	},
	{ "local4",	LOG_LOCAL4	},
	{ "local5",	LOG_LOCAL5	},
	{ "local6",	LOG_LOCAL6	},
	{ "local7",	LOG_LOCAL7	},
	{ NULL,		-1		}
};


/*
**  EP_SYSLOG_PRI_FROM_NAME --- translate string name to log priority
*/

int
ep_syslog_pri_from_name(const char *name)
{
	struct code *c;

	for (c = PriNames; c->c_name != NULL; c++)
	{
		if (strcasecmp(c->c_name, name) == 0)
			return c->c_val;
	}
	return -1;
}



char *
ep_syslog_name_from_pri(int pri)
{
	struct code *c;

	for (c = PriNames; c->c_name != NULL; c++)
	{
		if (c->c_val == pri)
			return c->c_name;
	}
	return "unknown";
}



int
ep_syslog_fac_from_name(const char *name)
{
	struct code *c;

	for (c = FacNames; c->c_name != NULL; c++)
	{
		if (strcasecmp(c->c_name, name) == 0)
			return c->c_val;
	}
	return -1;
}



char *
ep_syslog_name_from_fac(int fac)
{
	struct code *c;

	for (c = FacNames; c->c_name != NULL; c++)
	{
		if (c->c_val == fac)
			return c->c_name;
	}
	return "unknown";
}
