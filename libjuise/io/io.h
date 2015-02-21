/*
 * $Id$
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * This file defines a simple light-weight file buffering mechanism
 * that avoids copying data by returning a pointer into its buffer.
 */

static inline boolean
safe_close (int fd)
{
    boolean rc = FALSE;

    if (fsync(fd))
        rc = TRUE;

    if (close(fd))
        rc = TRUE;

    return rc;
}
