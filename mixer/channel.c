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
#include "session.h"
#include "channel.h"
#include "netconf.h"
#include <ctype.h>

static unsigned mx_channel_id; /* Monotonically increasing ID number */

#define NETCONF_MARKER "]]>]]>"
const char mx_netconf_marker[] = NETCONF_MARKER;
unsigned mx_netconf_marker_len = sizeof(mx_netconf_marker) - 1;

static int
mx_channel_read (mx_channel_t *mcp, char *buf, unsigned long bufsiz)
{
    int len;

    len = libssh2_channel_read(mcp->mc_channel, buf, bufsiz);

    DBG_POLL("C%u read %d", mcp->mc_id, len);
    if (len > 0) {
	if (opt_debug & DBG_FLAG_DUMP)
	    slaxMemDump("chread: ", buf, len, ">", 0);
    } else {
	if (len == LIBSSH2_ERROR_SOCKET_RECV) {
	    if (mcp->mc_session)
		mcp->mc_session->mss_base.ms_state = MSS_FAILED;
	}
    }

    return len;
}

static int
mx_channel_write (mx_channel_t *mcp, const char *buf, unsigned long bufsiz)
{
    int len;

    len = libssh2_channel_write(mcp->mc_channel, buf, bufsiz);

    DBG_POLL("C%u write %d", mcp->mc_id, len);
    if (len > 0)
	if (opt_debug & DBG_FLAG_DUMP)
	    slaxMemDump("chwrite: ", buf, len, ">", 0);

    return len;
}

mx_channel_t *
mx_channel_create (mx_sock_session_t *session,
		   mx_sock_t *client, LIBSSH2_CHANNEL *channel)
{
    mx_channel_t *mcp = malloc(sizeof(*mcp));
    if (mcp == NULL)
	return NULL;

    bzero(mcp, sizeof(*mcp));
    mcp->mc_id = ++mx_channel_id;
    mcp->mc_session = session;
    mcp->mc_channel = channel;
    mcp->mc_rbufp = mx_buffer_create(0);

    if (mx_mti(client)->mti_set_channel)
	mx_mti(client)->mti_set_channel(client, session, mcp);
    mcp->mc_client = client;

    TAILQ_INSERT_HEAD(&session->mss_channels, mcp, mc_link);

    MX_LOG("C%u: new channel, S%u, channel %p, client S%u",
	   mcp->mc_id, mcp->mc_session->mss_base.ms_id,
	   mcp->mc_channel, mcp->mc_client->ms_id);

    return mcp;
}

mx_channel_t *
mx_channel_direct_tcpip (mx_sock_session_t *session, mx_sock_t *client,
			 const char *desthost, unsigned destport)
{
    LIBSSH2_CHANNEL *channel;
    const char *shost = inet_ntoa(client->ms_sin.sin_addr);
    unsigned int sport = ntohs(client->ms_sin.sin_port);

    /* Must use blocking IO for channel creation */
    libssh2_session_set_blocking(session->mss_session, 1);

    channel = libssh2_channel_direct_tcpip_ex(session->mss_session,
					      desthost, destport,
					      shost, sport);

    libssh2_session_set_blocking(session->mss_session, 0);

    if (channel == NULL) {
        mx_log("could not open the direct-tcpip channel");
	return NULL;
    }

    mx_channel_t *mcp = mx_channel_create(session, client, channel);
    if (mcp == NULL) {
	/* XXX fail */
	return NULL;
    }

    return mcp;
}

static int
mx_channel_netconf_send_hello (mx_channel_t *mcp)
{
    const char hello[] = "<?xml version=\"1.0\"?>\n"
	"<hello xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"
	  "<capabilities>\n"
	  "<capability>urn:ietf:params:netconf:base:1.0</capability>\n"
	  "</capabilities>"
	"</hello>\n" NETCONF_MARKER "\n";
    int len, hlen = sizeof(hello) - 1;

    len = mx_channel_write(mcp, hello, hlen);
    mx_log("C%u sent hello (%d)%s", mcp->mc_id, len,
	   (len == hlen) ? " complete" : " short");

    return 0;
}

/*
 * Detect if we have the NETCONF end-of-frame marker at the end
 * of our input stream.
 */
static int
mx_channel_netconf_has_marker (mx_channel_t *mcp)
{
    char *cp;
    char *zp;
    const char *czp;
    mx_buffer_t *prev = NULL, *cur = mcp->mc_rbufp;

    /* Find the previous buffer, if there is one */
    while (cur->mb_next != NULL) {
	prev = cur;
	cur = cur->mb_next;
    }

    for (;;) {
	for (cp = cur->mb_data + cur->mb_start + cur->mb_len - 1;
	     cp > cur->mb_data + cur->mb_start; cp--)
	    if (!isspace((int) *cp))
		goto found_end;
	if (prev == NULL)
	    return FALSE;

	cur = prev;
	prev = NULL;
    }

    found_end:

    /*
     * We've found the last trailing non-ws char, so we need
     * to see if the previous bytes are the NETCONF framing
     * marker.  The marker might span the last buffer and
     * the previous one, which is annoy to handle.
     */

    if (cp - (cur->mb_data + cur->mb_start) > mx_netconf_marker_len) {
	zp = cp + 1 - mx_netconf_marker_len;
	if (0) mx_log("[%.*s] [%.*s]", mx_netconf_marker_len, zp,
		      mx_netconf_marker_len, mx_netconf_marker);
	if (memcmp(zp, mx_netconf_marker, mx_netconf_marker_len) == 0) {
	    /*
	     * Mark the new end of the string, discarding the
	     * marker and and trailing whitespace.
	     */
	    *zp = '\0';	/* Shouldn't matter, but ... */
	    cur->mb_len = zp - (cur->mb_data + cur->mb_start);
	    if (cur->mb_next) {
		/* If the next buffer is empty, free it */
		mx_buffer_free(cur->mb_next);
		cur->mb_next = NULL;
	    }
	    return TRUE;
	}

    } else {

	int left = cp - (cur->mb_data + cur->mb_start);
	czp = mx_netconf_marker + mx_netconf_marker_len - left;
	if (memcmp(cp, czp, left) == 0) {
	    if (prev) {
		left = mx_netconf_marker_len - left;
		if (memcmp(prev->mb_data + prev->mb_start
			   + prev->mb_len - left,
			   mx_netconf_marker, left) == 0) {
		    prev->mb_len -= left;
		    /* Shouldn't matter, but ... */
		    prev->mb_data[prev->mb_start + prev->mb_len] = '\0';

		    prev->mb_next = NULL;
		    mx_buffer_free(cur);
		    return TRUE;
		}
	    }
	}
    }

    return FALSE;
}

static int
mx_channel_netconf_read_hello (mx_channel_t *mcp)
{
    mx_buffer_t *mbp = mcp->mc_rbufp;
    int len;

    for (;;) {
	if (mbp->mb_start + mbp->mb_len == mbp->mb_size) {
	    mx_buffer_t *newp = mx_buffer_create(0);
	    if (newp == NULL) {
		mx_log("C%u cannot extend buffer", mcp->mc_id);
		return TRUE;
	    }

	    mbp->mb_next = newp;
	    mbp = newp;
	}

	len = mx_channel_read(mcp, mbp->mb_data + mbp->mb_start + mbp->mb_len,
			       mbp->mb_size - (mbp->mb_start + mbp->mb_len));

	if (libssh2_channel_eof(mcp->mc_channel)) {
	    DBG_POLL("C%u eof during read_hello", mcp->mc_id);
	    return FALSE;
	}

	if (len == LIBSSH2_ERROR_EAGAIN) {
	    /* Nothing to read, nothing to write; move on */
	    DBG_POLL("C%u is drained", mcp->mc_id);
#if 0
	    break;
#else
	    sleep(1);
	    continue;
#endif
	}

	if (len < 0)
	    return TRUE;

	mbp->mb_len += len;
	DBG_POLL("C%u read %d", mcp->mc_id, len);

	if (mx_channel_netconf_has_marker(mcp)) {
	    mx_log("C%u found end-of-frame; len %lu, discarding",
		   mcp->mc_id, mcp->mc_rbufp->mb_len);
	    mbp->mb_len = mbp->mb_start = 0;
	    if (mbp->mb_next) {
		mx_buffer_free(mbp->mb_next);
		mbp->mb_next = NULL;
	    }
	    return TRUE;
	}
    }

    return FALSE;
}

mx_channel_t *
mx_channel_netconf (mx_sock_session_t *mssp, mx_sock_t *client, int xml_mode)
{
    LIBSSH2_CHANNEL *channel;
    mx_channel_t *mcp;

    mcp = TAILQ_FIRST(&mssp->mss_released);
    if (mcp) {
	mx_log("%s reusing channel C%u for client S%u",
               mx_sock_title(&mssp->mss_base),
	       mcp->mc_id, client->ms_id);

	TAILQ_REMOVE(&mssp->mss_released, mcp, mc_link);
	TAILQ_INSERT_HEAD(&mssp->mss_channels, mcp, mc_link);

	mcp->mc_state = MSS_RPC_INITIAL;
	mcp->mc_client = client;
	if (mx_mti(client)->mti_set_channel)
	    mx_mti(client)->mti_set_channel(client, mcp->mc_session, mcp);

	return mcp;
    }

    /* Must use blocking IO for channel creation */
    libssh2_session_set_blocking(mssp->mss_session, 1);

    channel = libssh2_channel_open_session(mssp->mss_session);
    if (channel == NULL) {
	mx_log("%s could not open netconf channel",
               mx_sock_title(&mssp->mss_base));
	return NULL;
    }

    if (!xml_mode) {
	if (libssh2_channel_subsystem(channel, "netconf") != 0) {
	    mx_log("%s could not open netconf subsystem",
		   mx_sock_title(&mssp->mss_base));
	    goto try_xml_mode;
	}
	mx_log("%s opened netconf subsystem channel to %s",
	       mx_sock_title(&mssp->mss_base), mssp->mss_target);
    } else {
	static const char command[] = "xml-mode netconf need-trailer";

    try_xml_mode:
	if (libssh2_channel_process_startup(channel,
					    "exec", sizeof("exec") - 1,
					    command, strlen(command)) != 0) {
	    mx_log("%s could not open netconf xml-mode",
		   mx_sock_title(&mssp->mss_base));
	    libssh2_channel_free(channel);
	    channel = NULL;
	} else {
	    mx_log("%s opened netconf xml-mode channel to %s",
		   mx_sock_title(&mssp->mss_base), mssp->mss_target);
	}
    }

    libssh2_session_set_blocking(mssp->mss_session, 0);

    if (channel == NULL) {
	mx_log("%s could not open netconf channel",
               mx_sock_title(&mssp->mss_base));
	return NULL;
    }

    mcp = mx_channel_create(mssp, client, channel);
    if (mcp == NULL) {
	/* XXX fail */
	return NULL;
    }

    mx_channel_netconf_send_hello(mcp);
    mx_channel_netconf_read_hello(mcp);

    return mcp;
}

void
mx_channel_close (mx_channel_t *mcp)
{
    if (mcp == NULL)
	return;

    if (mcp->mc_client) {
	mx_sock_t *msp = mcp->mc_client;
	if (mx_mti(msp)->mti_set_channel)
	    mx_mti(msp)->mti_set_channel(msp, NULL, NULL);
    }

    libssh2_channel_free(mcp->mc_channel);
    mcp->mc_channel = NULL;
    free(mcp);
}

int
mx_channel_write_buffer (mx_channel_t *mcp, mx_buffer_t *mbp)
{
    int len = mbp->mb_len, slen = mbp->mb_len, rc;

    do {
	rc = mx_channel_write(mcp, mbp->mb_data + mbp->mb_start, len);
	if (rc < 0) {
	    if (rc == LIBSSH2_ERROR_EAGAIN)
		return rc;

	    /* XXX recovery/close? */
	    mx_log("C%u write failed %d", mcp->mc_id, rc);
	    return rc;
	}

	mbp->mb_start += rc;
	len -= rc;
    } while (len > 0);

    mbp->mb_start = mbp->mb_len = 0; /* Reset */

    return slen;
}

static int
mx_channel_netconf_detect_marker (mx_channel_t *mcp UNUSED,
				  mx_buffer_t *mbp UNUSED)
{
    mx_offset_t len;
    char *sp, *cp, *ep, *zp;

    if (mcp->mc_marker_seen) {
	mx_log("C%u netconf: checking for marker at beginning (%lu/%lu)",
	       mcp->mc_id, mcp->mc_marker_seen, mbp->mb_len);

	len = mx_netconf_marker_len - mcp->mc_marker_seen;
	if (len > mbp->mb_len)
	    len = mbp->mb_len;

	cp = mbp->mb_data + mbp->mb_start;
	if (memcmp(cp, mx_netconf_marker + mcp->mc_marker_seen, len) == 0) {
	    if (len != mx_netconf_marker_len - mcp->mc_marker_seen) {
		mx_log("C%u netconf marker more found (%lu/%lu)",
		       mcp->mc_id, mcp->mc_marker_seen, mbp->mb_len);
		mcp->mc_marker_seen += len;
		mbp->mb_start = mbp->mb_len = 0;
		return FALSE;
	    }

	    mx_log("C%u netconf marker found at beginning (%lu/%lu)",
		   mcp->mc_id, mcp->mc_marker_seen, mbp->mb_len);
	    mcf_set_seen_eoframe(mcp);
	    mx_buffer_reset(mbp);
	    return TRUE;
	}
    }

    sp = mbp->mb_data + mbp->mb_start;
    zp = mbp->mb_data + mbp->mb_start + mbp->mb_len;

    for (ep = zp - 1; ep >= sp; ep--)
	if (!isspace((int) *ep))  /* skip trailing white space */
	    break;
    ep += 1;

    cp = ep - mx_netconf_marker_len;
    for ( ; cp >= sp && cp < ep; cp++) {
	if (memcmp(cp, mx_netconf_marker, ep - cp) == 0)
	    goto found;
	if (ep != zp - 1)	/* If there was trailing whitespace */
	    break;
    }

    return FALSE;		/* Nothing interesting */

 found:
    /*
     * We've found part of the end-of-frame marker.  If we found the whole
     * thing, then life is good.  We set the start to done.
     */
    mbp->mb_len -= zp - cp;

    if (ep - cp == mx_netconf_marker_len) {
	mx_log("C%u netconf marker found", mcp->mc_id);
	mcf_set_seen_eoframe(mcp);
	return TRUE;
    }

    mx_log("C%u netconf marker partial found (%ld/%lu)",
	   mcp->mc_id, ep - cp, mbp->mb_len);
    mcp->mc_marker_seen = ep - cp;
    return FALSE;
}

void
mx_channel_release (mx_channel_t *mcp)
{
    mx_sock_t *client = mcp->mc_client;
    mx_sock_session_t *session = mcp->mc_session;

    MX_LOG("C%u: release channel, S%u, channel %p, client S%u",
	   mcp->mc_id, session->mss_base.ms_id,
	   mcp->mc_channel, client->ms_id);

    if (client && mx_mti(client)->mti_set_channel)
	mx_mti(client)->mti_set_channel(client, NULL, NULL);
    mcp->mc_client = NULL;
    mcp->mc_request = NULL;

    TAILQ_REMOVE(&session->mss_channels, mcp, mc_link);
    TAILQ_INSERT_HEAD(&session->mss_released, mcp, mc_link);
}

void
mx_channel_print (mx_channel_t *mcp, int indent, const char *prefix)
{
    mx_buffer_t *mbp = mcp->mc_rbufp;
    unsigned long read_avail = 0;

    libssh2_channel_window_read_ex(mcp->mc_channel, &read_avail, NULL);

    mx_log("%*s%sC%u: S%u, channel %p, client S%u, rb %lu/%lu, avail %lu",
	   indent + INDENT, "", prefix,
	   mcp->mc_id, mcp->mc_session->mss_base.ms_id,
	   mcp->mc_channel, mcp->mc_client ? mcp->mc_client->ms_id : 0,
	   mbp->mb_start, mbp->mb_len, read_avail);
}

int
mx_channel_handle_input (mx_channel_t *mcp)
{
    int len;
    mx_buffer_t *mbp = mcp->mc_rbufp;

    if (mbp->mb_len == 0) { /* Nothing buffered */
	mbp->mb_start = 0;
	len = mx_channel_read(mcp, mbp->mb_data, mbp->mb_size);
	if (len == LIBSSH2_ERROR_EAGAIN) {
	    /* Nothing to read, nothing to write; move on */
	    DBG_POLL("C%u is drained", mcp->mc_id);
	    return 1;

	} else if (libssh2_channel_eof(mcp->mc_channel)) {
	    DBG_POLL("C%u is at eof", mcp->mc_id);
	    return 1;

	} else if (len < 0) {
	    return len;

	} else {
	    mbp->mb_len = len;
	}
    }

    mx_channel_netconf_detect_marker(mcp, mbp);

    /*
     * If the write call would block (returns TRUE), then
     * we move on.
     */
    if (mcp->mc_client == NULL
	    || mx_mti(mcp->mc_client)->mti_write(mcp->mc_client, mcp, mbp))
	return 1;

    /*
     * If we're in IDLE/INIT state and we're seen the end-of-frame
     * marker, then the RPC is complete.
     */
    if (mcf_is_seen_eoframe(mcp) && (mcp->mc_state == MSS_RPC_INITIAL
				     || mcp->mc_state == MSS_RPC_IDLE)) {
	mcp->mc_state = MSS_RPC_COMPLETE;
	mcf_clear_seen_eoframe(mcp);
    }

    if (mcp->mc_state == MSS_RPC_COMPLETE && !mcf_is_hold_channel(mcp)) {
	/*
	 * The RPC is complete, so we can detach the channel from the
	 * websocket, allowing us to reuse it.
	 */
	if (mcp->mc_client && mx_mti(mcp->mc_client)->mti_write_complete)
	    mx_mti(mcp->mc_client)->mti_write_complete(mcp->mc_client, mcp);
	
	mx_channel_release(mcp);
    }

    return 0;
}

int
mx_channel_sock (mx_channel_t *mcp)
{
    return mcp->mc_session->mss_base.ms_sock;
}

int
mx_channel_has_buffered (mx_channel_t *mcp)
{
    return (mcp->mc_rbufp->mb_len != 0);
}

