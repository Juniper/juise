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

mx_request_t *
mx_request_create (mx_sock_websocket_t *mswp, mx_buffer_t *mbp,
		   mx_muxid_t muxid, const char *tag, const char **attr);

int
mx_request_start_rpc (mx_sock_websocket_t *mswp, mx_request_t *mrp);

mx_request_t *
mx_request_find (mx_muxid_t muxid);

void
mx_request_free (mx_request_t *mrp);

void
mx_request_print (mx_request_t *mrp, int indent, const char *prefix);

void
mx_request_init (void);

void
mx_request_release_session (mx_sock_session_t *session);

void
mx_request_restart_rpc (mx_request_t *mrp);
