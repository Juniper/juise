/*
 * $Id$
 *
 * Copyright (c) 2000-2006, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * (Originally part of libjuniper/strextra.c)
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "config.h"
#include <libjuise/string/strextra.h>

/*
 * strdupf(): sprintf + strdup: two great tastes in one!
 */
char *
strdupf (const char *fmt, ...)
{
    va_list vap;
    char buf[BUFSIZ];

    va_start(vap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, vap);
    va_end(vap);

    return strdup(buf);
}

