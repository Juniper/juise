/*
 * $Id$
 *
 * Copyright (c) 2012, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * The mixer daemon will accept requests for SSH channels and will
 * service those requests using existing SSH connections where
 * possible, opening new connections as needed.  Incoming requests
 * can use the WebSockets format.
 */

#include "config.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
#include <stdarg.h>

#include <assert.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <libslax/slax.h>

#include <libssh2.h>

#include <libjuise/common/aux_types.h>
#include <libjuise/string/strextra.h>

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

#define LOCALHOST_ADDRESS "127.0.0.1"

#define MAXPOLL		10000	/* Largest number of open sockets to poll() */
#define MAXARGS		10	/* Max number of args to carve up */
#define INDENT 		4	/* Indentation increment */
#define BUFFER_DEFAULT_SIZE (4*1024)
#define POLL_TIMEOUT	30000	/* Poll() timeout */

#define MST_NONE	0	/* None/unknown */
#define MST_LISTENER	1	/* Listen for incoming connections */
#define MST_FORWARDER	2	/* Forward data to/from an ssh channel */
#define MST_SESSION	3	/* An ssh session */
#define MST_CONSOLE	4	/* Debug console shell */
#define MST_WEBSOCKET	5	/* Websocket client (in a browser) */

#define MST_MAX		5	/* max(MST_*) */

static unsigned mx_sock_id;   /* Monotonically increasing ID number */
static unsigned mx_channel_id; /* Monotonically increasing ID number */
static struct pollfd mx_pollfd[MAXPOLL];
static struct pollfd *mx_poll_owner[MAXPOLL];

static const char keydir[] = ".ssh";
static const char keybase1[] = "id_dsa.pub";
static const char keybase2[] = "id_dsa";

static char keyfile1[MAXPATHLEN], keyfile2[MAXPATHLEN];

static const char *opt_user;
static const char *opt_password;
static unsigned opt_verbose = FALSE;
static unsigned opt_debug;

/* Flags for opt_debug: */
#define DBG_FLAG_POLL	(1<<0)	/* poll()/poller related */

enum {
    AUTH_NONE = 0,
    AUTH_PASSWORD,
    AUTH_PUBLICKEY
};

typedef short poll_event_t;
typedef unsigned mx_type_t;	/* Type for MST_* */

struct mx_sock_s;		    /* Forward declarations */
struct mx_sock_forwarder_s;
struct mx_sock_session_s;

typedef TAILQ_ENTRY(mx_sock_s) mx_sock_link_t;
typedef TAILQ_HEAD(mx_sock_list_s, mx_sock_s) mx_sock_list_t;

typedef struct mx_buffer_s {
    struct mx_buffer_s *mb_next; /* Next buffer */
    unsigned long mb_start;	/* Offset of first data byte */
    unsigned long mb_len;	/* Number of bytes in use */
    unsigned long mb_size;	/* Number of bytes in the buffer */
    char mb_data[0];
} mx_buffer_t;

struct mx_channel_s;		    /* Forward declaration */
typedef TAILQ_ENTRY(mx_channel_s) mx_channel_link_t;
typedef TAILQ_HEAD(mx_channel_list_s, mx_channel_s) mx_channel_list_t;

typedef struct mx_channel_s {
    mx_channel_link_t mc_link;	/* List of channels */
    unsigned mc_id;
    struct mx_sock_session_s *mc_session; /* Session for this channel */
    LIBSSH2_CHANNEL *mc_channel; /* Our libssh2 channel */
    struct mx_sock_s *mc_forwarder; /* Our forwarder */
    mx_buffer_t *mc_rbufp;	/* Read buffer */
} mx_channel_t;

typedef struct mx_sock_s {
    mx_sock_link_t ms_link;	/* List of all open sockets */
    unsigned ms_id;		/* Socket identifier */
    mx_type_t ms_type;		/* MST_* type */
    unsigned ms_sock;		/* Underlaying sock */
    unsigned ms_state;		/* Type-specific state */
    struct sockaddr_in ms_sin;	/* Address of peer */
} mx_sock_t;

/* Values for ms_state */
#define MSS_NORMAL	0	/* Normal/okay/ignore */
#define MSS_FAILED	1	/* Failed; needs to be closed */
#define MSS_INPUT	2	/* Needs to read input */
#define MSS_OUTPUT	3	/* Needs to write output */

typedef struct mx_sock_listener_s {
    mx_sock_t msl_base;		/* Base sock info */
    unsigned msl_port;		/* Port being listened on */
    mx_type_t msl_spawns;	/* Type of socket spawned */
    char *msl_target;		/* Target session */
} mx_sock_listener_t;

typedef struct mx_sock_forwarder_s {
    mx_sock_t msf_base;
    struct mx_sock_session_s *msf_session; /* Master session */
    mx_channel_t *msf_channel;	   /* Our channel */
    mx_buffer_t *msf_rbufp;	   /* Read buffer */
} mx_sock_forwarder_t;

typedef struct mx_sock_session_s {
    mx_sock_t mss_base;
    char *mss_target;		  /* Remote host name (target) */
    LIBSSH2_SESSION *mss_session; /* libssh2 info */
    mx_channel_list_t mss_channels; /* Set of channels */
} mx_sock_session_t;

typedef struct mx_sock_websocket_s {
    mx_sock_t msw_base;
    mx_buffer_t *msw_rbufp;	   /* Read buffer */
} mx_sock_websocket_t;

mx_sock_list_t mx_sock_list;	/* List of all sockets */
int mx_sock_count;		/* Number of mx_sock_t in mx_sock_list */

#define MX_TYPE_PRINT_ARGS \
    mx_sock_t *msp UNUSED, int indent UNUSED, const char *prefix UNUSED
typedef void (*mx_type_print_func_t)(MX_TYPE_PRINT_ARGS);

#define MX_TYPE_PREP_ARGS \
    mx_sock_t *msp UNUSED, struct pollfd *pollp UNUSED, int *timeout UNUSED
typedef int (*mx_type_prep_func_t)(MX_TYPE_PREP_ARGS);

#define MX_TYPE_POLLER_ARGS \
    mx_sock_t *msp UNUSED, struct pollfd *pollp UNUSED
typedef int (*mx_type_poller_func_t)(MX_TYPE_POLLER_ARGS);

#define MX_TYPE_SPAWN_ARGS \
    mx_sock_listener_t *mslp UNUSED, \
    int sock UNUSED, struct sockaddr_in *sin UNUSED, socklen_t sinlen UNUSED
typedef mx_sock_t *(*mx_type_spawn_func_t)(MX_TYPE_SPAWN_ARGS);

#define MX_TYPE_WRITE_ARGS \
    mx_sock_t *msp UNUSED, mx_buffer_t *mbp UNUSED
typedef int (*mx_type_write_func_t)(MX_TYPE_WRITE_ARGS);

#define MX_TYPE_SET_CHANNEL_ARGS \
    mx_sock_t *msp UNUSED, mx_sock_session_t *mssp UNUSED, \
	mx_channel_t *mcp UNUSED
typedef void (*mx_type_set_channel_func_t)(MX_TYPE_SET_CHANNEL_ARGS);

typedef struct mx_type_info_s {
    mx_type_t mti_type;		/* MST_* */
    const char *mti_name;	/* Printable name */
    mx_type_print_func_t mti_print; /* Print function */
    mx_type_prep_func_t mti_prep; /* Prepare for poll() data */
    mx_type_poller_func_t mti_poller; /* Process poll() data */
    mx_type_spawn_func_t mti_spawn; /* Spawn a new sock from a listener */
    mx_type_write_func_t mti_write; /* Write a buffer of data */
    mx_type_set_channel_func_t mti_set_channel; /* Set session/channel */
} mx_type_info_t;

mx_type_info_t mx_type_info[MST_MAX + 1];

static const char *remote_desthost = "localhost"; /* resolved by the server */
static unsigned int remote_destport = 22;

static mx_channel_t *
mx_channel_create (const char *target, mx_sock_t *forwarder);


#define MX_LOG(_fmt...) \
    do { if (opt_debug || opt_verbose) mx_log(_fmt); } while (0)

#define DBG_FLAG(_flag, _fmt...) \
    do { if (opt_debug & _flag) mx_log(_fmt); } while(0)
#define DBG_POLL(_fmt...) DBG_FLAG(DBG_FLAG_POLL, _fmt)

static void
#ifdef HAVE_PRINTFLIKE
__printflike(1, 2)
#endif /* HAVE_PRINTFLIKE */
mx_log (const char *fmt, ...)
{
    va_list vap;
    int len = strlen(fmt);
    char *cfmt = alloca(len + 2);

    memcpy(cfmt, fmt, len);
    cfmt[len] = '\n';
    cfmt[len + 1] = '\0';

    va_start(vap, fmt);
    vfprintf(stderr, cfmt, vap);
    va_end(vap);
}

static void
mx_log_callback (void *opaque UNUSED, const char *fmt, va_list vap)
{
    int len = strlen(fmt);
    char *cfmt = alloca(len + 2);

    memcpy(cfmt, fmt, len);
    cfmt[len] = '\n';
    cfmt[len + 1] = '\0';

    vfprintf(stderr, cfmt, vap);
}

static const char *
mx_sock_sin (mx_sock_t *msp)
{
    #define NUM_BUFS 3
    static unsigned buf_num;
    static char bufs[NUM_BUFS][BUFSIZ];
    const char *shost = inet_ntoa(msp->ms_sin.sin_addr);
    unsigned int sport = ntohs(msp->ms_sin.sin_port);

    if (++buf_num >= NUM_BUFS)
	buf_num = 0;

    snprintf(bufs[buf_num], BUFSIZ, "%s:%d", shost, sport);

    return bufs[buf_num];
}

static mx_sock_t *
mx_console_start (void)
{
    mx_sock_t *msp = malloc(sizeof(*msp));
    if (msp == NULL)
	return NULL;

    bzero(msp, sizeof(*msp));
    msp->ms_id = ++mx_sock_id;
    msp->ms_type = MST_CONSOLE;
    msp->ms_sock = 0;		/* fileno(stdin) */

    TAILQ_INSERT_HEAD(&mx_sock_list, msp, ms_link);
    mx_sock_count += 1;

    fprintf(stderr, ">> ");
    fflush(stderr);

    return msp;
}

/**
 * Split the input given by users into tokens
 * @buf buffer to split
 * @args array for args
 * @maxargs length of args
 * @return number of args
 * @bug needs to handle escapes and quotes
 */
static int
mx_console_split_args (char *buf, const char **args, int maxargs)
{
    static const char wsp[] = " \t\n\r";
    int i;
    char *s;

    for (i = 0; (s = strsep(&buf, wsp)) && i < maxargs - 1; i++) {
	args[i] = s;

	if (buf)
	    buf += strspn(buf, wsp);
    }

    args[i] = NULL;

    return i;
}

static int
strabbrev (const char *name, const char *value)
{
    int min = 1;
    int len = strlen(value);
    return (len >= min && strncmp(name, value, len) == 0);
}

static mx_type_info_t *
mx_mti_number (mx_type_t type)
{
    static mx_type_info_t mti_null = { mti_name: "null/zero/nada" };

    if (type > MST_MAX)
	return &mti_null;

    return &mx_type_info[type];
}

static mx_type_info_t *
mx_mti (mx_sock_t *msp)
{
    return mx_mti_number(msp->ms_type);
}

static const char *
mx_sock_type_number (mx_type_t type)
{
    if (type > MST_MAX)
	return "unknown";
    if (mx_type_info[type].mti_name)
	return mx_type_info[type].mti_name;
    return "unknown";
}

static const char *
mx_sock_type (mx_sock_t *msp)
{
    if (msp == NULL)
	return "null";
    return mx_sock_type_number(msp->ms_type);
}

static void *
mx_sock (mx_sock_t *msp, mx_type_t type)
{
    assert(msp);
    assert(msp->ms_type == type);

    return msp;
}

static mx_buffer_t *
mx_buffer_create (unsigned size)
{
    if (size == 0)
	size = BUFFER_DEFAULT_SIZE;

    mx_buffer_t *mbp = malloc(sizeof(*mbp) + size);
    if (mbp == NULL)
	return NULL;

    bzero(mbp, sizeof(*mbp));
    mbp->mb_size = size;

    return mbp;
}

static void
mx_buffer_free (mx_buffer_t *mbp)
{
    free(mbp);
}

static void
mx_listener_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);

    mx_log("%*s%sport %u, spawns %s to %s", indent, "", prefix,
	   mslp->msl_port, mx_sock_type_number(mslp->msl_spawns),
	   mslp->msl_target);
}

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

static void
mx_session_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_session_t *mssp = mx_sock(msp, MST_SESSION);
    mx_channel_t *mcp;

    mx_log("%*s%starget %s, session %p", indent, "", prefix,
	   mssp->mss_target,
	   mssp->mss_session);

    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {
	mx_buffer_t *mbp = mcp->mc_rbufp;
	unsigned long read_avail = 0;

	libssh2_channel_window_read_ex(mcp->mc_channel,
				       &read_avail, NULL);

	mx_log("%*sC%u: S%u, channel %p, forwarder S%u, rb %lu/%lu, avail %lu",
	       indent + INDENT, "",
	       mcp->mc_id, mcp->mc_session->mss_base.ms_id,
	       mcp->mc_channel, mcp->mc_forwarder->ms_id,
	       mbp->mb_start, mbp->mb_len, read_avail);

    }
}

static void
mx_console_list (const char **argv UNUSED)
{
    mx_sock_t *msp;
    const char *shost = NULL;
    unsigned int sport = 0;
    int i = 0;
    int indent = INDENT;

    mx_log("%d open sockets:", mx_sock_count);

    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {

	mx_log("%*sS%u: %s, fd %u, state %u", indent, "",
		msp->ms_id, mx_sock_type(msp), msp->ms_sock, msp->ms_state);
	if (msp->ms_sin.sin_port) {
	    shost = inet_ntoa(msp->ms_sin.sin_addr);
	    sport = ntohs(msp->ms_sin.sin_port);
	    mx_log("\t(%s .. %d)", shost, sport);
	}

	if (mx_mti(msp)->mti_print)
	    mx_mti(msp)->mti_print(msp, indent + INDENT, "");

	i += 1;
    }
}

static void
mx_debug_print_help (const char *cp UNUSED)
{
    mx_log("Flags: %0x", opt_debug);
}

static unsigned
mx_debug_parse_flag (const char *cp)
{
    unsigned rc = 0;

    if (cp == NULL || *cp == '\0') {
	mx_debug_print_help(NULL);
	return 0;
    }

    if (streq(cp, "poll"))
	rc |= DBG_FLAG_POLL;

    if (streq(cp, "all"))
	rc |= - 1;

    return rc;
}

static int
mx_console_poller (MX_TYPE_POLLER_ARGS)
{
    if (pollp && pollp->revents & POLLIN) {
	char buf[BUFSIZ];

	if (fgets(buf, sizeof(buf), stdin)) {
	    const char *argv[MAXARGS], *cp;

	    mx_console_split_args(buf, argv, MAXARGS);
	    cp = argv[0];
	    if (cp && *cp) {
		if (strabbrev("quit", cp)) {
		    exit(1);

		} else if (strabbrev("list", cp) || strabbrev("ls", cp)) {
		    mx_console_list(argv);

		} else if (strabbrev("don", cp)) {
		    opt_debug |= mx_debug_parse_flag(argv[1]);

		} else if (strabbrev("doff", cp)) {
		    opt_debug &= ~mx_debug_parse_flag(argv[1]);

		} else {
		    mx_log("%s: command not found", cp);
		}
	    }

	    fprintf(stderr, ">> ");
	    fflush(stderr);
	}
    }

    return FALSE;
}

static mx_sock_t *
mx_listener (unsigned port, mx_type_t type, int spawns, const char *target)
{
    struct sockaddr_in sin;
    socklen_t sinlen;

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0)
	return NULL;

    int sockopt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(LOCALHOST_ADDRESS);
    if (sin.sin_addr.s_addr == INADDR_NONE) {
        mx_log("listener port %u: inet_addr: %s", port, strerror(errno));
	close(sock);
	return NULL;
    }

    sinlen = sizeof(sin);
    if (bind(sock, (struct sockaddr *) &sin, sinlen) < 0) {
        mx_log("listener port %u: bind: %s", port, strerror(errno));
	close(sock);
	return NULL;
    }

    if (listen(sock, 5) < 0) {
        mx_log("listener port %u: listen: %s", port, strerror(errno));
	close(sock);
	return NULL;
    }

    mx_sock_listener_t *mslp = malloc(sizeof(*mslp));
    if (mslp == NULL)
	return NULL;

    bzero(mslp, sizeof(*mslp));
    mslp->msl_base.ms_id = ++mx_sock_id;
    mslp->msl_base.ms_type = type;
    mslp->msl_base.ms_sock = sock;
    mslp->msl_port = port;
    mslp->msl_spawns = spawns;
    mslp->msl_target = strdup(target);

    TAILQ_INSERT_HEAD(&mx_sock_list, &mslp->msl_base, ms_link);
    mx_sock_count += 1;

    MX_LOG("S%u new listener, fd %u, spawns %s, port %s:%d...",
	   mslp->msl_base.ms_id, mslp->msl_base.ms_sock,
	   mx_sock_type_number(mslp->msl_spawns),
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    return &mslp->msl_base;
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
    msfp->msf_base.ms_sin = *sin;

    msfp->msf_rbufp = mx_buffer_create(0);

    mx_channel_t *mcp;
    mcp = mx_channel_create(mslp->msl_target, &msfp->msf_base);
    if (mcp == NULL) {
	mx_log("could not open channel");
	/* XXX close msfp */
	return NULL;
    }

    return &msfp->msf_base;
}

static void
mx_nonblocking (int sock)
{
#if defined(O_NONBLOCK)
    int flags = fcntl(sock, F_GETFL, 0);

    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#elif defined(FIONBIO)
    int flags = 1;

    ioctl(sock, FIONBIO, &flags);
#endif
}

static mx_sock_t *
mx_listener_accept (mx_sock_listener_t *listener)
{
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);

    int sock = accept(listener->msl_base.ms_sock,
		      (struct sockaddr *) &sin, &sinlen);
    if (sock < 0) {
        mx_log("S%u: accept: %s", listener->msl_base.ms_id, strerror(errno));
	return NULL;
    }

    mx_nonblocking(sock);

    assert(mx_mti_number(listener->msl_spawns)->mti_spawn);

    mx_sock_t *msp;
    msp = mx_mti_number(listener->msl_spawns)->mti_spawn(listener, sock,
							 &sin, sinlen);
    if (msp == NULL)
	return NULL;

    TAILQ_INSERT_HEAD(&mx_sock_list, msp, ms_link);
    mx_sock_count += 1;

    MX_LOG("S%u new %s, fd %u, port %s:%d...",
	   msp->ms_id, mx_sock_type(msp), msp->ms_sock,
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    return msp;
}

static mx_sock_session_t *
mx_session (LIBSSH2_SESSION *session, int sock, const char *target)

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

    TAILQ_INSERT_HEAD(&mx_sock_list, &mssp->mss_base, ms_link);
    mx_sock_count += 1;

    MX_LOG("S%u new %s, fd %u, target %s",
	   mssp->mss_base.ms_id, mx_sock_type(&mssp->mss_base),
	   mssp->mss_base.ms_sock, mssp->mss_target);

    return mssp;
}

static mx_sock_session_t *
mx_session_open (const char *target)
{
    int rc, sock = -1, i, auth = AUTH_NONE;
    mx_sock_session_t *mssp;
    struct sockaddr_in sin;
    const char *fingerprint;
    char *userauthlist;
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

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    fprintf(stderr, "Fingerprint: ");
    for (i = 0; i < 20; i++)
        fprintf(stderr, "%02X ", (unsigned char) fingerprint[i]);
    fprintf(stderr, "\n");

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, opt_user, strlen(opt_user));
    mx_log("Authentication methods: %s", userauthlist);

    if (strstr(userauthlist, "password"))
        auth |= AUTH_PASSWORD;
    if (strstr(userauthlist, "publickey"))
        auth |= AUTH_PUBLICKEY;

    LIBSSH2_AGENT *agent = NULL;
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;

    if (auth & AUTH_PUBLICKEY) {
	if (opt_password && *opt_password) {

	    if (libssh2_userauth_publickey_fromfile(session, opt_user,
						    keyfile1, keyfile2,
						    opt_password)) {
		mx_log("authentication by public key failed");
		return NULL;
	    }
	    mx_log("Authentication by public key succeeded");

	} else {
	    /* Connect to the ssh-agent */
	    agent = libssh2_agent_init(session);
	    if (!agent) {
		mx_log("failure initializing ssh-agent support");
		return NULL;
	    }

	    if (libssh2_agent_connect(agent)) {
		mx_log("failure connecting to ssh-agent");
		return NULL;
	    }

	    if (libssh2_agent_list_identities(agent)) {
		mx_log("failure requesting identities to ssh-agent");
		return NULL;
	    }

	    for (;;) {
		rc = libssh2_agent_get_identity(agent, &identity, prev_identity);
		if (rc == 1)
		    break;

		if (rc < 0) {
		    mx_log("Failure obtaining identity from ssh-agent");
		    return NULL;
		}

		if (libssh2_agent_userauth(agent, opt_user, identity)) {
		    mx_log("authentication with username %s and "
			   "public key %s failed",
			   opt_user, identity->comment);
		} else {
		    mx_log("authentication with username %s and "
			   "public key %s succeeded",
			   opt_user, identity->comment);
		    break;
		}
		prev_identity = identity;
	    }

	    if (rc) {
		mx_log("Couldn't continue authentication");
		return NULL;
	    }

	    /* We're authenticated now. */
	}

    } else if (auth & AUTH_PASSWORD) {
        if (libssh2_userauth_password(session, opt_user, opt_password)) {
            mx_log("authentication by password failed");
            return NULL;
        }
    } else {
        mx_log("no supported authentication methods found");
        return NULL;
    }

    /* Must use non-blocking IO hereafter due to the current libssh2 API */
    libssh2_session_set_blocking(session, 0);

    mssp = mx_session(session, sock, target);
    if (mssp == NULL) {
	mx_log("mx session failed");
	return NULL;
    }

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

static mx_channel_t *
mx_channel (mx_sock_t *forwarder, mx_sock_session_t *session)
{
    LIBSSH2_CHANNEL *channel;
    const char *shost = inet_ntoa(forwarder->ms_sin.sin_addr);
    unsigned int sport = ntohs(forwarder->ms_sin.sin_port);

    channel = libssh2_channel_direct_tcpip_ex(session->mss_session,
					      remote_desthost, remote_destport,
					      shost, sport);
    if (!channel) {
        mx_log("could not open the direct-tcpip channel");
	return NULL;
    }

    mx_channel_t *mcp = malloc(sizeof(*mcp));
    if (mcp == NULL)
	return NULL;

    bzero(mcp, sizeof(*mcp));
    mcp->mc_id = ++mx_channel_id;
    mcp->mc_session = session;
    mcp->mc_channel = channel;
    mcp->mc_rbufp = mx_buffer_create(0);

    assert(mx_mti(forwarder)->mti_set_channel);
    mx_mti(forwarder)->mti_set_channel(forwarder, session, mcp);
    mcp->mc_forwarder = forwarder;

    TAILQ_INSERT_HEAD(&session->mss_channels, mcp, mc_link);

    MX_LOG("C%u: new channel, S%u, channel %p, forwarder S%u",
	   mcp->mc_id, mcp->mc_session->mss_base.ms_id,
	   mcp->mc_channel, mcp->mc_forwarder->ms_id);

    return mcp;
}

static mx_channel_t *
mx_channel_create (const char *target, mx_sock_t *forwarder)
{
    mx_sock_session_t *session = mx_session_find(target);
    mx_channel_t *mcp;

    if (session == NULL) {
	session = mx_session_open(target);
	if (session == NULL)
	    return NULL;
    }

    /* Must use blocking IO for channel creation */
    libssh2_session_set_blocking(session->mss_session, 1);

    mcp = mx_channel(forwarder, session);

    libssh2_session_set_blocking(session->mss_session, 0);

    return mcp;
}

static void
mx_channel_close (mx_channel_t *mcp)
{
    if (mcp == NULL)
	return;

    if (mcp->mc_forwarder) {
	mx_sock_t *msp = mcp->mc_forwarder;
	assert(mx_mti(msp)->mti_set_channel);
	mx_mti(msp)->mti_set_channel(msp, NULL, NULL);
    }

    libssh2_channel_free(mcp->mc_channel);
    free(mcp);
}

static void
mx_listener_close (mx_sock_t *msp)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);

    free(mslp->msl_target);
}

static void
mx_forwarder_close (mx_sock_t *msp)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);

    mx_buffer_free(msfp->msf_rbufp);
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

static void
mx_sock_close (mx_sock_t *msp)
{
    if (msp == NULL)
	return;

    switch (msp->ms_type) {
    case MST_LISTENER:
	mx_listener_close(msp);
	break;

    case MST_FORWARDER:
	mx_forwarder_close(msp);
	break;

    case MST_SESSION:
	mx_session_close(msp);
	break;
    }

    if (msp->ms_sock)
	close(msp->ms_sock);
    TAILQ_REMOVE(&mx_sock_list, msp, ms_link);
    mx_sock_count -= 1;

    free(msp);
}

static int
mx_listener_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);
    const char *shost;
    unsigned int sport;

    if (pollp && pollp->revents & POLLIN) {
	mx_sock_t *newp = mx_listener_accept(mslp);
	if (newp == NULL) {
	    mx_log("S%u: accept: %s", msp->ms_id, strerror(errno));
	    return TRUE;
	}

	shost = inet_ntoa(newp->ms_sin.sin_addr);
	sport = ntohs(newp->ms_sin.sin_port);

	mx_log("new connection from %s here to remote %s:%d",
	       mx_sock_sin(newp), remote_desthost, remote_destport);
    }

    return FALSE;
}

static void
mx_channel_write (mx_channel_t *mcp, mx_buffer_t *mbp)
{
    int len = mbp->mb_len, rc;

    do {
	rc = libssh2_channel_write(mcp->mc_channel,
				   mbp->mb_data + mbp->mb_start,
				   len - mbp->mb_start);
	if (rc < 0) {
	    if (rc == LIBSSH2_ERROR_EAGAIN)
		return;

	    mx_log("C%u: libssh2_channel_write: %d", mcp->mc_id, rc);
	    /* XXX recovery/close? */
	    return;
	}

	mbp->mb_start += rc;
    } while (mbp->mb_start < (unsigned) len);

    mbp->mb_start = mbp->mb_len = 0; /* Reset */
}

static int
mx_channel_sock (mx_channel_t *mcp)
{
    return mcp->mc_session->mss_base.ms_sock;
}

static int
mx_channel_has_buffered (mx_channel_t *mcp)
{
    return (mcp->mc_rbufp->mb_len != 0);
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
	DBG_POLL("S%u blocking pollout for fd %d", msp->ms_id, pollp->fd);

    } else if (mx_channel_has_buffered(msfp->msf_channel)) {
	pollp->fd = mx_channel_sock(msfp->msf_channel);
	pollp->events = POLLOUT;
	DBG_POLL("S%u blocking pollout for fd %d (channel)",
		 msp->ms_id, pollp->fd);

    } else {
	pollp->fd = msp->ms_sock;
	pollp->events = POLLIN;
	DBG_POLL("S%u blocking pollin for fd %d", msp->ms_id, pollp->fd);
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
		    mx_log("S%u: read error: %s", msp->ms_id, strerror(errno));
		    msp->ms_state = MSS_FAILED;
		    return TRUE;
		}

	    } else if (len == 0) {
		mx_log("S%u: disconnect (%s)", msp->ms_id, mx_sock_sin(msp));
		return TRUE;
	    } else {
		mbp->mb_len = len;
	    }
	}
    }

    if (mbp->mb_len)
	mx_channel_write(msfp->msf_channel, mbp);

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
		  mbp->mb_data + mbp->mb_start, len - mbp->mb_start, 0);
	if (rc <= 0) {
	    if (errno != EWOULDBLOCK) {
		mx_log("S%u: write error: %s",
		       msfp->msf_base.ms_id, strerror(errno));
		msfp->msf_base.ms_state = MSS_FAILED;
	    }

	    return TRUE;
	}

	mbp->mb_start += rc;
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
mx_sock_isreadable (int sock)
{
    struct pollfd pfd;
    int rc;

    bzero(&pfd, sizeof(pfd));
    pfd.fd = sock;
    pfd.events = POLLIN;

    for (;;) {
	rc = poll(&pfd, 1, 0);
        if (rc < 0) {
	    if (errno == EINTR)
		continue;
	    return FALSE;
	}
	return (pfd.revents & POLLIN) ? TRUE : FALSE;
    }
}

static int
mx_forwarder_is_buf (mx_sock_t *msp, int flags UNUSED)
{
    mx_sock_forwarder_t *msfp = mx_sock(msp, MST_FORWARDER);
    return (msfp->msf_rbufp->mb_len != 0);
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
	    if (mx_forwarder_is_buf(mcp->mc_forwarder, POLLOUT)) {
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
    int len;

    DBG_POLL("S%u processing (%p/0x%x) readable %s",
	     msp->ms_id, pollp, pollp ? pollp->revents : 0,
	     mx_sock_isreadable(msp->ms_sock) ? "yes" : "no");

    TAILQ_FOREACH(mcp, &mssp->mss_channels, mc_link) {
	mx_buffer_t *mbp = mcp->mc_rbufp;

	for (;;) {
	    if (mbp->mb_len == 0) { /* Nothing buffered */
		len = libssh2_channel_read(mcp->mc_channel,
					   mbp->mb_data, mbp->mb_size);
		if (len == LIBSSH2_ERROR_EAGAIN) {
		    /* Nothing to read, nothing to write; move on */
		    DBG_POLL("C%u is drained", mcp->mc_id);
		    break;

		} else if (len < 0) {
		    mx_log("libssh2_channel_read: %d", (int) len);
		    return TRUE;

		} else {
		    mbp->mb_len = len;
		    DBG_POLL("C%u read %d", mcp->mc_id, len);
		}
	    }

	    /*
	     * If the write call would block (returns TRUE), then
	     * we move on.
	     */
	    assert(mx_mti(mcp->mc_forwarder)->mti_write);
	    if (mx_mti(mcp->mc_forwarder)->mti_write(mcp->mc_forwarder, mbp))
		break;
	}

	if (libssh2_channel_eof(mcp->mc_channel)) {
	    mx_log("C%u: disconnect (%s:%d) eof",
		   mcp->mc_id, remote_desthost, remote_destport);
	    return TRUE;
	}
    }

    DBG_POLL("S%u done, readable %s", msp->ms_id,
	     mx_sock_isreadable(msp->ms_sock) ? "yes" : "no");

    return FALSE;
}

static void
main_loop (void)
{
    mx_sock_t *msp;
    int waiting = FALSE;
    int rc;

    for (;;) {
	struct pollfd *pollp;
	int nfd = 0;
	int mindex = -1;
	int timeout = POLL_TIMEOUT;

	assert(mx_sock_count < MAXPOLL);

	bzero(&mx_pollfd, sizeof(mx_pollfd[0]) * mx_sock_count);
	bzero(&mx_poll_owner, sizeof(mx_poll_owner[0]) * mx_sock_count);

	TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	    mindex += 1;

	    if (mx_mti(msp)->mti_prep) {
		if (mx_mti(msp)->mti_prep(msp, &mx_pollfd[nfd], &timeout)) {
		    mx_pollfd[nfd].revents = 0;
		    mx_poll_owner[mindex] = &mx_pollfd[nfd];
		    nfd += 1;
		}
	    } else {
		mx_pollfd[nfd].fd = msp->ms_sock;
		mx_pollfd[nfd].events = POLLIN;
		mx_pollfd[nfd].revents = 0;
		mx_poll_owner[mindex] = &mx_pollfd[nfd];
		nfd += 1;
	    }

	    if (msp->ms_state == MSS_FAILED)
		goto failure;
	}

	if (opt_debug & DBG_FLAG_POLL) {
	    mx_log("poll: nfd %d, timeout %d", nfd, timeout);
	    int i;
	    for (i = 0; i < nfd; i++) {
		mx_log("    %d: fd %d%s%s", i, mx_pollfd[i].fd,
		       (mx_pollfd[i].events & POLLIN) ? " pollin" : "",
		       (mx_pollfd[i].events & POLLOUT) ? " pollout" : "");
	    }
	}

	struct timeval tv_begin, tv_end;
	gettimeofday(&tv_begin, NULL);

	rc = poll(mx_pollfd, nfd, timeout);
        if (rc < 0) {
	    if (errno == EINTR)
		continue;
	    mx_log("poll: %s", strerror(errno));
            goto shutdown;
        }

	gettimeofday(&tv_end, NULL);
	unsigned long delta = (tv_end.tv_sec - tv_begin.tv_sec) * 1000;
	delta += (tv_end.tv_usec - tv_begin.tv_usec) / 1000;

	if (opt_debug & DBG_FLAG_POLL) {
	    if (delta > 20000)
		mx_log("poll: long wait (%lu)", delta);
	    mx_log("poll: nfd %d, timeout %d", nfd, timeout);
	    int i;
	    for (i = 0; i < nfd; i++) {
		mx_log("    %d: fd %d%s%s", i, mx_pollfd[i].fd,
		       (mx_pollfd[i].revents & POLLIN) ? " pollin" : "",
		       (mx_pollfd[i].revents & POLLOUT) ? " pollout" : "");
	    }
	}

	waiting = FALSE;

	mindex = -1;
	TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	    mindex += 1;

	    pollp = mx_poll_owner[mindex];

	    if (pollp && pollp->revents & POLLNVAL) {
		mx_log("invalid poll entry");
		goto failure;
	    }

	    if (mx_mti(msp)->mti_poller)
		if (mx_mti(msp)->mti_poller(msp, pollp))
		    goto failure;

	    if (msp->ms_state == MSS_FAILED)
		goto failure;
	}
	continue;

    failure:
	mx_sock_close(msp);
    }

shutdown:
    TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	mx_sock_close(msp);
    }
}

static void
print_help (const char *opt)
{
    if (opt)
	fprintf(stderr, "invalid option: %s\n\n", opt);

    fprintf(stderr, "syntax: mixer [options]\n");
    exit(1);
}

#define MX_TYPE_INFO_VERSION 1

static void
mx_type_info_register (int version UNUSED, mx_type_info_t *mtip)
{
    mx_log("type: %s (%d)", mtip->mti_name, mtip->mti_type);
    
    mx_type_info[mtip->mti_type] = *mtip;
}

static void
mx_listener_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_LISTENER,
    mti_name: "listener",
    mti_print: mx_listener_print,
    mti_poller: mx_listener_poller,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

static void
mx_forwarder_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_FORWARDER,
    mti_name: "forwarder",
    mti_print: mx_forwarder_print,
    mti_prep: mx_forwarder_prep,
    mti_poller: mx_forwarder_poller,
    mti_spawn: mx_forwarder_spawn,
    mti_write: mx_forwarder_write,
    mti_set_channel: mx_forwarder_set_channel,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

static void
mx_session_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_SESSION,
    mti_name: "session",
    mti_print: mx_session_print,
    mti_prep: mx_session_prep,
    mti_poller: mx_session_poller,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

static void
mx_console_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_CONSOLE,
    mti_name: "console",
    mti_poller: mx_console_poller,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

static void
mx_websocket_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_websocket_t *mswp = mx_sock(msp, MST_WEBSOCKET);
    mx_buffer_t *mbp = mswp->msw_rbufp;

    mx_log("%*s%srb %lu/%lu",
	   indent, "", prefix,
	   mbp->mb_start, mbp->mb_len);
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
		if (errno != EWOULDBLOCK) {
		    mx_log("S%u: read error: %s", msp->ms_id, strerror(errno));
		    msp->ms_state = MSS_FAILED;
		    return TRUE;
		}

	    } else if (len == 0) {
		mx_log("S%u: disconnect (%s)", msp->ms_id, mx_sock_sin(msp));
		return TRUE;
	    } else {
		mbp->mb_len = len;
	    }

	    slaxMemDump("wsread: ", mbp->mb_data, mbp->mb_len, ">", 0);
	    mbp->mb_start = mbp->mb_len = 0;
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
mx_websocket_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_WEBSOCKET,
    mti_name: "websocket",
    mti_print: mx_websocket_print,
    mti_prep: mx_websocket_prep,
    mti_poller: mx_websocket_poller,
    mti_spawn: mx_websocket_spawn,
#if 0
    mti_write: mx_websocket_write,
    mti_set_channel: mx_websocket_set_channel,
#endif
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}

static void
mx_type_info_init (void)
{
    mx_listener_init();
    mx_forwarder_init();
    mx_session_init();
    mx_console_init();
    mx_websocket_init();
}

int
main (int argc UNUSED, char **argv UNUSED)
{
    char *cp;
    int rc;
    int opt_getpass = FALSE, opt_console = FALSE;
    char *opt_home = NULL;

    for (argv++; *argv; argv++) {
	cp = *argv;
	if (*cp != '-')
	    break;

	if (streq(cp, "--prompt-for-password") || streq(cp, "-p")) {
	    opt_getpass = TRUE;

	} else if (streq(cp, "--password")) {
	    opt_password = *++argv;

	} else if (streq(cp, "--console") || streq(cp, "-c")) {
	    opt_console = TRUE;

	} else if (streq(cp, "--user") || streq(cp, "-u")) {
	    opt_user = *++argv;

	} else if (streq(cp, "--home")) {
	    opt_home = *++argv;

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    opt_verbose = TRUE;

	} else if (streq(cp, "--debug")) {
	    cp  = *++argv;
	    if (cp == NULL)
		print_help(NULL);
	    opt_debug |= mx_debug_parse_flag(cp);

	} else if (streq(cp, "--help") || streq(cp, "-h")) {
	    print_help(NULL);

	} else {
	    print_help(cp);
	}
    }

    slaxLogEnableCallback(mx_log_callback, NULL);

    if (opt_verbose)
	slaxLogEnable(TRUE);

    if (opt_home == NULL)
	opt_home = getenv("HOME");

    snprintf(keyfile1, sizeof(keyfile1), "%s/%s/%s",
	     opt_home, keydir, keybase1);
    snprintf(keyfile2, sizeof(keyfile2), "%s/%s/%s",
	     opt_home, keydir, keybase2);

    if (opt_user == NULL)
	opt_user = strdup(getlogin());

    if (opt_getpass)
	opt_password = strdup(getpass("Password:"));

    mx_type_info_init();

    rc = libssh2_init (0);
    if (rc != 0) {
        mx_log("libssh2 initialization failed (%d)", rc);
        return 1;
    }

    if (opt_console)
	mx_console_start();

    if (mx_listener(2222, MST_LISTENER, MST_FORWARDER, "alice") == NULL) {
	mx_log("initial listen failed");
    }

    if (mx_listener(3333, MST_LISTENER, MST_FORWARDER, "bob") == NULL) {
	mx_log("initial listen failed");
    }

    if (mx_listener(8000, MST_LISTENER, MST_WEBSOCKET, "carl") == NULL) {
	mx_log("initial listen failed");
    }

    main_loop();

    libssh2_exit();

    return 0;
}
