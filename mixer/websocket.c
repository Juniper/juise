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

/*
 * Headers: The websocket interface between lighttpd and mixer does
 * not contain a framing protocol, which is needed for our
 * communications.  So we roll our own.  We overload framing with
 * some additional neccessities:
 *
 *   - data length (total, including header)
 *   - muxer ID (number indicating source/destination channel)
 *   - operation (verb describing the content)
 *
 * We need these values encoded in ascii, since various forms
 * of websocket do not support binary and dealing with binary
 * from javascript is no one's idea of fun.  So we force field
 * widths on the header fields.
 *
 * We use '#' as the leader, "." as a field separator, and '\n' as the
 * trailer, but we do allow a set of 'name="value"' attributes to be
 * encoded between the end of the basic header and the newline.  A
 * version number is also included, which currently has the value "01".
 *
 * The end result is:
 *
 * #<version>.<length>.<operation>.<muxer-id>.<attributes>\n
 *
 * For example:
 *
 * #01.00000140.rpc     .00000001.host="router" user="test"\n
 */

#include "local.h"
#include "websocket.h"
#include "request.h"
#include "session.h"
#include "channel.h"

typedef struct mx_header_s {
    char mh_pound;		/* Leader: pound sign */
    char mh_version[2];		/* MX_HEADER_VERSION */
    char mh_dot1;		/* Separator: period */
    char mh_len[8];		/* Total data length (including header) */
    char mh_dot2;		/* Separator: period */
    char mh_operation[8];	/* Operation name */
    char mh_dot3;		/* Separator: period */
    char mh_muxid[8];		/* Muxer ID */
    char mh_dot4;		/* Separator: period */
    char mh_trailer[];
} mx_header_t;

#define MX_HEADER_VERSION_0 '0'
#define MX_HEADER_VERSION_1 '1'

static unsigned long
strntoul (const char *buf, size_t bufsiz)
{
    unsigned long val = 0;

    for ( ; bufsiz > 0; buf++, bufsiz--)
	if (isdigit((int) *buf))
	    val = val * 10 + (*buf - '0');

    return val;
}

void
mx_websocket_handle_request (mx_sock_websocket_t *mswp, mx_buffer_t *mbp)
{
    char *cp = mbp->mb_data + mbp->mb_start;
    char *ep = mbp->mb_data + mbp->mb_start + mbp->mb_len;
    char *trailer;
    mx_header_t *mhp = (mx_header_t *) cp;

    if (mhp->mh_pound != '#' || mhp->mh_version[0] != MX_HEADER_VERSION_0
	|| mhp->mh_version[1] != MX_HEADER_VERSION_1
	|| mhp->mh_dot1 != '.' || mhp->mh_dot2 != '.'
	|| mhp->mh_dot3 != '.' || mhp->mh_dot4 != '.') {
	mx_log("S%u parse request fails (%c)", mswp->msw_base.ms_id, *cp);
	goto fatal;
    }

    unsigned long len = strntoul(mhp->mh_len, sizeof(mhp->mh_len));
    mx_muxid_t muxid = strntoul(mhp->mh_muxid, sizeof(mhp->mh_muxid));
    char *operation = mhp->mh_operation;
    for (cp = operation + sizeof(mhp->mh_operation) - 1; cp >= operation; cp--)
	if (*cp != ' ')
	    break;
    *++cp = '\0';
    mx_log("S%u incoming request '%s', muxid %lu, len %lu", 
	   mswp->msw_base.ms_id, operation, muxid, len);

    if (len < mbp->mb_len) {
	/* XXX Handle short read */
    }

    trailer = cp = mhp->mh_trailer;
    for (; cp < ep; cp++) {
	if (*cp == '\n')
	    break;
    }
    if (cp >= ep)
	goto fatal;
    *cp++ = '\0';		/* Skip over '\n' */

    /*
     * Mark the header data as consumed.  The rest of the payload
     * may be used during the request.
     */
    int delta = cp - mbp->mb_data;
    mbp->mb_start = delta; 
    mbp->mb_len -= delta;

    mx_log("S%u websocket request op '%s', rest '%s'",
	   mswp->msw_base.ms_id, operation, trailer);

    const char *attrs[MAX_XML_ATTR];
    if (*trailer == '\0') {
	attrs[0] = NULL;
    } else if (xml_parse_attributes(attrs, MAX_XML_ATTR, trailer)) {
	mx_log("S%u websocket request ('%s') with broken attributes ('%s')",
	       mswp->msw_base.ms_id, operation, trailer);
	goto fatal;
    }

    if (streq(operation, MX_OP_RPC)) {
	/* Build an request instance */
	mx_request_t *mrp = mx_request_create(mswp, mbp, muxid,
					      operation, attrs);

	if (mrp == NULL)
	    goto fatal;

	mswp->msw_rbufp = NULL;	/* Buffer is now owned by the request */

	mx_request_start_rpc(mswp, mrp);

    } else if (streq(operation, MX_OP_HOSTKEY)) {
	mx_request_t *mrp = mx_request_find(muxid);
	if (mrp) {
	    if (mrp->mr_state != MSS_HOSTKEY) {
		mx_log("R%u in wrong state", mrp->mr_id);
		mx_buffer_reset(mbp);
		return;
	    }

	    if (mx_session_approve_hostkey(mrp->mr_session, mrp,
					   mbp->mb_data + mbp->mb_start,
					   mbp->mb_len))
		goto fatal;

	    if (mx_session_check_auth(mrp->mr_session, mrp)) {
		mx_log("R%u waiting for check auth", mrp->mr_id);
		mrp->mr_state = MSS_PASSWORD;
		return;
	    }

	    mx_request_restart_rpc(mrp);

	} else {
	    mx_log("S%u muxid %lu not found (ignored)",
		   mswp->msw_base.ms_id, muxid);
	}
#if 0
    } else if (streq(operation, "command")) {
    } else if (streq(operation, "password")) {
    } else if (streq(operation, "unknown-host")) {
#endif
    } else {
	mx_log("S%u websocket: unknown request '%s'",
	       mswp->msw_base.ms_id, operation);
	mx_buffer_reset(mbp);
	mbp->mb_start = mbp->mb_len = 0;
    }

    return;

 fatal:
    mx_log("S%u fatal error parsing request", mswp->msw_base.ms_id);
    mswp->msw_base.ms_state = MSS_FAILED;
    mx_buffer_reset(mbp);
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
    if (mbp && mbp->mb_len) {
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

    if (mbp == NULL)
	mswp->msw_rbufp = mbp = mx_buffer_create(0);

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

    return &mswp->msw_base;
}

static void
mx_websocket_header_format_int (char *buf, int blen, unsigned value)
{
    char *cp = buf + blen - 1;

    for ( ; cp >= buf; cp--) {
	*cp = (value % 10) + '0';
	value /= 10;
    }
}

static void
mx_websocket_header_format_string (char *buf, int blen, const char *value)
{
    int vlen = strlen(value);

    if (vlen > blen)
	vlen = blen;
    memcpy(buf, value, vlen);
    if (vlen < blen)
	memset(buf + vlen, ' ', blen - vlen);
}

static void
mx_websocket_header_build (mx_header_t *mhp, int len, const char *operation,
			   mx_muxid_t muxid)
{
    mhp->mh_pound = '#';
    mhp->mh_version[0] = MX_HEADER_VERSION_0;
    mhp->mh_version[1] = MX_HEADER_VERSION_1;
    mhp->mh_dot1 = mhp->mh_dot2 = mhp->mh_dot3 = mhp->mh_dot4 = '.';
    mx_websocket_header_format_int(mhp->mh_len, sizeof(mhp->mh_len), len);
    mx_websocket_header_format_string(mhp->mh_operation,
				      sizeof(mhp->mh_operation), operation);
    mx_websocket_header_format_int(mhp->mh_muxid,
				   sizeof(mhp->mh_muxid), muxid);
}

static int
mx_websocket_check_hostkey (MX_TYPE_CHECK_HOSTKEY_ARGS)
{
    mx_log("S%u checking hostkey: [%s]", client->ms_id, info);

    int ilen = strlen(info);
    int len = sizeof(mx_header_t) + 1 + ilen;
    char buf[len + 1];

    mx_muxid_t muxid = mrp->mr_muxid;
    mx_header_t *mhp = (mx_header_t *) buf;
    mx_websocket_header_build(mhp, len, MX_OP_HOSTKEY, muxid);
    buf[sizeof(*mhp)] = '\n';
    memcpy(buf + sizeof(*mhp) + 1, info, ilen + 1);

    int rc = write(client->ms_sock, buf, len);
    if (rc > 0) {
	if (rc != len)
	    mx_log("S%u complete very short write (%d/%d)",
		   mssp->mss_base.ms_id, rc, len);
    }

    return TRUE;
}

static int
mx_websocket_write (MX_TYPE_WRITE_ARGS)
{
    mx_log("S%u write rb %lu/%lu", msp->ms_id, mbp->mb_start, mbp->mb_len);
    int len = mbp->mb_len;
    char *buf = mbp->mb_data + mbp->mb_start;
    char *mbuf = NULL;
    int header_len = sizeof(mx_header_t) + 1;

    if (mcp && (mcp->mc_state == MSS_RPC_INITIAL
		|| mcp->mc_state == MSS_RPC_IDLE)) {
	len += header_len;

	if (mbp->mb_start < (unsigned) header_len) {
	    mbuf = malloc(len);
	    if (mbuf == NULL)
		return TRUE;
	    memcpy(mbuf + header_len, buf, mbp->mb_len);
	    buf = mbuf;
	} else {
	    mbp->mb_start -= header_len;
	    mbp->mb_len += header_len;
	    buf -= header_len;
	}

	mx_muxid_t muxid = mcp->mc_request ? mcp->mc_request->mr_muxid : 0;
	mx_header_t *mhp = (mx_header_t *) buf;
	mx_websocket_header_build(mhp, len, MX_OP_REPLY, muxid);
	buf[sizeof(*mhp)] = '\n';

	mcp->mc_state = MSS_RPC_READ_REPLY;
    }

    int rc = write(msp->ms_sock, buf, len);
    if (rc > 0) {
	if (rc == len) {
	    mx_buffer_reset(mbp);
	    mcp->mc_state = MSS_RPC_IDLE;
	} else if (rc < header_len) {
	    mx_log("S%u very short write (%d/%d/%lu)",
		   msp->ms_id, rc, len, mbp->mb_len);
	} else {
	    /*
	     * If we didn't used a malloc buffer, we want header_len
	     * to count as part of the length.
	     */
	    if (mbuf)
		rc -= header_len;
	    mbp->mb_start += rc;
	    mbp->mb_len -= rc;
	}
    }

    if (mbuf)
	free(mbuf);

    return FALSE;
}

static int
mx_websocket_write_complete (MX_TYPE_WRITE_COMPLETE_ARGS)
{
    mx_log("S%u write complete", msp->ms_id);

    if (mcp->mc_state == MSS_RPC_READ_REPLY) {
	/* XXX Do something */
    }

    int len = sizeof(mx_header_t) + 1;
    char buf[len];

    mx_muxid_t muxid = mcp->mc_request ? mcp->mc_request->mr_muxid : 0;
    mx_header_t *mhp = (mx_header_t *) buf;
    mx_websocket_header_build(mhp, len, MX_OP_COMPLETE, muxid);
    buf[sizeof(*mhp)] = '\n';
	
    int rc = write(msp->ms_sock, buf, len);
    if (rc > 0) {
	if (rc != len)
	    mx_log("S%u complete very short write (%d/%d)",
		   msp->ms_id, rc, len);
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
    mti_write_complete: mx_websocket_write_complete,
#if 0
    mti_set_channel: mx_websocket_set_channel,
#endif
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

