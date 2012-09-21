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
#include "forwarder.h"

static void
mx_session_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;

    mx_log("%*s%starget %s, session %p", indent, "", prefix,
	   mssp->mss_target,
	   mssp->mss_session);

    mx_log("%*s%sChannels in use:", indent, "", prefix);
    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {
	mx_channel_print(mcp, indent, prefix);
    }

    mx_log("%*s%sChannels released:", indent, "", prefix);
    TAILQ_FOREACH(mcp, &mssp->mss_released, mc_link) {
	mx_channel_print(mcp, indent, prefix);
    }
}

mx_sock_session_t *
mx_session_create (LIBSSH2_SESSION *session, int sock, const char *target)

{
    mx_sock_session_t *mssp = malloc(sizeof(*mssp));
    if (mssp == NULL)
	return NULL;

    bzero(mssp, sizeof(*mssp));
    mssp->mss_base.ms_id = ++mx_sock_id;
    mssp->mss_base.ms_type = MST_SESSION;
    mssp->mss_base.ms_sock = sock;

    mssp->mss_session = session;
    mssp->mss_target = strdup(target);
    TAILQ_INIT(&mssp->mss_channels);
    TAILQ_INIT(&mssp->mss_released);

    TAILQ_INSERT_HEAD(&mx_sock_list, &mssp->mss_base, ms_link);
    mx_sock_count += 1;

    MX_LOG("S%u new %s, fd %u, target %s",
	   mssp->mss_base.ms_id, mx_sock_type(&mssp->mss_base),
	   mssp->mss_base.ms_sock, mssp->mss_target);

    return mssp;
}

/*
 * At this point we haven't yet authenticated, and we don't know if we can
 * trust the remote host.  So we extract the hostkey and check if it's a
 * known host.  If so, we're cool.  If not, report the issue to the user
 * and let them make the call.
 */
int
mx_session_check_hostkey (mx_sock_session_t *session,
			  mx_sock_t *client UNUSED, mx_request_t *mrp)
{
    const char *fingerprint;
    int i;
    char buf[1024], *bp = buf, *ep = buf + sizeof(buf);

    fingerprint = libssh2_hostkey_hash(session->mss_session,
				       LIBSSH2_HOSTKEY_HASH_SHA1);
    bp += snprintf(bp, ep - bp, "Fingerprint: ");
    for (i = 0; i < 20; i++)
        bp += snprintf(bp, ep - bp, "%02X ", (unsigned char) fingerprint[i]);
    *bp++ = '\n';
    *bp++ = '\0';

    mx_log("S%u hostkey check %s", session->mss_base.ms_id, buf);

    if (client && mx_mti(client)->mti_check_hostkey)
	return mx_mti(client)->mti_check_hostkey(session, client, mrp);

    return FALSE;
}

int
mx_session_check_auth (mx_sock_session_t *session, mx_sock_t *client UNUSED,
		       const char *user, const char *password)
{
    int rc, auth_publickey = FALSE, auth_password = FALSE;
    char *userauthlist;

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session->mss_session,
					 user, strlen(user));
    mx_log("Authentication methods: %s", userauthlist);

    if (strstr(userauthlist, "password"))
        auth_password = TRUE;
    if (strstr(userauthlist, "publickey"))
        auth_publickey = TRUE;

    LIBSSH2_AGENT *agent = NULL;
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;

    if (auth_publickey) {
	if (password == NULL || *password == '\0')
	    password = mx_password(session->mss_target, user);

	if (password && *password) {

	    if (libssh2_userauth_publickey_fromfile(session->mss_session, user,
						    keyfile1, keyfile2,
						    password)) {
		mx_log("authentication by public key failed");
	    } else {
		mx_log("Authentication by public key succeeded");
		return FALSE;
	    }
	}

	/* Connect to the ssh-agent */
	agent = libssh2_agent_init(session->mss_session);
	if (!agent) {
	    mx_log("failure initializing ssh-agent support");
	    goto try_next;
	}

	if (libssh2_agent_connect(agent)) {
	    mx_log("failure connecting to ssh-agent");
	    goto try_next;
	}

	if (libssh2_agent_list_identities(agent)) {
	    mx_log("failure requesting identities to ssh-agent");
	    goto try_next;
	}

	for (;;) {
	    rc = libssh2_agent_get_identity(agent, &identity, prev_identity);
	    if (rc == 1)	/* 1 -> end of list of identities */
		break;

	    if (rc < 0) {
		mx_log("Failure obtaining identity from ssh-agent");
		break;
	    }

	    if (libssh2_agent_userauth(agent, user, identity) == 0) {
		mx_log("S%u ssh auth username %s, public key %s succeeded",
		       session->mss_base.ms_id, user, identity->comment);
		/* Rah!!  We're authenticated now */
		return FALSE;
	    }

	    mx_log("S%u ssh auth username %s, public key %s failed",
		   session->mss_base.ms_id, user, identity->comment);

	    prev_identity = identity;
	}
    }
	
 try_next:
    if (auth_password) {
	if (password == NULL || *password == '\0')
	    password = mx_password(session->mss_target, user);

	if (password == NULL || *password == '\0') {
	    mx_log("S%u no password for target '%s', user '%s'",
		   session->mss_base.ms_id, session->mss_target, user);
	    /* XXX send 'password' action to client */
	    return TRUE;
	}

        if (libssh2_userauth_password(session->mss_session, user, password)) {
	    session->mss_pwfail += 1;
            mx_log("S%u authentication by password failed (%u)",
		   session->mss_base.ms_id, session->mss_pwfail);
	    if (session->mss_pwfail > MAX_PWFAIL)
		goto failure;
	    return TRUE;
        }

	mx_log("S%u ssh auth username %s, password succeeded",
	       session->mss_base.ms_id, user);
	/* We're authenticated now */
	return FALSE;
    }

    mx_log("S%u no supported authentication methods found",
	   session->mss_base.ms_id);

 failure:
    session->mss_base.ms_state = MSS_FAILED;
    return TRUE;
}

mx_sock_session_t *
mx_session_open (mx_sock_t *client, mx_request_t *mrp)
{
    int rc, sock = -1;
    mx_sock_session_t *mssp;
    struct sockaddr_in sin;
    LIBSSH2_SESSION *session;

    /* Connect to SSH server */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sin.sin_family = AF_INET;
    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(LOCALHOST_ADDRESS))) {
        mx_log("session: inet_addr: %s", strerror(errno));
        return NULL;
    }

    sin.sin_port = htons(22);
    if (connect(sock, (struct sockaddr*) &sin, sizeof(struct sockaddr_in))) {
        mx_log("failed to connect");
        return NULL;
    }

    /* Create a session instance */
    session = libssh2_session_init();
    if (!session) {
        mx_log("could not initialize SSH session");
        return NULL;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);
    if (rc) {
        mx_log("error when starting up SSH session: %d", rc);
        return NULL;
    }

    /*
     * We allocate the mx_sock_session_t now, knowing that we may
     * still have problems.  If we don't make it thru, we use the
     * ms_state to record our current state.
     */
    mssp = mx_session_create(session, sock, mrp->mr_target);
    if (mssp == NULL) {
	mx_log("mx session failed");
	return NULL;
    }

    if (mx_session_check_hostkey(mssp, client, mrp))
	return mssp;

    const char *user = mrp->mr_user ?: opt_user ?: getlogin();

    if (mx_session_check_auth(mssp, client, user, mrp->mr_password))
	return mssp;

    mssp->mss_base.ms_state = MSS_ESTABLISHED;

    return mssp;
}

static mx_sock_session_t *
mx_session_find (const char *target)
{
    mx_sock_t *msp;
    mx_sock_session_t *mssp;

    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	if (msp->ms_type != MST_SESSION)
	    continue;

	mssp = mx_sock(msp, MST_SESSION);
	if (streq(target, mssp->mss_target))
	    return mssp;
    }

    return NULL;
}

mx_sock_session_t *
mx_session (mx_sock_t *client, mx_request_t *mrp)
{
    mx_sock_session_t *session = mx_session_find(mrp->mr_target);

    if (session == NULL)
	session = mx_session_open(client, mrp);

    return session;
}

static void
mx_session_close (mx_sock_t *msp)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);

    mx_channel_t *mcp;
    LIBSSH2_SESSION *session = mssp->mss_session;

    for (;;) {
	mcp = TAILQ_FIRST(&mssp->mss_channels);
	if (mcp == NULL)
	    break;
	TAILQ_REMOVE(&mssp->mss_channels, mcp, mc_link);
	mx_channel_close(mcp);
    }

    libssh2_session_disconnect(session, "Client disconnecting");
    libssh2_session_free(session);

    free(mssp->mss_target);
}

static int
mx_session_prep (MX_TYPE_PREP_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;
    unsigned long read_avail = 0;
    int buf_input = FALSE, buf_output = FALSE;

    DBG_POLL("S%u prep: readable %s",
	     msp->ms_id, mx_sock_isreadable(msp->ms_id) ? "yes" : "no");

    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {
	if (!buf_input) {
	    if (mcp->mc_rbufp->mb_len) {
		read_avail = mcp->mc_rbufp->mb_len;
		DBG_POLL("C%u buffer len %lu", mcp->mc_id, read_avail);
		buf_input = TRUE;
	    } else {
		libssh2_channel_window_read_ex(mcp->mc_channel,
					       &read_avail, NULL);
		if (read_avail) {
		    DBG_POLL("C%u avail %lu", mcp->mc_id, read_avail);
		    buf_input = TRUE;
		}
	    }
	}

	if (!buf_output) {
	    mx_sock_t *client = mcp->mc_client;
	    if (client && mx_mti(client)->mti_is_buf
		    && mx_mti(client)->mti_is_buf(client, POLLOUT)) {
		DBG_POLL("C%u has buffered input from forwarder",
			 mcp->mc_id);
		buf_output = TRUE;
	    }
	}

	/* If we already know both bits need set, then stop looking */
	if (buf_input && buf_output)
	    break;
    }

    if (buf_input) {
	*timeout = 0;
	return FALSE;
    }

    pollp->fd = msp->ms_sock;
    pollp->events = (buf_input ? 0 : POLLIN) | (buf_output ? POLLOUT : 0);

    return TRUE;
}

static int
mx_session_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;

    DBG_POLL("S%u processing (%p/0x%x) readable %s",
	     msp->ms_id, pollp, pollp ? pollp->revents : 0,
	     mx_sock_isreadable(msp->ms_sock) ? "yes" : "no");

    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {

	for (;;) {
	    if (mx_channel_handle_input(mcp))
		break;
	}

	if (libssh2_channel_eof(mcp->mc_channel)) {
	    mx_log("C%u: disconnect, eof", mcp->mc_id);
	    return TRUE;
	}
    }

    DBG_POLL("S%u done, readable %s", msp->ms_id,
	     mx_sock_isreadable(msp->ms_sock) ? "yes" : "no");

    return FALSE;
}

void
mx_session_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_SESSION,
    mti_name: "session",
    mti_print: mx_session_print,
    mti_prep: mx_session_prep,
    mti_poller: mx_session_poller,
    mti_close: mx_session_close,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
