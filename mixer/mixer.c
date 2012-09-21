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

#include "local.h"
#include "listener.h"
#include "forwarder.h"
#include "session.h"
#include "console.h"
#include "websocket.h"

unsigned mx_sock_id;   /* Monotonically increasing ID number */

static struct pollfd mx_pollfd[MAX_POLL];
static struct pollfd *mx_poll_owner[MAX_POLL];

static const char keydir[] = ".ssh";
static const char keybase1[] = "id_dsa.pub";
static const char keybase2[] = "id_dsa";

char keyfile1[MAXPATHLEN], keyfile2[MAXPATHLEN];

const char *opt_user;
const char *opt_password;
const char *opt_desthost = "localhost";
unsigned opt_destport = 22;

static int opt_login;
static int opt_fork;

static mx_password_t *mx_saved_passwords;

mx_password_t *
mx_password_find (const char *target, const char *user)
{
    mx_password_t *mpp;

    for (mpp = mx_saved_passwords; mpp; mpp = mpp->mp_next)
	if (streq(target, mpp->mp_target) && streq(user, mpp->mp_user))
	    return mpp;

    return NULL;
}

const char *
mx_password (const char *target, const char *user)
{
    mx_password_t *mpp;

    for (mpp = mx_saved_passwords; mpp; mpp = mpp->mp_next)
	if (streq(target, mpp->mp_target) && streq(user, mpp->mp_user))
	    return mpp->mp_password;

    return NULL;
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

mx_buffer_t *
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

void
mx_buffer_free (mx_buffer_t *mbp)
{
    mx_buffer_t *next;
    do {
	next = mbp->mb_next;
	free(mbp);
	mbp = next;
    } while (next);
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

    mx_log("S%u close (%u)", msp->ms_id, msp->ms_state);

    if (mx_mti(msp)->mti_close)
	mx_mti(msp)->mti_close(msp);

    else if (msp->ms_sock > 0)
	close(msp->ms_sock);

    TAILQ_REMOVE(&mx_sock_list, msp, ms_link);
    mx_sock_count -= 1;

    free(msp);
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

	assert(mx_sock_count < MAX_POLL);

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

	if (streq(cp, "--console") || streq(cp, "-c")) {
	    opt_console = TRUE;

	} else if (streq(cp, "--fork")) {
	    opt_login = TRUE;

	} else if (streq(cp, "--home")) {
	    opt_home = *++argv;

	} else if (streq(cp, "--login")) {
	    opt_login = TRUE;

	} else if (streq(cp, "--password")) {
	    opt_password = *++argv;

	} else if (streq(cp, "--prompt-for-password") || streq(cp, "-p")) {
	    opt_getpass = TRUE;

	} else if (streq(cp, "--user") || streq(cp, "-u")) {
	    opt_user = *++argv;

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    opt_verbose = TRUE;

	} else if (streq(cp, "--debug")) {
	    cp  = *++argv;
	    if (cp == NULL)
		print_help(NULL);
	    mx_debug_flags(TRUE, cp);

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
