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

mx_channel_t *
mx_channel_create (mx_sock_session_t *session,
		   mx_sock_t *client, LIBSSH2_CHANNEL *channel);

mx_channel_t *
mx_channel_direct_tcpip (mx_sock_session_t *session, mx_sock_t *client,
			 const char *desthost, unsigned destport);

mx_channel_t *
mx_channel_netconf (mx_sock_session_t *mssp, mx_sock_t *client, int xml_mode);

int
mx_channel_handle_input (mx_channel_t *mcp);

void
mx_channel_close (mx_channel_t *mcp);

int
mx_channel_write_buffer (mx_channel_t *mcp, mx_buffer_t *mbp);

int
mx_channel_sock (mx_channel_t *mcp);

int
mx_channel_has_buffered (mx_channel_t *mcp);

void
mx_channel_print (mx_channel_t *mcp, int indent, const char *prefix);
