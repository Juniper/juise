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
 * racer -- Remote Access Connection Establish and Reuse Daemon
 */

#include "local.h"

#define POLL_TIMEOUT	10	/* Default timeout value */
#define POLLFD_BUMP	16	/* Incremental allocation for cs_pollfd_list */

/*
 * We build an array in cs_pollfd_list that we can pass to libssh2_poll().
 * 
 */
static LIBSSH2_POLLFD *
racer_connset_pollfd_new (racer_connset_t *csp)
{
    if (csp->cs_pollfd_len < csp->cs_pollfd_size) {
	unsigned len = csp->cs_pollfd_size + POLLFD_BUMP;
	LIBSSH2_POLLFD *newp = realloc(csp->cs_pollfd_list,
				       len * sizeof(*newp));
	if (newp == NULL)
	    return NULL;

	csp->cs_pollfd_list = newp;
	csp->cs_pollfd_len = len;
    }

    return csp->cs_pollfd_list + csp->cs_pollfd_len++;
}

racer_conn_t *
racer_conn_create (...)
{
    
}

void
racer_conn_add (racer_connset_t *csp, racer_conn_t *rcp)
{
}

void
racer_conn_destroy (racer_connset_t *csp, racer_conn_t *rcp)
{
}

void
racer_connset_init (racer_connset_t *csp)
{
    TAILQ_INIT(&csp->cs_conn_list);
    csp->cs_timeout = POLL_TIMEOUT;
}

void
racer_connset_listen (racer_connset_t *csp, int fd)
{
    csp->cs_listen = fd;

    LIBSSH2_POLLFD *pfp = racer_connset_pollfd_new(csp);
    if (pfp) {
	pfp->type = LIBSSH2_POLLFD_LISTENER;
	pfp->fd.listener->session->socket_fd = fd;
	pfp->events = LIBSSH2_POLLFD_POLLIN;
    }
}

void
racer_event_loop (racer_connset_t *csp)
{
    for (;;) {
	rc = libssh2_poll(csp->cs_pollfd_list,
			  csp->cs_pollfd_len, csp->cs_timeout);
    }
}
