/*
 * $Id$
 *
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include "local.h"
#include "request.h"
#include "session.h"
#include "channel.h"
#include "netconf.h"

static unsigned mx_request_id; /* Monotonically increasing ID number */
static mx_request_list_t mx_request_list; /* List of outstanding requests */

char mx_netconf_tag_open_rpc[] = "<rpc format=\"html\">";
unsigned mx_netconf_tag_open_rpc_len = sizeof(mx_netconf_tag_open_rpc) - 1;
char mx_netconf_tag_close_rpc[] = "</rpc>";
unsigned  mx_netconf_tag_close_rpc_len = sizeof(mx_netconf_tag_close_rpc) - 1;

mx_request_t *
mx_request_create (mx_sock_websocket_t *mswp, mx_buffer_t *mbp, int len,
		   mx_muxid_t muxid, const char *tag, const char **attrs)
{
    mx_request_t *mrp;
    char *cp;

    mrp = calloc(1, sizeof(*mrp));
    if (mrp == NULL)
	return NULL;

    mrp->mr_id = ++mx_request_id;

    mrp->mr_state = MSS_NORMAL;
    mrp->mr_muxid = muxid;
    mrp->mr_name = strdup(tag);
    mrp->mr_client = &mswp->msw_base;
    mrp->mr_target = nstrdup(xml_get_attribute(attrs, "target"));
    mrp->mr_user = nstrdup(xml_get_attribute(attrs, "user"));
    mrp->mr_password = nstrdup(xml_get_attribute(attrs, "password"));
    mrp->mr_passphrase = nstrdup(xml_get_attribute(attrs, "passphrase"));
    mrp->mr_hostkey = nstrdup(xml_get_attribute(attrs, "hostkey"));
    mrp->mr_rpc = mx_buffer_copy(mbp, len);

    if (mrp->mr_user == NULL && mrp->mr_target) {
	cp = index(mrp->mr_target, '@');
	if (cp) {
	    mrp->mr_user = mrp->mr_target;
	    *cp++ = '\0';
	    mrp->mr_target = strdup(cp);
	}
    }

    TAILQ_INSERT_HEAD(&mx_request_list, mrp, mr_link);

    mx_log("R%u request %s %lu from S%u, target %s, user %s",
	   mrp->mr_id, mrp->mr_name, mrp->mr_muxid,
	   mswp->msw_base.ms_id, mrp->mr_target, mrp->mr_user);

    return mrp;

#if 0
 fatal:
    free(mrp);
    return NULL;
#endif
}

/*
 * We need to add framing to the front and end of the RPC, but we really
 * want to put all the content into one mx_buffer_t.  So if it fits, that's
 * what we do.
 */
static mx_buffer_t *
mx_netconf_insert_framing (mx_buffer_t *mbp)
{
    const unsigned close_len
	= mx_netconf_tag_close_rpc_len + mx_netconf_marker_len;
    mx_buffer_t *newp = mbp;
    int fresh = FALSE, blen, len;

    if (mbp->mb_next != NULL)
	fresh = TRUE;
    else if (mbp->mb_start >= mx_netconf_tag_open_rpc_len) {
	fresh =  (mbp->mb_size - (mbp->mb_start + mbp->mb_len) < close_len);
    } else {
	fresh =  (mbp->mb_size - (mbp->mb_start + mbp->mb_len)
		  < close_len + mx_netconf_tag_open_rpc_len);
    }

    if (fresh) {
	blen = mx_netconf_tag_open_rpc_len;
	blen += mbp->mb_len;
	blen += mx_netconf_tag_close_rpc_len;
	blen += mx_netconf_marker_len;

	newp = mx_buffer_create(blen);
	if (newp == NULL)
	    return NULL;

	len = mx_netconf_tag_open_rpc_len;
	memcpy(newp->mb_data, mx_netconf_tag_open_rpc, len);
	blen = len;

	len = mbp->mb_len;
	memcpy(newp->mb_data + blen, mbp->mb_data + mbp->mb_start, len);
	blen += len;

	len = mx_netconf_tag_close_rpc_len;
	memcpy(newp->mb_data + blen, mx_netconf_tag_close_rpc, len);
	blen += len;

	len = mx_netconf_marker_len;
	memcpy(newp->mb_data + blen, mx_netconf_marker, len);
	blen += len;

	newp->mb_len = blen;

	return newp;
    }

    /* Find or make room for the open <rpc> tag */
    if (mbp->mb_start < mx_netconf_tag_open_rpc_len) {
	memmove(mbp->mb_data + mbp->mb_start,
		mbp->mb_data + mx_netconf_tag_open_rpc_len,
		mbp->mb_len);
	mbp->mb_start = 0;
    } else {
	mbp->mb_start -= mx_netconf_tag_open_rpc_len;
	mbp->mb_len += mx_netconf_tag_open_rpc_len;
    }

    /* Prepend the open <rpc> tag */
    memcpy(mbp->mb_data + mbp->mb_start, mx_netconf_tag_open_rpc,
	   mx_netconf_tag_open_rpc_len);

    /* Append the close </rpc> tag and the ]]>]]> framing marker */
    memcpy(mbp->mb_data + mbp->mb_start + mbp->mb_len,
	   mx_netconf_tag_close_rpc,
	   mx_netconf_tag_close_rpc_len);
    mbp->mb_len += mx_netconf_tag_close_rpc_len;

    memcpy(mbp->mb_data + mbp->mb_start + mbp->mb_len, mx_netconf_marker,
	   mx_netconf_marker_len);
    mbp->mb_len += mx_netconf_marker_len;

    return newp;
}

static int
mx_request_rpc_send (mx_sock_t *msp, mx_buffer_t *mbp,
		     mx_request_t *mrp, mx_channel_t *mcp)
{
    mx_log("R%u S%u/C%u sending rpc %.*s",
	   mrp->mr_id, msp->ms_id, mcp->mc_id,
	   (int) mbp->mb_len, mbp->mb_data + mbp->mb_start);

    ssize_t len;

    mx_buffer_t *newp = mx_netconf_insert_framing(mbp);

    mcp->mc_request = mrp;
    mcp->mc_state = MSS_RPC_INITIAL;
    len = mx_channel_write_buffer(mcp, newp);
    mx_log("R%u S%u/C%u send rpc, len %d",
	   mrp->mr_id, msp->ms_id, mcp->mc_id, (int) len);

    if (newp != mbp)
	mx_buffer_free(newp);

    return FALSE;
}

int
mx_request_start_rpc (mx_sock_websocket_t *mswp, mx_request_t *mrp)
{
    mx_log("R%u %s %lu on S%u, target '%s'",
	   mrp->mr_id, mrp->mr_name, mrp->mr_muxid,
	   mswp->msw_base.ms_id, mrp->mr_target);

    mx_sock_session_t *mssp = mx_session(mrp);
    if (mssp == NULL) {
	/* XXX send failure message */
	return TRUE;
    }

    if (mssp->mss_base.ms_state != MSS_ESTABLISHED)
	return TRUE;

    mx_channel_t *mcp = mx_channel_netconf(mssp, &mswp->msw_base, TRUE);
    if (mcp) {
	mx_log("C%u running R%u '%s' target '%s'",
	       mcp->mc_id, mrp->mr_id, mrp->mr_name, mrp->mr_target);
	mx_request_rpc_send(&mswp->msw_base, mrp->mr_rpc, mrp, mcp);
    }

    return TRUE;
}

mx_request_t *
mx_request_find (mx_muxid_t muxid)
{
    mx_request_t *mrp;

    TAILQ_FOREACH(mrp, &mx_request_list, mr_link) {
	if (mrp->mr_muxid == muxid)
	    return mrp;
    }

    return NULL;
}

void
mx_request_set_state (mx_request_t *mrp, unsigned state)
{
    mx_log("R%u state change: %d -> %d (S%u)",
	   mrp->mr_id, mrp->mr_state, state,
	   mrp->mr_session ? mrp->mr_session->mss_base.ms_id : 0);

    mrp->mr_state = state;
    if (mrp->mr_client)
	mrp->mr_client->ms_state = state;
    if (mrp->mr_session)
	mrp->mr_session->mss_base.ms_state = state;
}

void
mx_request_print (mx_request_t *mrp, int indent, const char *prefix)
{
    mx_log("%*s%sR%u: muxid %lu, name %s, target %s, user %s, dest %s:%u",
	   indent, "", prefix, mrp->mr_id, mrp->mr_muxid,
	   mrp->mr_name ?: "", mrp->mr_target ?: "", mrp->mr_user ?: "",
	   mrp->mr_desthost ?: "", mrp->mr_destport);
    mx_log("%*s%sclient S%u, session S%u C%u", indent + INDENT, "", prefix,
	   mrp->mr_client ? mrp->mr_client->ms_id : 0,
	   mrp->mr_session ? mrp->mr_session->mss_base.ms_id : 0,
	   mrp->mr_channel ? mrp->mr_channel->mc_id : 0);
}	

void
mx_request_print_all (int indent, const char *prefix)
{
    mx_request_t *mrp;

    mx_log("%*s%sList of all outstanding requests:%s",
	   indent, "", prefix ?: "",
	   TAILQ_EMPTY(&mx_request_list) ? " none" : "");

    TAILQ_FOREACH(mrp, &mx_request_list, mr_link) {
	mx_request_print(mrp, indent, prefix);
    }
}

void
mx_request_free (mx_request_t *mrp)
{
    TAILQ_REMOVE(&mx_request_list, mrp, mr_link);

    if (mrp->mr_name) free(mrp->mr_name);
    if (mrp->mr_target) free(mrp->mr_target);
    if (mrp->mr_user) free(mrp->mr_user);
    if (mrp->mr_password) free(mrp->mr_password);
    if (mrp->mr_passphrase) free(mrp->mr_passphrase);
    if (mrp->mr_hostkey) free(mrp->mr_hostkey);
    if (mrp->mr_hostkey) free(mrp->mr_hostkey);
    if (mrp->mr_rpc) mx_buffer_free(mrp->mr_rpc);

    free(mrp);
}

void
mx_request_release (mx_request_t *mrp)
{
    /* Reset the state of the client and session, as needed */
    unsigned state = (mrp->mr_state >= MSS_ESTABLISHED)
	? MSS_ESTABLISHED : MSS_NORMAL;

    mx_request_set_state(mrp, state);
    mx_request_free(mrp);
}

void
mx_request_release_session (mx_sock_session_t *session)
{
    mx_request_t *mrp;

    TAILQ_FOREACH(mrp, &mx_request_list, mr_link) {
	if (mrp->mr_session == session) {
	    mx_log("R%u session released S%u, C%u",
		   mrp->mr_id, mrp->mr_session->mss_base.ms_id,
		   mrp->mr_channel ? mrp->mr_channel->mc_id : 0);
	    mrp->mr_state = MSS_FAILED;
	    mrp->mr_session = NULL;
	    mrp->mr_channel = NULL;
	    mrp->mr_client = NULL;
	}
    }
}

void
mx_request_release_client (mx_sock_t *client)
{
    mx_request_t *mrp;

    TAILQ_FOREACH(mrp, &mx_request_list, mr_link) {
	if (mrp->mr_client == client) {
	    mx_log("R%u client released S%u, C%u",
		   mrp->mr_id, mrp->mr_client->ms_id,
		   mrp->mr_channel ? mrp->mr_channel->mc_id : 0);
	    if (mrp->mr_state == MSS_ESTABLISHED)
		mx_channel_release(mrp->mr_channel);

	    mrp->mr_state = MSS_FAILED;
	    mrp->mr_session = NULL;
	    mrp->mr_channel = NULL;
	    mrp->mr_client = NULL;
	}
    }
}

void
mx_request_restart_rpc (mx_request_t *mrp)
{
    mx_channel_t *mcp;

    /* If we're not already recorded as established, do it now */
    mx_request_set_state(mrp, MSS_ESTABLISHED);

    mcp = mx_channel_netconf(mrp->mr_session, mrp->mr_client, TRUE);
    if (mcp == NULL)
	return;

    mx_log("C%u running R%u '%s' target '%s'",
	   mcp->mc_id, mrp->mr_id, mrp->mr_name, mrp->mr_target);

    /*
     * When the RPC stalled (for hostkey or password), we recorded
     * the RPC contents in mr_rpc.
     */
    mx_buffer_t *mbp = mrp->mr_rpc;
    mx_request_rpc_send(mrp->mr_client, mbp, mrp, mcp);
}

void
mx_request_error (mx_request_t *mrp, const char *fmt, ...)
{
    va_list vap;
    int bufsiz = BUFSIZ, rc;
    char buf[bufsiz], *bp = buf;

    va_start(vap, fmt);

    rc = vsnprintf(bp, bufsiz, fmt, vap);
    if (rc > bufsiz) {
	bufsiz = rc;
	bp = alloca(bufsiz);
	rc = vsnprintf(bp, bufsiz, fmt, vap);
    }

    mx_sock_t *client = mrp->mr_client;
    if (client && mx_mti(client)->mti_error)
	mx_mti(client)->mti_error(client, mrp, bp);

    va_end(vap);
}

void
mx_request_check_health (void)
{
    mx_request_t *mrp, *next;

    TAILQ_FOREACH_SAFE(mrp, &mx_request_list, mr_link, next) {
	if (mrp->mr_state == MSS_ERROR) /* Errors aren't fatal */
	    mrp->mr_state = MSS_RPC_COMPLETE;
	if (mrp->mr_state == MSS_FAILED || mrp->mr_state == MSS_RPC_COMPLETE)
	    mx_request_release(mrp);
    }
}

void
mx_request_init (void)
{
    TAILQ_INIT(&mx_request_list);
}
