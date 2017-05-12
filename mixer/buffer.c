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

#include <libpsu/psucommon.h>
#include <libpsu/psustring.h>

#include "local.h"
#include "buffer.h"

mx_buffer_t *
mx_buffer_create (unsigned size)
{
    if (size == 0)
	size = BUFFER_DEFAULT_SIZE;

    mx_buffer_t *mbp = malloc(sizeof(*mbp) + size);
    if (mbp == NULL)
	return NULL;

    bzero(mbp, sizeof(*mbp));
    mbp->mb_size = size;

    return mbp;
}

mx_buffer_t *
mx_buffer_copy (mx_buffer_t *base, int len)
{
    mx_buffer_t *mbp = mx_buffer_create(len);

    if (mbp) {
	memcpy(mbp->mb_data, base->mb_data + base->mb_start, len);
	mbp->mb_len = len;
    }

    return mbp;
}

void
mx_buffer_reset (mx_buffer_t *mbp)
{
    mbp->mb_start = mbp->mb_len = 0;
}

void
mx_buffer_free (mx_buffer_t *mbp)
{
    mx_buffer_t *next;
    do {
	next = mbp->mb_next;
	free(mbp);
	mbp = next;
    } while (next);
}
