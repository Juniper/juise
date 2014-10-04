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
#include "listener.h"
#include "request.h"

static void
mx_listener_print (MX_TYPE_PRINT_ARGS)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);

    mx_log("%*s%spath %s, spawns %s to %s", indent, "", prefix,
	   mslp->msl_base.ms_sun.sun_path,
	   mx_sock_type_number(mslp->msl_spawns),
	   mslp->msl_request->mr_target);
}

mx_sock_t *
mx_listener (const char *path, mx_type_t type, int spawns, const char *target)
{
    struct sockaddr_un sun;

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
	return NULL;

    int sockopt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

    bzero(&sun, sizeof(sun));
    sun.sun_family = AF_UNIX;
#ifdef HAVE_SUN_LEN
    sun.sun_len = sizeof(sun);
#endif
    strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

    unlink(path);

    if (bind(sock, (struct sockaddr *) &sun, sizeof(sun)) < 0) {
        mx_log("listener path %s: bind: %s", path, strerror(errno));
	close(sock);
	return NULL;
    }

    if (listen(sock, 5) < 0) {
        mx_log("listener path %s: listen: %s", path, strerror(errno));
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
    mslp->msl_base.ms_sun = sun;
    mslp->msl_spawns = spawns;

    mslp->msl_request = calloc(1, sizeof(*mslp->msl_request));
    if (mslp->msl_request) {
	mslp->msl_request->mr_target = nstrdup(target);
	mslp->msl_request->mr_user = nstrdup(opt_user);
	mslp->msl_request->mr_password = nstrdup(opt_password);
	mslp->msl_request->mr_desthost = nstrdup(opt_desthost);
	mslp->msl_request->mr_destport = opt_destport;
    }

    TAILQ_INSERT_HEAD(&mx_sock_list, &mslp->msl_base, ms_link);
    mx_sock_count += 1;

    MX_LOG("%s new listener, fd %u, spawns %s, %s...",
	   mx_sock_title(&mslp->msl_base), mslp->msl_base.ms_sock,
	   mx_sock_type_number(mslp->msl_spawns),
	   mx_sock_name(&mslp->msl_base));

    return &mslp->msl_base;
}

static mx_sock_t *
mx_listener_accept (mx_sock_listener_t *listener)
{
    struct sockaddr_un sun;
    socklen_t sunlen = sizeof(sun);

    int sock = accept(listener->msl_base.ms_sock,
		      (struct sockaddr *) &sun, &sunlen);
    if (sock < 0) {
        mx_log("%s: accept: %s", mx_sock_title(&listener->msl_base),
               strerror(errno));
	return NULL;
    }

    mx_nonblocking(sock);

    assert(mx_mti_number(listener->msl_spawns)->mti_spawn);

    mx_sock_t *msp;
    msp = mx_mti_number(listener->msl_spawns)->mti_spawn(listener, sock,
							 &sun, sunlen);
    if (msp == NULL)
	return NULL;

    TAILQ_INSERT_HEAD(&mx_sock_list, msp, ms_link);
    mx_sock_count += 1;

    MX_LOG("%s new %s, fd %u, %s...",
	   mx_sock_title(msp), mx_sock_type(msp), msp->ms_sock,
	   mx_sock_name(msp));

    return msp;
}

static void
mx_listener_close (mx_sock_t *msp)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);

    mx_request_free(mslp->msl_request);
}

static int
mx_listener_poller (MX_TYPE_POLLER_ARGS)
{
    mx_sock_listener_t *mslp = mx_sock(msp, MST_LISTENER);

    if (pollp && pollp->revents & POLLIN) {
	mx_sock_t *newp = mx_listener_accept(mslp);
	if (newp == NULL) {
	    mx_log("%s: accept: %s", mx_sock_title(msp), strerror(errno));
	    return TRUE;
	}

	mx_log("new connection from %s here to remote %s:%d",
	       mx_sock_name(newp), mslp->msl_request->mr_desthost,
	       mslp->msl_request->mr_destport);
    }

    return FALSE;
}

void
mx_listener_init (void)
{
    static mx_type_info_t mti = {
	.mti_type = MST_LISTENER,
	.mti_name = "listener",
	.mti_letter = "L",
	.mti_print = mx_listener_print,
	.mti_poller = mx_listener_poller,
	.mti_close = mx_listener_close,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
