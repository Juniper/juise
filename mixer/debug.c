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

static FILE *mx_log_fp;

FILE *
mx_log_file (FILE *fp)
{
    FILE *old = mx_log_fp;
    mx_log_fp = fp;
    if (fp)
        setlinebuf(fp);
    return old;
}

FILE *
mx_log_fd (int fd)
{
    FILE *fp = fdopen(fd, "w+");
    if (fp == NULL)
	return NULL;

    FILE *old = mx_log_fp;
    mx_log_fp = fp;
    if (fp)
        setlinebuf(fp);
    return old;
}

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
    vfprintf(mx_log_fp ?: stderr, cfmt, vap);
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

    vfprintf(mx_log_fp ?: stderr, cfmt, vap);
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

    if (streq(cp, "dump"))
	rc |= DBG_FLAG_DUMP;

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
