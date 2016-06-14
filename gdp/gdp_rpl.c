#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>

#include <ep/ep_dbg.h>

static EP_DBG Dbg = EP_DBG_INIT("gdplogd.rplservice",
       						"GDP GCL Replication Service");
#define RPL_DIR "/etc/ep_adm_params/gdprpl"

void
_rpl_init(gdp_gcl_t *gcl) //This function is tentatively prepared to set a set of inital replica servers until a reconfiguration interface is developed.
{
    gdp_pname_t pname;

    //Log server names are now hard-coded but these should be read from files.
    char *externalname1 = "gdp-logd-172.10.1.2";
    char *externalname2 = "gdp-logd-172.10.1.3";

    gdp_rplsvr_t *svr1 = ep_mem_zalloc(sizeof (*svr1));
    gdp_rplsvr_t *svr2 = ep_mem_zalloc(sizeof (*svr2));

    gdp_parse_name(externalname1, svr1->svrname);
    gdp_parse_name(externalname2, svr2->svrname);

    gdp_printable_name(svr1->svrname, svr1->svrpname);
    gdp_printable_name(svr2->svrname, svr2->svrpname);

    ep_thr_mutex_lock(&gcl->mutex);
    LIST_INSERT_HEAD(&gcl->rplsvr, svr1, svrlist);
    LIST_INSERT_HEAD(&gcl->rplsvr, svr2, svrlist);
    ep_thr_mutex_unlock(&gcl->mutex);
}

/* _rpl_fwd_append --- forward an APPEND request to other servers
**        
**        It replies success when a forwarding message is sent out successfully.
*/

EP_STAT
_rpl_fwd_append(gdp_req_t *req)
{
    EP_STAT estat = EP_STAT_OK;
    gdp_rplsvr_t *currentsvr;
    gdp_rplsvr_t *nextsvr;

    req->ackcnt = 1; //The number of acks before ack reply towards a writer. Should be under gdp_gcl in the future.
    req->flags |= GDP_REQ_PERSIST; //Keep this req till the number of acked server is satisfied. Timeout is needed in the future to avoid memory leak.
    uint32_t reqflags = 0; 
    char ebuf[200];
    void (*cbfunc)(gdp_event_t *) = NULL;
    cbfunc = _rpl_resp_cb;


    ep_thr_mutex_lock(&req->gcl->mutex);
    currentsvr = LIST_FIRST(&req->gcl->rplsvr);
    ep_thr_mutex_unlock(&req->gcl->mutex);

    if (currentsvr == NULL) //Initialize replication service if not.
    {
        _rpl_init(req->gcl);

        //Again check it.
        ep_thr_mutex_lock(&req->gcl->mutex);
        currentsvr = LIST_FIRST(&req->gcl->rplsvr);
        ep_thr_mutex_unlock(&req->gcl->mutex);
        if (currentsvr == NULL)
            return GDP_STAT_NAK_SVCUNAVAIL; //??
    }

    for (; currentsvr != NULL; currentsvr = nextsvr)
    {
        nextsvr = LIST_NEXT(currentsvr, svrlist);
        if (memcmp(currentsvr->svrname, _GdpMyRoutingName, sizeof _GdpMyRoutingName) != 0)
        {
            gdp_rplcb_t *cbarg = ep_mem_zalloc(sizeof (*cbarg));
            cbarg->req = req;
            memcpy(cbarg->svrname, currentsvr->svrname, sizeof cbarg->svrname);
            LIST_INSERT_HEAD(&req->rplcb, cbarg, cblist);
            estat = _gdp_gcl_fwd_append(req->gcl, req->pdu->datum, currentsvr->svrname, cbfunc, cbarg, NULL, reqflags);
            if (!EP_STAT_ISOK(estat))
            {

                ep_app_fatal("Cannot rpl_fwd_append:\n\t%s",
                        ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
            else {
                req->fwdcnt++;
                ep_app_info("do_fwd_append (%d): %s",
		                req->fwdcnt, ep_stat_tostr(estat, ebuf, sizeof ebuf));
            }
        }
    }

    return estat;
}

/* _rpl_resp_cb --- start processing an incoming response for the forwarded APPEND requests.
**        
**        
*/


void
_rpl_resp_cb(gdp_event_t *gev)
{
    gdp_event_print(gev, stdout, 3);

    if (gdp_event_gettype(gev) == GDP_EVENT_CREATED)
    {
        gdp_gcl_t *gcl = gdp_event_getgcl(gev);
        if (gcl != NULL) {
            _rpl_resp_proc(gev);
        }
    }
    else if (gdp_event_gettype(gev) == GDP_EVENT_EOS)
    {
        //It should show an error message.
    }

    gdp_event_free(gev);
}

void
_rpl_resp_proc(gdp_event_t *gev)
{
    gdp_rplcb_t *cbarg = gdp_event_getudata(gev);

    //Add a log server into acksvr//--->
    _rpl_add_ackedsvr(cbarg->req, cbarg->svrname);
    //<---

    //Check whether timing is to reply an ack to a client or not//--->
    uint16_t ackcnt = _rpl_get_number_ackedsvr(cbarg->req);

    if ( (ackcnt >= cbarg->req->ackcnt) && (cbarg->req->acksnt != 1) )
    {
        _rpl_reply_ack(cbarg->req, EP_STAT_OK);
        cbarg->req->acksnt = 1;
    }

    if (ackcnt == cbarg->req->fwdcnt)
    {
        _rpl_ackedsvr_freeall(&cbarg->req->acksvr);
        _gdp_req_free(&cbarg->req);
    }
    //<---

    _rpl_rplcbarg_free(cbarg);
}

void
_rpl_reply_ack(gdp_req_t *req, EP_STAT estat)
{
    req->pdu->cmd = GDP_ACK_SUCCESS;
    req->stat = _gdp_pdu_out(req->pdu, req->chan, NULL);
}

void
_rpl_add_ackedsvr(gdp_req_t *req, const gdp_name_t svrname)
{
    //stop-gap implementation. This should check whether incoming ack is corresponding to a sent pdu.

    if (!gdp_name_is_valid(svrname))
    {
        printf("_rpl_add_ackedsvr. Error. unexpected svrname is chosen.\n");
        exit(1);
    }
    gdp_rplsvr_t *svr;
    svr = ep_mem_zalloc(sizeof (*svr));

    memcpy(svr->svrname, svrname, sizeof svr->svrname);
    gdp_printable_name(svr->svrname, svr->svrpname);

    //Need to check duplication here//
    _gdp_req_lock(req);
    LIST_INSERT_HEAD(&req->acksvr, svr, acklist);
}

uint16_t
_rpl_get_number_ackedsvr(const gdp_req_t *req)
{
    //Need to check whether log servers in the list is corresponding to any log server in sending candidates.
    uint16_t count = 0;
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(&req->acksvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, acklist);
        count++;
    }
    return count;
}

void
_rpl_rplsvr_freeall(struct rplsvr_head *rplsvr)
{
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(rplsvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, svrlist);
        LIST_REMOVE(current, svrlist);
        current = NULL;
    }
}

void
_rpl_ackedsvr_freeall(struct rplsvr_head *acksvr)
{
    gdp_rplsvr_t *current;
    gdp_rplsvr_t *next;
    for (current = LIST_FIRST(acksvr); current != NULL; current = next)
    {
        next = LIST_NEXT(current, acklist);
        LIST_REMOVE(current, acklist);
        current = NULL;
    }
}

void
_rpl_rplcbarg_free(gdp_rplcb_t *rplcbarg)
{
    LIST_REMOVE(rplcbarg, cblist);
    rplcbarg = NULL;
}
