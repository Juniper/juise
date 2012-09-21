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
#include "websocket.h"
#include "request.h"
#include "session.h"

void
mx_websocket_handle_request (mx_sock_websocket_t *mswp, mx_buffer_t *mbp)
{
    char *cp = mbp->mb_data + mbp->mb_start;
    char *ep = mbp->mb_data + mbp->mb_start + mbp->mb_len;
    char *tag, *base;

    if (*cp != '#') {
	mx_log("S%u parse request fails (%c)", mswp->msw_base.ms_id, *cp);
	goto fatal;
    }

    base = ++cp;
    for (; cp < ep; cp++) {
	if (*cp == '\n')
	    break;
    }
    if (cp >= ep)
	goto fatal;

    /*
     * Mark the header data as consumed.  The rest of the payload
     * may be used during the request.
     */
    int delta = cp - (mbp->mb_data + mbp->mb_start) + 1; /* Skip over '\n' */
    mbp->mb_start = delta; 
    mbp->mb_len -= delta;

    /* Make a copy of the data we can scribble on */
    tag = alloca(cp - base);
    memcpy(tag, base, cp - base);
    tag[cp - base] = '\0';
    ep = tag + (cp - base);
    cp = tag;

    for (; cp < ep && !isspace((int) *cp); cp++)
	continue;

    if (*cp != '\0')
	*cp++ = '\0';

    mx_log("S%u websocket request tag '%s', rest '%s'",
	   mswp->msw_base.ms_id, tag, cp);

    const char *attrs[MAX_XML_ATTR];
    if (xml_parse_attributes(attrs, MAX_XML_ATTR, cp)) {
	mx_log("S%u websocket request ('%s') with broken attributes ('%s')",
	       mswp->msw_base.ms_id, tag, cp);
	goto fatal;
    }

    if (streq(tag, "rpc")) {
	/* Build an request instance */
	mx_request_t *mrp = mx_request_create(mswp, mbp, tag, attrs);

	if (mx_request_start_rpc(mswp, mbp, mrp))
	    mbp->mb_start = mbp->mb_len = 0;

#if 0
    } else if (streq(tag, "command")) {
    } else if (streq(tag, "login")) {
    } else if (streq(tag, "password")) {
    } else if (streq(tag, "unknown-host")) {
#endif
    } else {
	mx_log("S%u websocket: unknown request '%s'",
	       mswp->msw_base.ms_id, tag);
	mbp->mb_start = mbp->mb_len = 0;
    }

    return;

 fatal:
    mx_log("S%u fatal error parsing request", mswp->msw_base.ms_id);
    mswp->msw_base.ms_state = MSS_FAILED;
    mbp->mb_start = mbp->mb_len = 0;
}

static void
mx_websocket_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_websocket_t *mswp = mx_sock(msp, MST_WEBSOCKET);
    mx_buffer_t *mbp = mswp->msw_rbufp;

    mx_log("%*s%srb %lu/%lu", indent, "", prefix, mbp->mb_start, mbp->mb_len);
}

static int
mx_websocket_prep (MX_TYPE_PREP_ARGS)
{
    mx_sock_websocket_t *mswp = mx_sock(msp, MST_WEBSOCKET);
    mx_buffer_t *mbp = mswp->msw_rbufp;

    /*
     * If we have buffered data, we need to poll for output on
     * the channels' session.
     */
    if (mbp->mb_len) {
	DBG_POLL("S%u websocket has data", msp->ms_id);
	return FALSE;

    } else {
	pollp->fd = msp->ms_sock;
	pollp->events = POLLIN;
	DBG_POLL("S%u blocking pollin for fd %d", msp->ms_id, pollp->fd);
    }

    return TRUE;
}

static int
mx_websocket_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_websocket_t *mswp = mx_sock(msp, MST_WEBSOCKET);
    mx_buffer_t *mbp = mswp->msw_rbufp;
    int len;

    if (pollp && pollp->revents & POLLIN) {

	if (mbp->mb_len == 0) {
	    mbp->mb_start = mbp->mb_len = 0;

	    len = recv(msp->ms_sock, mbp->mb_data, mbp->mb_size, 0);
	    if (len < 0) {
		if (errno == EWOULDBLOCK)
		    return FALSE;

		mx_log("S%u: read error: %s", msp->ms_id, strerror(errno));
		msp->ms_state = MSS_FAILED;
		return TRUE;
	    }

	    if (len == 0) {
		mx_log("S%u: disconnect (%s)", msp->ms_id, mx_sock_sin(msp));
		return TRUE;
	    }

	    mbp->mb_len = len;
	    slaxMemDump("wsread: ", mbp->mb_data, mbp->mb_len, ">", 0);

	    mx_websocket_handle_request(mswp, mbp);
	}
    }

    return FALSE;
}

static mx_sock_t *
mx_websocket_spawn (MX_TYPE_SPAWN_ARGS)
{
    mx_sock_websocket_t *mswp = malloc(sizeof(*mswp));

    if (mswp == NULL)
	return NULL;

    bzero(mswp, sizeof(*mswp));
    mswp->msw_base.ms_id = ++mx_sock_id;
    mswp->msw_base.ms_type = MST_WEBSOCKET;
    mswp->msw_base.ms_sock = sock;
    mswp->msw_base.ms_sin = *sin;

    mswp->msw_rbufp = mx_buffer_create(0);
    TAILQ_INIT(&mswp->msw_requests);

    return &mswp->msw_base;
}

static int
mx_websocket_check_hostkey (MX_TYPE_CHECK_HOSTKEY_ARGS)
{
    mx_log("S%u checking hostkey", client->ms_id);

    return FALSE;
}

static int
mx_websocket_write (MX_TYPE_WRITE_ARGS)
{
    mx_log("S%u write rb %lu/%lu", msp->ms_id, mbp->mb_start, mbp->mb_len);

    int len = write(msp->ms_sock, mbp->mb_data + mbp->mb_start, mbp->mb_len);
    if (len > 0) {
	if ((unsigned) len == mbp->mb_len)
	    mbp->mb_start = mbp->mb_len = 0;
	else {
	    mbp->mb_start += len;
	    mbp->mb_len -= len;
	}
    }

    return FALSE;
}

void
mx_websocket_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_WEBSOCKET,
    mti_name: "websocket",
    mti_print: mx_websocket_print,
    mti_prep: mx_websocket_prep,
    mti_poller: mx_websocket_poller,
    mti_spawn: mx_websocket_spawn,
    mti_check_hostkey: mx_websocket_check_hostkey,
    mti_write: mx_websocket_write,
#if 0
    mti_set_channel: mx_websocket_set_channel,
#endif
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

