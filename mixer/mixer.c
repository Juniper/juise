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

/*
 * Mixer using mx_sock_t as a base type around which specific types
 * are created.  Each type is identified by the ms_id (MST_*) value,
 * which is also used as the index into the mx_type_info table, where
 * type-specific function handle the details of each socket type.
 *
 * MST_LISTENER: listens for an incoming connection and spawns a second
 * type of socket.
 *
 * MST_FORWARDER: forwards content from a local socket over a ssh
 * channel.
 *
 * MST_SESSION: an SSH session, wrapping the libssh2 connection.
 *
 * MST_CONSOLE: a debug console.
 *
 * MST_WEBSOCKET: a WebSocket connection to a browser.
 */

#include "local.h"
#include "listener.h"
#include "forwarder.h"
#include "session.h"
#include "console.h"
#include "db.h"
#include "websocket.h"
#include "request.h"
#include <signal.h>
#include <err.h>

unsigned mx_sock_id;   /* Monotonically increasing ID number */

static struct pollfd mx_pollfd[MAX_POLL];
static struct pollfd *mx_poll_owner[MAX_POLL];

static const char keydir[] = ".ssh";
static const char keybase1[] = "id_dsa.pub";
static const char keybase2[] = "id_dsa";

/* Filenames of SSH private and public key files (in ~/.ssh/) */
char keyfile1[MAXPATHLEN], keyfile2[MAXPATHLEN];

const char *opt_user;		/* User name (if not getlogin()) */
const char *opt_password;
const char *opt_db = NULL;
const char *opt_desthost = "localhost";

unsigned opt_destport = 22;
int opt_no_agent;
int opt_no_known_hosts;
int opt_no_db;
int opt_keepalive;
int opt_knownhosts;

static int opt_login;
static int opt_fork;

static mx_password_t *mx_saved_passwords;

mx_password_t *
mx_password_find (const char *target, const char *user)
{
    mx_password_t *mpp;

    for (mpp = mx_saved_passwords; mpp; mpp = mpp->mp_next) {
	if (!streq(target, mpp->mp_target))
	    continue;
	if ((user == NULL && mpp->mp_user == NULL)
	    || streq(user, mpp->mp_user)) {
	    mpp->mp_laststamp = time(NULL);
	    return mpp;
	}
    }

    return NULL;
}

const char *
mx_password (const char *target, const char *user)
{
    mx_password_t *mpp = mx_password_find(target, user);

    return mpp ? mpp->mp_password : NULL;
}

mx_password_t *
mx_password_save (const char *target, const char *user, const char *password)
{
    mx_password_t *mpp = calloc(1, sizeof(*mpp));

    mpp->mp_target = strdup(target);
    mpp->mp_user = strdup(user);
    mpp->mp_password = strdup(password);

    mpp->mp_next = mx_saved_passwords;
    mx_saved_passwords = mpp;

    return mpp;
}

void
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

void
mx_sock_close (mx_sock_t *msp)
{
    if (msp == NULL)
	return;

    mx_log("%s close (%u)", mx_sock_title(msp), msp->ms_state);

    if (mx_mti(msp)->mti_close)
	mx_mti(msp)->mti_close(msp);

    else if (msp->ms_sock > 0)
	close(msp->ms_sock);

    TAILQ_REMOVE(&mx_sock_list, msp, ms_link);
    mx_sock_count -= 1;

    free(msp);
}

/*
 * main_loop: event loop for mixer
 *
 * libssh2 lacks a select() or poll() compatible interface.  There
 * are deprecated functions (libssh2_poll_channel_read and libssh2_poll())
 * and some googlework shows there are known bugs in them.
 *
 * So we roll our own.  There are two parts, the prep function and the
 * poller.  The prep function sets up a poll() data structures before
 * we call poll().  The poller function processes whatever data the is
 * present.  Each socket type can define their own prep and poller
 * functions.  If the prep function returns TRUE, it should fill in
 * the poll struct and the timeout value.
 */
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

	assert(mx_sock_count < MAX_POLL);

	bzero(&mx_pollfd, sizeof(mx_pollfd[0]) * mx_sock_count);
	bzero(&mx_poll_owner, sizeof(mx_poll_owner[0]) * mx_sock_count);

	TAILQ_FOREACH(msp, &mx_sock_list, ms_link) {
	    mindex += 1;

	    if (mx_mti(msp)->mti_prep == NULL
                || mx_mti(msp)->mti_prep(msp, &mx_pollfd[nfd], &timeout)) {
		mx_pollfd[nfd].fd = msp->ms_sock;
		mx_pollfd[nfd].events = POLLIN;
		mx_pollfd[nfd].revents = 0;
		mx_poll_owner[mindex] = &mx_pollfd[nfd];

                if (opt_debug & DBG_FLAG_POLL)
                    mx_log("  prep: %d: fd %d %s %s %x (%s%s)",
                         nfd, mx_pollfd[nfd].fd, mx_sock_title(msp),
                         mx_sock_type(msp), mx_pollfd[nfd].events,
                         (mx_pollfd[nfd].events & POLLIN) ? " pollin" : "",
                         (mx_pollfd[nfd].events & POLLOUT) ? " pollout" : "");

		nfd += 1;
	    }

	    if (msp->ms_state == MSS_FAILED)
		goto failure;
	}

        DBG_POLL("poll<: nfd %d, timeout %d", nfd, timeout);

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
	    int i;
	    mx_log("poll: rc %d, delta %lu%s", rc, delta,
                   (delta > 20000) ? " long wait" : "");
	    for (i = 0; i < nfd; i++) {
		mx_log("  post %d: fd %d %s %x (%s%s%s)", i, mx_pollfd[i].fd,
                       mx_sock_type(msp), mx_pollfd[i].revents,
		       (mx_pollfd[i].revents & POLLIN) ? " pollin" : "",
		       (mx_pollfd[i].revents & POLLOUT) ? " pollout" : "",
		       (mx_pollfd[i].revents & POLLERR) ? " pollerr" : "");
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

	    if (mx_mti(msp)->mti_poller
                && mx_mti(msp)->mti_poller(msp, pollp)) {
                mx_log("%s poller detects failure", mx_sock_title(msp));
                goto failure;
            }

	    if (msp->ms_state == MSS_FAILED)
		goto failure;
	}

	/* Look thru the requests to see what's failing */
	mx_request_check_health();

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

    fprintf(stderr,
	    "Usage: mixer [options]\n\n"
	    "\t--console or -C: enable console mode\n"
	    "\t--db <dbname>: Specify mixer database file\n"
	    "\t--debug <flag>: turn on specified debug flag\n"
	    "\t--fork: force fork\n"
	    "\t--help: display this message\n"
	    "\t--home <dir>: specify home directory\n"
	    "\t--keep-alive <secs> OR -k <secs>: keep-alive timeout\n"
	    "\t--log <file>: send log message to file\n"
	    "\t--login: require use login\n"
	    "\t--no-db: do not use device database\n"
	    "\t--password <xxx>: use password for device logins\n"
	    "\t--port <n>: use alternative port for websocket\n"
	    "\t--use-known-hosts OR -K: use openssh .known_hosts files\n"
	    "\t--verbose: Enable verbose logs\n"
	    "\t--version OR -V: show version information (and exit)\n"
	    "\nProject juise home page: http://juise.googlecode.com\n"
	    "\n");
	        
    exit(1);
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

static void
sigchld_handler (int sig UNUSED)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
	continue;
}

static void
print_version (void)
{
    printf("libjuice version %s\n", LIBJUISE_VERSION);
    printf("libslax version %s\n",  LIBSLAX_VERSION);
}

int
main (int argc UNUSED, char **argv UNUSED)
{
    char *cp;
    int rc;
    int opt_getpass = FALSE, opt_console = FALSE;
    char *opt_home = NULL;
    unsigned opt_port = 8000;
    char *opt_logfile = NULL;

    for (argv++; *argv; argv++) {
	cp = *argv;
	if (*cp != '-')
	    break;

	if (streq(cp, "--console") || streq(cp, "-c")) {
	    opt_console = TRUE;

	} else if (streq(cp, "--db")) {
	    opt_db = *++argv;

	} else if (streq(cp, "--debug")) {
	    cp  = *++argv;
	    if (cp == NULL)
		print_help(NULL);
	    mx_debug_flags(TRUE, cp);

	} else if (streq(cp, "--fork")) {
	    opt_fork = TRUE;

	} else if (streq(cp, "--help") || streq(cp, "-h")) {
	    print_help(NULL);

	} else if (streq(cp, "--home")) {
	    opt_home = *++argv;

	} else if (streq(cp, "--keep-alive") || streq(cp, "-k")) {
	    opt_keepalive = atoi(*++argv);

	} else if (streq(cp, "--log")) {
	    opt_logfile = *++argv;
            if (opt_logfile == NULL)
                errx(1, "missing log file name");

	} else if (streq(cp, "--login")) {
	    opt_login = TRUE;

	} else if (streq(cp, "--no-db")) {
	    opt_no_db = TRUE;

	} else if (streq(cp, "--password")) {
	    opt_password = *++argv;

	} else if (streq(cp, "--port")) {
	    opt_port = atoi(*++argv);

	} else if (streq(cp, "--prompt-for-password") || streq(cp, "-p")) {
	    opt_getpass = TRUE;

	} else if (streq(cp, "--no-agent")) {
	    opt_no_agent = TRUE;

	} else if (streq(cp, "--no-known-hosts")) {
	    opt_no_known_hosts = TRUE;

	} else if (streq(cp, "--user") || streq(cp, "-u")) {
	    opt_user = *++argv;

	} else if (streq(cp, "--use-known-hosts") || streq(cp, "-K")) {
	    opt_knownhosts = TRUE;

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    opt_verbose = TRUE;

	} else if (streq(cp, "--version") || streq(cp, "-V")) {
	    print_version();
	    exit(0);

	} else {
	    print_help(cp);
	}
    }

    signal(SIGPIPE, SIG_IGN);

    if (opt_logfile) {
        FILE *fp = fopen(opt_logfile, "w");
        if (fp == NULL)
            errx(1, "cannot open log file '%s'", opt_logfile);
        mx_log_file(fp);
    }

    slaxLogEnableCallback(mx_log_callback, NULL);

    if (opt_verbose)
	slaxLogEnable(TRUE);

    if (opt_home == NULL)
	opt_home = getenv("HOME");

    if (opt_fork) {
	struct sigaction sa;

	bzero(&sa, sizeof(sa));
	sa.sa_handler =  sigchld_handler;
	sigaction(SIGCHLD, &sa, 0);
    }

    snprintf(keyfile1, sizeof(keyfile1), "%s/%s/%s",
	     opt_home, keydir, keybase1);
    snprintf(keyfile2, sizeof(keyfile2), "%s/%s/%s",
	     opt_home, keydir, keybase2);

    if (opt_user == NULL)
	opt_user = strdup(getlogin());

    if (opt_getpass)
	opt_password = strdup(getpass("Password:"));

    mx_type_info_init();

    if (!opt_no_db && !mx_db_init())
	errx(1, "mixer database initialization failed");

    rc = libssh2_init (0);
    if (rc != 0)
        errx(1, "libssh2 initialization failed (%d)", rc);

    if (opt_console)
	mx_console_start();

    if (mx_listener(opt_port, MST_LISTENER, MST_WEBSOCKET,
		    "websocket") == NULL)
	errx(1, "initial listen failed");

    main_loop();

    libssh2_exit();

    if (opt_logfile) {
        FILE *fp = mx_log_file(NULL);
        if (fp)
            fclose(fp);
    }

    if (!opt_no_db)
	mx_db_close();

    return 0;
}
