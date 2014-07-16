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

#define MX_OP_COMPLETE	"complete"
#define MX_OP_ERROR	"error"
#define MX_OP_HOSTKEY	"hostkey"
#define MX_OP_PASSPHRASE "psphrase"
#define MX_OP_PASSWORD	"psword"
#define MX_OP_REPLY	"reply"
#define MX_OP_RPC	"rpc"
#define MX_OP_HTMLRPC	"htmlrpc"
#define MX_OP_AUTHINIT	"authinit"
#define MX_OP_DATA	"data"

void
mx_websocket_handle_request (mx_sock_websocket_t *mswp, mx_buffer_t *mbp);

void
mx_websocket_init (void);
