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

unsigned opt_debug;
unsigned opt_verbose;

void
#ifdef HAVE_PRINTFLIKE
__printflike(1, 2)
#endif /* HAVE_PRINTFLIKE */
mx_log (const char *fmt, ...)
{
    va_list vap;
    int len = strlen(fmt);
    char *cfmt = alloca(len + 2);

    memcpy(cfmt, fmt, len);
    cfmt[len] = '\n';
    cfmt[len + 1] = '\0';

    va_start(vap, fmt);
    vfprintf(stderr, cfmt, vap);
    va_end(vap);
}

void
mx_log_callback (void *opaque UNUSED, const char *fmt, va_list vap)
{
    int len = strlen(fmt);
    char *cfmt = alloca(len + 2);

    memcpy(cfmt, fmt, len);
    cfmt[len] = '\n';
    cfmt[len + 1] = '\0';

    vfprintf(stderr, cfmt, vap);
}

static void
mx_debug_print_help (const char *cp UNUSED)
{
    mx_log("Flags: %0x", opt_debug);
}

static unsigned
mx_debug_parse_flag (const char *cp)
{
    unsigned rc = 0;

    if (cp == NULL || *cp == '\0') {
	mx_debug_print_help(NULL);
	return 0;
    }

    if (streq(cp, "poll"))
	rc |= DBG_FLAG_POLL;

    if (streq(cp, "all"))
	rc |= - 1;

    return rc;
}

void
mx_debug_flags (int set, const char *value)
{
    if (set)
	opt_debug |= mx_debug_parse_flag(value);
    else 
	opt_debug &= ~mx_debug_parse_flag(value);
}
