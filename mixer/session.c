/*
 * $Id$
 *
 * Copyright (c) 2012-2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#include <netdb.h>

#include "local.h"
#include "util.h"
#include "session.h"
#include "channel.h"
#include "forwarder.h"
#include "request.h"
#include "db.h"

static char *known_hosts;

static void
mx_session_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;

    mx_log("%*s%starget %s, session %p (%s)", indent, "", prefix,
	   mssp->mss_target, mssp->mss_session,
	   mssp->mss_canonname ?: "???");

    mx_log("%*s%sChannels in use:%s", indent, "", prefix,
	   TAILQ_EMPTY(&mssp->mss_channels) ? " none" : "");
    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {
	mx_channel_print(mcp, indent, prefix);
    }

    mx_log("%*s%sChannels released:%s", indent, "", prefix,
	   TAILQ_EMPTY(&mssp->mss_released) ? " none" : "");
    TAILQ_FOREACH(mcp, &mssp->mss_released, mc_link) {
	mx_channel_print(mcp, indent, prefix);
    }
}

mx_sock_session_t *
mx_session_create (LIBSSH2_SESSION *session, int sock,
		   const char *target, const char *canonname)
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
    if (canonname)
	mssp->mss_canonname = strdup(canonname);
    TAILQ_INIT(&mssp->mss_channels);
    TAILQ_INIT(&mssp->mss_released);

    TAILQ_INSERT_HEAD(&mx_sock_list, &mssp->mss_base, ms_link);
    mx_sock_count += 1;

    MX_LOG("S%u new %s, fd %u, target %s/%s",
	   mssp->mss_base.ms_id, mx_sock_type(&mssp->mss_base),
	   mssp->mss_base.ms_sock, mssp->mss_target,
	   mssp->mss_canonname ?: "");

    return mssp;
}

/*
 * At this point we haven't yet authenticated, and we don't know if we can
 * trust the remote host.  So we extract the hostkey and check if it's a
 * known host.  If so, we're cool.  If not, report the issue to the user
 * and let them make the call.
 */
int
mx_session_check_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp)
{
    mx_sock_t *client = mrp->mr_client;
    const char *fingerprint;
    int i;
    char buf[BUFSIZ], *bp = buf, *ep = buf + sizeof(buf);
    size_t len = 0;
    int type = 0;
    const char *typename = NULL;
    int check = 0;
    const char *hostname = mssp->mss_canonname ?: mssp->mss_target;

    const char line0[] = "WARNING: Host key for '%s' was found, "
	"but the keys do not match!\n";
    const char line1[] = "The authenticity of host '%s' can't "
	"be established.\n";
    const char line2[] = "%s key fingerprint is ";
    
    fingerprint = libssh2_session_hostkey(mssp->mss_session, &len, &type);
    if (fingerprint) {
	switch (type) {
	case LIBSSH2_HOSTKEY_TYPE_RSA:
	    typename = "RSA";
	    break;

	case LIBSSH2_HOSTKEY_TYPE_DSS:
	    typename = "DSA";
	    break;

	case LIBSSH2_HOSTKEY_TYPE_UNKNOWN:
	    typename = "unknown";
	    break;
	}
    }

    if (opt_knownhosts && fingerprint && known_hosts) {
	LIBSSH2_KNOWNHOSTS *nh = libssh2_knownhost_init(mssp->mss_session);

	if (nh) {
	    /* Read all hosts from here */ 
	    libssh2_knownhost_readfile(nh, known_hosts,
				       LIBSSH2_KNOWNHOST_FILE_OPENSSH);
	    struct libssh2_knownhost *host;

	    check = libssh2_knownhost_checkp(nh, hostname, 22,
					     fingerprint, len,
					     LIBSSH2_KNOWNHOST_TYPE_PLAIN|
                                             LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                             &host);
	    switch (check) {
	    case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
		/* something prevented the check to be made */
		bp += snprintf_safe(bp, ep - bp,
				    "Known host check failure\n");
		check = 0;
		break;

	    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
		/* no host match was found */
		check = 0;
		break;

	    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
		/* hosts and keys match */
		return FALSE;

	    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
		/* host was found, but the keys didn't match! */
		bp += snprintf_safe(bp, ep - bp, line0, hostname);
		break;
	    }

	    libssh2_knownhost_free(nh);
	}
    }

    fingerprint = libssh2_hostkey_hash(mssp->mss_session,
				       LIBSSH2_HOSTKEY_HASH_MD5);

    /*
     * Check our hostkey database for this keymatch.
     */
    if (check == 0) {
	switch (mx_db_check_hostkey(mssp, mrp)) {
	case DB_CHECK_HOSTKEY_MATCH:
	    return FALSE;

	case DB_CHECK_HOSTKEY_MISMATCH:
	    bp += snprintf_safe(bp, ep - bp, line0, hostname);
	    /* fallthru */

	case DB_CHECK_HOSTKEY_NOMATCH:
	    bp += snprintf_safe(bp, ep - bp, line1, hostname);
	    bp += snprintf_safe(bp, ep - bp, line2, typename);
	}
    }

    for (i = 0; i < 20; i++)
        bp += snprintf_safe(bp, ep - bp, "%02X%s",
			    (unsigned char) fingerprint[i], 
			    i < 19 ? ":" : "");
    bp += snprintf_safe(bp, ep - bp, ".\n");
    bp += snprintf_safe(bp, ep - bp, "Do you want to accept this host key?");

    mx_log("S%u hostkey check [%s]", mssp->mss_base.ms_id, buf);

    if (client && mx_mti(client)->mti_check_hostkey)
	return mx_mti(client)->mti_check_hostkey(client, mrp, buf);

    return FALSE;
}

static int
mx_session_check_agent (mx_sock_session_t *session, mx_request_t *mrp UNUSED,
			const char *user)
{
    LIBSSH2_AGENT *agent = NULL;
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;
    int rc;

    /* Connect to the ssh-agent */
    agent = libssh2_agent_init(session->mss_session);
    if (!agent) {
	mx_log("failure initializing ssh-agent support");
	return FALSE;
    }

    if (libssh2_agent_connect(agent)) {
	mx_log("failure connecting to ssh-agent");
	return FALSE;
    }

    if (libssh2_agent_list_identities(agent)) {
	mx_log("failure requesting identities to ssh-agent");
	return FALSE;
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
	    return TRUE;
	}

	mx_log("S%u ssh auth username %s, public key %s failed",
	       session->mss_base.ms_id, user, identity->comment);

	prev_identity = identity;
    }

    return FALSE;
}

static int
mx_session_check_keyfile (mx_sock_session_t *session, mx_request_t *mrp,
			  const char *user)
{
    mx_sock_t *msp = mrp->mr_client;
    const char *passphrase = mrp->mr_passphrase;
    const char *db_passphrase = mx_db_get_passphrase();

    if (passphrase && *passphrase == '\0') {
	mx_log("R%u null passphrase", mrp->mr_id);
	mx_request_set_state(mrp, MSS_PASSWORD);
	return FALSE;
    }

    if (db_passphrase == NULL && passphrase == NULL) {
	/*
	 * We got here with no passphrase; must be the initial attempt.
	 * Try to decrypt with no (empty) passphrase.
	 */
	if (libssh2_userauth_publickey_fromfile(session->mss_session, user,
						keyfile1, keyfile2, NULL)) {
	    /* Ignore failure */
	} else {
	    mx_log("R%u Authentication by empty public key succeeded",
		   mrp->mr_id);
	    goto success;
	}
    }

    if (passphrase && *passphrase) {
	if (libssh2_userauth_publickey_fromfile(session->mss_session, user,
						keyfile1, keyfile2,
						passphrase)) {
	    mx_log("R%u Authentication by new public key failed", mrp->mr_id);
	} else {
	    mx_log("R%u Authentication by new public key succeeded",
		   mrp->mr_id);
	    mx_db_save_passphrase(passphrase);
	    goto success;
	}
    }

    if (db_passphrase) {
	if (libssh2_userauth_publickey_fromfile(session->mss_session, user,
						keyfile1, keyfile2,
						db_passphrase)) {
	    mx_log("R%u Authentication by existing public key failed",
		   mrp->mr_id);
	} else {
	    mx_log("R%u Authentication by existing public key succeeded",
		   mrp->mr_id);
	    goto success;
	}
    }

    if (exists(keyfile1) && exists(keyfile2)) {
	char buf[BUFSIZ], *bp = buf, *ep = buf + sizeof(buf);

	if (passphrase)
	    bp += snprintf_safe(bp, ep - bp, "Invalid passphrase\n");

	bp += snprintf_safe(bp, ep - bp,
		 "Enter passphrase for keyfile %s:", keyfile1);

	mx_request_set_state(mrp, MSS_PASSPHRASE);
	if (mx_mti(msp)->mti_get_passphrase
	        && mx_mti(msp)->mti_get_passphrase(msp, mrp, buf)) {
	    return TRUE;
	}
    }

    return FALSE;

 success:
    mx_request_set_state(mrp, MSS_ESTABLISHED);
    return FALSE;
}

static int
mx_session_check_password (mx_sock_session_t *session, mx_request_t *mrp,
			  const char *user)
{
    mx_sock_t *msp = mrp->mr_client;
    const char *password = mrp->mr_password;

    if (password == NULL)
	password = mx_password(session->mss_target, user);

    if (password && *password) {
        if (libssh2_userauth_password(session->mss_session, user, password)) {
	    session->mss_pwfail += 1;
            mx_log("S%u authentication by password failed (%u)",
		   session->mss_base.ms_id, session->mss_pwfail);
	    if (session->mss_pwfail > MAX_PWFAIL)
		return FALSE;
        } else {
	    mx_log("S%u ssh auth username %s, password succeeded",
		   session->mss_base.ms_id, user);
	    /* We're authenticated now */
	    mx_request_set_state(mrp, MSS_ESTABLISHED);

	    /* Save to in-memory cache as well as db */
	    mx_password_save(mrp->mr_target, user, password);
	    mx_db_save_password(mrp, password);

	    return FALSE;
	}
    }

    char buf[BUFSIZ], *bp = buf, *ep = buf + sizeof(buf);

    if (password && *password)
	bp += snprintf_safe(bp, ep - bp, "Invalid password\n");

    bp += snprintf_safe(bp, ep - bp, "Enter password:");

    mx_request_set_state(mrp, MSS_PASSWORD);

    if (mx_mti(msp)->mti_get_password
	    && mx_mti(msp)->mti_get_password(msp, mrp, buf))
	return TRUE;

    return FALSE;
}

int
mx_session_check_auth (mx_sock_session_t *mssp, mx_request_t *mrp)
{
    const char *user = mrp->mr_user ?: opt_user ?: getlogin();
    const char *password = mrp->mr_password;
    int auth_publickey = FALSE, auth_password = FALSE;
    char *userauthlist;

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(mssp->mss_session,
					 user, strlen(user));
    mx_log("Authentication methods: %s", userauthlist ?: "(empty)");

    if (userauthlist) {
	if (strstr(userauthlist, "password"))
	    auth_password = TRUE;
	if (strstr(userauthlist, "publickey"))
	    auth_publickey = TRUE;
    }

    mx_request_set_state(mrp, MSS_PASSWORD);

    if (auth_publickey) {
	if (!opt_no_agent && mx_session_check_agent(mssp, mrp, user))
	    goto success;

	if (mx_session_check_keyfile(mssp, mrp, user))
	    return TRUE;

	if (mrp->mr_state == MSS_ESTABLISHED)
	    goto success;
    }

    if (auth_password && (mrp->mr_state != MSS_ESTABLISHED)) {
	if (password == NULL || *password == '\0')
	    password = mx_password(mssp->mss_target, user);

	mx_request_set_state(mrp, MSS_PASSWORD);
	if (mx_session_check_password(mssp, mrp, user))
	    return TRUE;

	if (mrp->mr_state == MSS_ESTABLISHED)
	    goto success;
    }

    mx_log("S%u no supported authentication methods found",
	   mssp->mss_base.ms_id);

    mx_request_set_state(mrp, MSS_FAILED);
    return TRUE;

 success:
    mx_log("R%u auth'd session is established; S%u",
	   mrp->mr_id, mssp->mss_base.ms_id);
    mx_request_set_state(mrp, MSS_ESTABLISHED);
    return FALSE;
}

int
mx_session_approve_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp)
{
    mx_log("R%u host key is approved S%u", mrp->mr_id, mssp->mss_base.ms_id);

    /*
     * Save hostkey to db
     */
    mx_db_save_hostkey(mssp, mrp);

    return FALSE;
}

mx_sock_session_t *
mx_session_open (mx_request_t *mrp)
{
    int rc, sock = -1;
    mx_sock_session_t *mssp;
    LIBSSH2_SESSION *session;
    struct addrinfo hints, *res, *aip;
    char buf[BUFSIZ];
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    snprintf(buf, sizeof(buf), "%d", mrp->mr_port);

    mx_log("R%u session open to %s", mrp->mr_id, mrp->mr_hostname);

    rc = getaddrinfo(mrp->mr_hostname, buf, &hints, &res);
    if (rc) {
	mx_log("R%u invalid hostname: '%s': %s", mrp->mr_id, mrp->mr_hostname,
	       gai_strerror(rc));
	mx_request_error(mrp, "invalid hostname: %s", mrp->mr_hostname);
	return NULL;
    }

    for (aip = res; aip; aip = aip->ai_next) {
	char hn[NI_MAXHOST], sn[NI_MAXSERV];
	if (getnameinfo(aip->ai_addr, aip->ai_addrlen, hn, sizeof(hn),
		sn, sizeof(sn), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
	    mx_log("R%u connecting to '%s': %s%s%s%s", mrp->mr_id, hn, sn,
		   aip->ai_canonname ? " (" :"",
		   aip->ai_canonname ?: "", aip->ai_canonname ? ")" : "");
	} else {
	    strncpy(hn, mrp->mr_hostname, sizeof(hn));
	    strncpy(sn, buf, sizeof(sn));
	}

	/* Connect to SSH server */
	sock = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
	if (sock < 0)
	    continue;

	if (connect(sock, aip->ai_addr, aip->ai_addrlen)) {
	    mx_log("R%u failed to connect to target '%s' '%s'",
		   mrp->mr_id, hn, sn);
	} else
	    break;

	close(sock);
    }

    if (aip == NULL) {
        mx_log("R%u could not open SSH session connection", mrp->mr_id);
	mx_request_error(mrp, "could not open connection: %s",
		mrp->mr_hostname);
	freeaddrinfo(res);
        return NULL;
    }

    mx_log("R%u connected to %s", mrp->mr_id, mrp->mr_hostname);

    /* Create a session instance */
    session = libssh2_session_init();
    if (!session) {
        mx_log("could not initialize SSH session");
	freeaddrinfo(res);
        return NULL;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);
    if (rc) {
        mx_log("error when starting up SSH session: %d", rc);
	freeaddrinfo(res);
        return NULL;
    }

    if (opt_keepalive)
	libssh2_keepalive_config(session, 1, opt_keepalive);

    /*
     * We allocate the mx_sock_session_t now, knowing that we may
     * still have problems.  If we don't make it thru, we use the
     * ms_state to record our current state.
     */
    mssp = mx_session_create(session, sock, mrp->mr_fulltarget,
	    aip->ai_canonname);
    if (mssp == NULL) {
	mx_log("mx session failed");
	freeaddrinfo(res);
	return NULL;
    }

    freeaddrinfo(res);

    mrp->mr_session = mssp;

    if (mx_session_check_hostkey(mssp, mrp)) {
	mx_request_set_state(mrp, MSS_HOSTKEY);
	mx_log("S%u R%u waiting for hostkey check; client S%u",
	       mssp->mss_base.ms_id, mrp->mr_id, mrp->mr_client->ms_id);
	return mssp;
    }

    if (mx_session_check_auth(mssp, mrp)) {
	mx_log("S%u R%u waiting for auth; client S%u",
	       mssp->mss_base.ms_id, mrp->mr_id, mrp->mr_client->ms_id);
    } else
	mx_request_set_state(mrp, MSS_ESTABLISHED);

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
	if (mssp->mss_canonname && streq(target, mssp->mss_canonname))
	    return mssp;
    }

    return NULL;
}

mx_sock_session_t *
mx_session (mx_request_t *mrp)
{
    mx_sock_session_t *session = mx_session_find(mrp->mr_fulltarget);

    if (session == NULL || session->mss_base.ms_state != MSS_ESTABLISHED)
	session = mx_session_open(mrp);

    return session;
}

static void
mx_session_close (mx_sock_t *msp)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;
    LIBSSH2_SESSION *session = mssp->mss_session;

    mx_request_release_session(mssp);

    for (;;) {
	mcp = TAILQ_FIRST(&mssp->mss_channels);
	if (mcp == NULL)
	    break;
	TAILQ_REMOVE(&mssp->mss_channels, mcp, mc_link);
	mx_channel_close(mcp);
    }

    libssh2_session_disconnect(session, "Client disconnecting");
    libssh2_session_free(session);
    mssp->mss_session = NULL;

    free(mssp->mss_target);
    free(mssp->mss_canonname);
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

    if (opt_keepalive) {
	int next = 0;
	int rc = libssh2_keepalive_send(mssp->mss_session, &next);
	if (rc == 0) {
	    next *= 1000;
	    if (0 && *timeout > next)
		*timeout = next;
	} else {
	    mx_log("S%u keepalive failed: %d", msp->ms_sock, rc);
	    mssp->mss_base.ms_state = MSS_FAILED;
	}
    }

    return TRUE;
}

static int
mx_session_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;
    mx_channel_t *dead = NULL;

    DBG_POLL("S%u processing (%p/0x%x) readable %s",
	     msp->ms_id, pollp, pollp ? pollp->revents : 0,
	     mx_sock_isreadable(msp->ms_sock) ? "yes" : "no");

#if 0
    if (mx_sock_isreadable(msp->ms_sock) && TAILQ_EMPTY(&mssp->mss_channels)
	    && TAILQ_EMPTY(&mssp->mss_released)) {
	/*
	 * Interesting development; we have no channels, but the
	 * socket is readable.  Something must be going very wrong.
	 */
	mx_log("S%u input with no channels, failed", msp->ms_id);
	mssp->mss_base.ms_state = MSS_FAILED;
	return TRUE;
    }
#endif

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

    /* Released sockets should not be making output */
    TAILQ_FOREACH(mcp, &mssp->mss_released, mc_link) {
	unsigned long read_avail = 0;
	libssh2_channel_window_read_ex(mcp->mc_channel, &read_avail, NULL);
	if (read_avail) {
	    DBG_POLL("C%u read avail on released channel: %lu",
		     mcp->mc_id, read_avail);
	    dead = mcp;
	}

	if (libssh2_channel_eof(mcp->mc_channel)) {
	    mx_log("C%u: disconnect, eof", mcp->mc_id);
	    dead = mcp;
	}
    }

    if (dead) {
	TAILQ_REMOVE(&mssp->mss_released, dead, mc_link);
	mx_channel_close(dead);
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

    char buf[MAXPATHLEN];

    buf[0] = '\0';
    getcwd(buf, sizeof(buf));
    known_hosts = strdupf("%s/.ssh/known_hosts", buf);

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
