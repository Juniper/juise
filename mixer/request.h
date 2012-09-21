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
mx_request_create (mx_sock_websocket_t *mswp, mx_buffer_t *mbp UNUSED,
		   const char *tag, const char **attr);

int
mx_request_start_rpc (mx_sock_websocket_t *mswp, mx_buffer_t *mbp UNUSED,
		      mx_request_t *mrp);

mx_request_t *
mx_request_find (mx_sock_websocket_t *mswp, const char *muxid);

void
mx_request_free (mx_request_t *mrp);

