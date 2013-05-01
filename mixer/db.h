/*
 * $Id$
 *
 * Copyright (c) 2013, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#define DB_CHECK_HOSTKEY_MATCH	    0
#define DB_CHECK_HOSTKEY_NOMATCH    1
#define DB_CHECK_HOSTKEY_MISMATCH   2

int
mx_db_check_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp);

void
mx_db_save_hostkey (mx_sock_session_t *mssp, mx_request_t *mrp);

const char *
mx_db_get_passphrase (void);

void
mx_db_save_passphrase (const char *passphrase);

void
mx_db_save_password (mx_request_t *mrp, const char *password);

mx_boolean_t
mx_db_target_lookup (const char *target, mx_request_t *mrp);

int
mx_db_init (void);

void
mx_db_close (void);
