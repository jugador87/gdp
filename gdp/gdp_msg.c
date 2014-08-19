/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**  Message management
**
**		Messages contain the header and data information for a single
**		message.
*/

#include <ep/ep.h>
#include <ep/ep_thr.h>

#include <gdp/gdp.h>
#include "gdp_priv.h"

static LIST_HEAD(msg_head, gdp_msg)	MsgFreeList;
static EP_THR_MUTEX					MsgFreeListMutex EP_THR_MUTEX_INITIALIZER;

gdp_msg_t *
gdp_msg_new(void)
{
	gdp_msg_t *msg;

	// get a message off the free list, if any
	ep_thr_mutex_lock(&MsgFreeListMutex);
	if ((msg = LIST_FIRST(&MsgFreeList)) != NULL)
	{
		LIST_REMOVE(msg, list);
	}
	ep_thr_mutex_unlock(&MsgFreeListMutex);

	if (msg == NULL)
	{
		// nothing on the free list; allocate anew
		msg = ep_mem_zalloc(sizeof *msg);
		ep_thr_mutex_init(&msg->mutex, EP_THR_MUTEX_RECURSIVE);
	}

	// initialize metadata
	msg->recno = GDP_PKT_NO_RECNO;
	msg->ts.stamp.tv_sec = TT_NOTIME;
	if (msg->dbuf == NULL)
	{
		msg->dbuf = gdp_buf_new();
		gdp_buf_setlock(msg->dbuf, &msg->mutex);
//		gdp_buf_setlock(msg->dbuf, NULL);
	}
	return msg;
}


void
gdp_msg_free(gdp_msg_t *msg)
{
	ep_thr_mutex_lock(&MsgFreeListMutex);
	LIST_INSERT_HEAD(&MsgFreeList, msg, list);
	ep_thr_mutex_unlock(&MsgFreeListMutex);
}
