/* vim: set ai sw=4 sts=4 ts=4: */

#include <ep/ep_stat.h>
#include <gdp/gdp_stat.h>

/*
 **  Status codes to string mappings
 **
 **  TODO    Verify that these strings are accurate.
 */

static struct ep_stat_to_string Stats[] =
{
// module name
        { GDP_STAT_NEW(OK, 0), "Swarm-GDP", },

        // individual codes
        { GDP_STAT_MSGFMT, "message format error", },
        { GDP_STAT_SHORTMSG, "incomplete message", },
        { GDP_STAT_READ_OVERFLOW, "read overflow", },
        { GDP_STAT_NOT_IMPLEMENTED, "not implemented", },
        { GDP_STAT_PKT_WRITE_FAIL, "write failure", },
        { GDP_STAT_PKT_READ_FAIL, "read failure", },
        { GDP_STAT_PKT_VERSION_MISMATCH, "protocol version mismatch", },
        { GDP_STAT_PKT_NO_SEQ, "no sequence number", },
        { GDP_STAT_KEEP_READING, "more input needed", },
        { GDP_STAT_NOT_OPEN, "GCL is not open", },
        { GDP_STAT_UNKNOWN_RID, "request id unknown", },
        { GDP_STAT_INTERNAL_ERROR, "GDP internal error", },
        { GDP_STAT_BAD_IOMODE, "GDP bad I/O mode", },
        { GDP_STAT_GCL_NAME_INVALID, "invalid GCL name", },

        // end of list sentinel
        { EP_STAT_OK, NULL }, };

void
gdp_stat_init(void)
{
	ep_stat_reg_strings(Stats);
}
