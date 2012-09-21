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

    mx_log("%*s%sport %u, spawns %s to %s", indent, "", prefix,
	   mslp->msl_port, mx_sock_type_number(mslp->msl_spawns),
	   mslp->msl_request->mr_target);
}

mx_sock_t *
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

    MX_LOG("S%u new listener, fd %u, spawns %s, port %s:%d...",
	   mslp->msl_base.ms_id, mslp->msl_base.ms_sock,
	   mx_sock_type_number(mslp->msl_spawns),
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    return &mslp->msl_base;
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
	    mx_log("S%u: accept: %s", msp->ms_id, strerror(errno));
	    return TRUE;
	}

	mx_log("new connection from %s here to remote %s:%d",
	       mx_sock_sin(newp), mslp->msl_request->mr_desthost,
	       mslp->msl_request->mr_destport);
    }

    return FALSE;
}

void
mx_listener_init (void)
{
    static mx_type_info_t mti = {
    mti_type: MST_LISTENER,
    mti_name: "listener",
    mti_print: mx_listener_print,
    mti_poller: mx_listener_poller,
    mti_close: mx_listener_close,
    };

    mx_type_info_register(MX_TYPE_INFO_VERSION, &mti);
}
