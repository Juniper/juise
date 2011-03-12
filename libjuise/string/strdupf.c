/*
 * $Id: strdupf.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 2000-2006, Juniper Networks, Inc.
 * All rights reserved.
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

