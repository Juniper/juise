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

extern unsigned opt_debug;
extern unsigned opt_verbose;

/* Flags for opt_debug: */
#define DBG_FLAG_POLL	(1<<0)	/* poll()/poller related */
#define DBG_FLAG_DUMP	(1<<1)	/* dump packet contents */

#define MX_LOG(_fmt...) \
    do { if (opt_debug || opt_verbose) mx_log(_fmt); } while (0)

#define DBG_FLAG(_flag, _fmt...) \
    do { if (opt_debug & _flag) mx_log(_fmt); } while(0)
#define DBG_POLL(_fmt...) DBG_FLAG(DBG_FLAG_POLL, _fmt)

FILE *
mx_log_file (FILE *fp);

void
#ifdef HAVE_PRINTFLIKE
__printflike(1, 2)
#endif /* HAVE_PRINTFLIKE */
mx_log (const char *fmt, ...);

void
mx_log_callback (void *opaque UNUSED, const char *fmt, va_list vap);

void
mx_debug_flags (int set, const char *value);
