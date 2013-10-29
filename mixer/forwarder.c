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
#include "forwarder.h"
#include "session.h"
#include "channel.h"

static void
mx_forwarder_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    mx_buffer_t *mbp = msfp->msf_rbufp;

    mx_log("%*s%ssession S%u, channel C%u to %s, rb %lu/%lu",
	   indent, "", prefix,
	   msfp->msf_session->mss_base.ms_id, msfp->msf_channel->mc_id,
	   msfp->msf_session->mss_target,
	   mbp->mb_start, mbp->mb_len);
}

static mx_sock_t *
mx_forwarder_spawn (MX_TYPE_SPAWN_ARGS)
{
    mx_sock_forwarder_t *msfp = malloc(sizeof(*msfp));
    if (msfp == NULL)
	return NULL;

    bzero(msfp, sizeof(*msfp));
    msfp->msf_base.ms_id = ++mx_sock_id;
    msfp->msf_base.ms_type = MST_FORWARDER;
    msfp->msf_base.ms_sock = sock;
    msfp->msf_base.ms_sun = *sun;

    msfp->msf_rbufp = mx_buffer_create(0);

    /* XXX msl_request needs to move into the forwarder */
    mslp->msl_request->mr_client = &msfp->msf_base;

    mx_sock_session_t *mssp = mx_session(mslp->msl_request);
    if (mssp == NULL) {
	mx_log("%s could not open session", mx_sock_title(&msfp->msf_base));
	/* XXX fail nicely */
	return NULL;
    }

    mx_request_t *mrp = mslp->msl_request;
    mx_channel_t *mcp;
    mcp = mx_channel_direct_tcpip(mssp, &msfp->msf_base,
				  mrp->mr_desthost, mrp->mr_destport);
    if (mcp == NULL) {
	mx_log("%s could not open channel", mx_sock_title(&msfp->msf_base));
	/* XXX close msfp */
	return NULL;
    }

    return &msfp->msf_base;
}

static void
mx_forwarder_close (mx_sock_t *msp)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);

    mx_buffer_free(msfp->msf_rbufp);
}

static int
mx_forwarder_prep (MX_TYPE_PREP_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    mx_buffer_t *mbp = msfp->msf_rbufp;

    /*
     * If we have buffered data, we need to poll for output on
     * the channels' session.
     */
    if (mbp->mb_len) {
	pollp->fd = mx_channel_sock(msfp->msf_channel);
	pollp->events = POLLOUT;
	DBG_POLL("%s blocking pollout for fd %d",
                 mx_sock_title(msp), pollp->fd);

    } else if (mx_channel_has_buffered(msfp->msf_channel)) {
	pollp->fd = mx_channel_sock(msfp->msf_channel);
	pollp->events = POLLOUT;
	DBG_POLL("%s blocking pollout for fd %d (channel)",
		 mx_sock_title(msp), pollp->fd);

    } else {
	pollp->fd = msp->ms_sock;
	pollp->events = POLLIN;
	DBG_POLL("%s blocking pollin for fd %d",
                 mx_sock_title(msp), pollp->fd);
    }

    return TRUE;
}

static int
mx_forwarder_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    mx_buffer_t *mbp = msfp->msf_rbufp;
    int len;

    if (pollp && pollp->revents & POLLIN) {

	if (mbp->mb_len == 0) {
	    mbp->mb_start = mbp->mb_len = 0;

	    len = recv(msp->ms_sock, mbp->mb_data, mbp->mb_size, 0);
	    if (len < 0) {
		if (errno != EWOULDBLOCK) {
		    mx_log("%s: read error: %s",
                           mx_sock_title(msp), strerror(errno));
		    msp->ms_state = MSS_FAILED;
		    return TRUE;
		}

	    } else if (len == 0) {
		mx_log("%s: disconnect (%s)",
                       mx_sock_title(msp), mx_sock_name(msp));
		return TRUE;
	    } else {
		mbp->mb_len = len;
	    }
	}
    }

    if (mbp->mb_len)
	mx_channel_write_buffer(msfp->msf_channel, mbp);

    return FALSE;
}

static int
mx_forwarder_write (MX_TYPE_WRITE_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    int rc;
    unsigned len = mbp->mb_len;

    while (mbp->mb_start < len) {
	rc = send(msfp->msf_base.ms_sock,
		  mbp->mb_data + mbp->mb_start, len, 0);
	if (rc <= 0) {
	    if (errno != EWOULDBLOCK) {
		mx_log("%s: write error: %s",
		       mx_sock_title(&msfp->msf_base), strerror(errno));
		msfp->msf_base.ms_state = MSS_FAILED;
	    }

	    return TRUE;
	}

	mbp->mb_start += rc;
	mbp->mb_len -= rc;
    }

    mbp->mb_start = mbp->mb_len = 0; /* Reset */
    return FALSE;
}

static void
mx_forwarder_set_channel (MX_TYPE_SET_CHANNEL_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);

    msfp->msf_session = mssp;
    msfp->msf_channel = mcp;
}

static int
mx_forwarder_is_buf (MX_TYPE_IS_BUF_ARGS)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    return (msfp->msf_rbufp->mb_len != 0);
}

void
mx_forwarder_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_FORWARDER,
    mti_name: "forwarder",
    mti_letter: "F",
    mti_print: mx_forwarder_print,
    mti_prep: mx_forwarder_prep,
    mti_poller: mx_forwarder_poller,
    mti_spawn: mx_forwarder_spawn,
    mti_write: mx_forwarder_write,
    mti_set_channel: mx_forwarder_set_channel,
    mti_close: mx_forwarder_close,
    mti_is_buf: mx_forwarder_is_buf,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
