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

mx_buffer_t *
mx_buffer_create (unsigned size);

void
mx_buffer_free (mx_buffer_t *mbp);

void
mx_buffer_reset (mx_buffer_t *mbp);
