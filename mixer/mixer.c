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

#include "base.h"
#include "local.h"
#include "mtypes.h"

#define POLL_TIMEOUT 60

static mixer_peer_list_t peer_list;
static int peer_count;
trace_file_t *trace_file;

mixer_peer_type_t mixer_peer_type_table[NUM_MIXER_PEER_TYPES];

void *
mixer_peer_create (unsigned type, size_t size)
{
    mixer_peer_t *mpp = malloc(size);

    if (mpp == NULL)
	return NULL;

    bzero(mpp, size);
    mpp->mp_type = type;
    TAILQ_INSERT_TAIL(&peer_list, mpp, mp_link);

    mixer_peer_type_create(mpp);
    peer_count += 1;

    return mpp;
}

void
mixer_peer_destroy (mixer_peer_t *mpp)
{
    mixer_peer_type_destroy(mpp);

    free(mpp);
    peer_count -= 1;
}

static int
mixer_event_loop_once (void)
{
    mixer_peer_t *mpp;
    struct pollfd pfd[peer_count];
    int npfd = 0;

    TAILQ_FOREACH(mpp, &peer_list, mp_link) {
	if (mixer_peer_type_pre_select(mpp, pfd + npfd))
	    npfd += 1;
    }

    if (npfd == 0) {
	juise_log("event loop sees no FDs; dropping out");
	return -1;
    }

    int rc = poll(pfd, npfd, POLL_TIMEOUT);

    if (rc < 0) {
	juise_log("event loop failed: %d/%s", errno, strerror(errno));
	return rc;
    }

    npfd = 0;
    TAILQ_FOREACH(mpp, &peer_list, mp_link) {
	if (mixer_peer_type_post_select(mpp, pfd + npfd))
	    npfd += 1;
    }

    if (rc != npfd)
	juise_log("event loop count is off (%d/%d)", rc, npfd);

    return npfd;
}

static void
mixer_event_loop (void)
{
    for (;;) {
	if (mixer_event_loop_once() <= 0)
	    return;
    }
}

static void print_help (void)
{
    fprintf(stderr, "Usage:\n    mixer [options]\n\n");
}

int
main (int argc UNUSED, char **argv)
{
    char *cp;
    char *sockpath = NULL;
    int opt_ssh_agent = FALSE;
    int opt_logger = FALSE;
    char *opt_username = NULL;
    char *opt_trace_file_name = NULL;

    for ( ; *argv; argv++) {
	cp = *argv;

	if (*cp != '-') {
	    break;

	} else if (streq(cp, "--agent") || streq(cp, "-A")) {
	    opt_ssh_agent = TRUE;

	} else if (streq(cp, "--trace") || streq(cp, "-t")) {
	    opt_trace_file_name = *++argv;

	} else if (streq(cp, "--user") || streq(cp, "-u")) {
	    opt_username = *++argv;

	} else if (streq(cp, "--verbose") || streq(cp, "-v")) {
	    opt_logger = TRUE;

	} else {
	    fprintf(stderr, "invalid option: %s\n", cp);
	    print_help();
	    return 1;
	}
    }

    if (opt_trace_file_name == NULL)
	opt_trace_file_name = getenv("MIXER_TRACE_FILE");

    if (opt_trace_file_name)
	juise_trace_init(opt_trace_file_name, &trace_file);

#if 0
    if (opt_logger)
	slaxLogEnable(TRUE);
#endif

    TAILQ_INIT(&peer_list);

    if (sockpath == NULL)
	asprintf(&sockpath, "%s/.ssh/mixer.sock", getenv("HOME"), getpid());

    mixer_init_afunix(sockpath);
    mixer_init_websocket();
    mixer_init_session();
    mixer_init_channel();

    mixer_event_loop();

    free(sockpath);

    return 0;			/* Not reached */
}
