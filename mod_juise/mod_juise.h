/*
 * Copyright (c) 2014, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */
#ifndef MOD_JUISE_H
#define MOD_JUISE_H

int response_header_insert (server *srv, connection *con, const char *key, 
			    size_t key_len, const char *value, size_t vallen);

int response_header_overwrite (server *srv, connection *con, const char *key, 
			       size_t key_len, const char *value, 
			       size_t vallen);

char *slaxBase64Decode (const char *buf, size_t blen, size_t *olenp);

#endif /* MOD_JUISE_H */
