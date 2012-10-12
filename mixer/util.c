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

#include "local.h"
#include <sys/stat.h>
#include "util.h"

int
exists (const char *filename)
{
    struct stat st;

    if (stat(filename, &st) < 0)
	return FALSE;

    if (st.st_size == 0 || !(st.st_mode & S_IFREG))
	return FALSE;

    return TRUE;
}
