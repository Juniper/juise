/*
 * $Id$
 *
 * Copyright (c) 1997-2008, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * (Originally libjuniper/logging.c)
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <libjuise/env/env.h>
#include <libjuise/io/logging.h>

#if defined(HOSTPROG) && !defined(va_copy)
#define va_copy(dest, src) ((dest) = (src))
#endif

/*
 * Logging Support
 */

static int logging_level = LOG_INFO;
static int logging_mode  = LOGGING_STDERR;
static logging_log_func_t logging_log_func;

/*
 * This inline function strerror_ri() is to fix the return type of 
 * strerror_r() from int to const char *, it also takes care of EINVAL 
 * return value.
 */
static inline const char *
strerror_ri (int errnum, char *strerrbuf, size_t buflen)
{
    if (strerror_r(errnum, strerrbuf, buflen) != 0)
	if (strerror_r(EINVAL, strerrbuf, buflen) != 0)
	    return "unknown error";

    return strerrbuf;
}

static void
vlogging_stdout (const char *fmt, va_list ap)
{
    int saved_errno = errno;
    char fmt_cpy[BUFSIZ], errstr[STRERROR_BUFSIZ];
    char *mp;
    const char *ep;
    int elen, alen;

    /* Check to see if we can skip expanding the %m */
    mp = strstr(fmt, "%m");
    if (mp) {
	ep = strerror_ri(saved_errno, errstr, sizeof(errstr));
	elen = strlen(ep);
	if (strlen(fmt) + elen < sizeof(fmt_cpy)) {
	    alen = mp - fmt;
	    mp += 2;

	    memcpy(fmt_cpy, fmt, alen);
	    memcpy(fmt_cpy + alen, ep, elen);
	    memcpy(fmt_cpy + alen + elen, mp, strlen(mp) + 1);
	    fmt = fmt_cpy;
	}
    }

    (void)vfprintf(stderr, fmt, ap);
    (void)fputc('\n', stderr);
    errno = saved_errno;
}

/*
 * vlogging()
 *
 * In-place replacement for vsyslog() library routine.
 * Will log to stderr or to system log.
 */

void
vlogging (int severity, const char *format, va_list ap)
{
    vlogging_event(severity, NULL, NULL, NULL, format, ap);
}

/*
 * If a logging callback function has been invoked, and an ERRMSG tag has
 * been passed, then invoke the callback function, else invoke vsyslog
 */
void
vlogging_event (int severity, const char *tag, const char *lsname,
		const char **entry, const char *format, va_list ap)
{
#ifdef HAVE_VOLATILE
    volatile int saved_errno __unused = errno;
#endif
    int pri = LOG_PRI(severity);
    va_list newap;
    logging_log_func_t log_func = logging_log_func;
	
    switch (pri) {
    case LOG_DEBUG:
	if (logging_level < LOG_DEBUG) 
	    return;
    case LOG_INFO:
	if (logging_level < LOG_INFO) 
	    return;
    default:
	break;
    }

    if (logging_mode & LOGGING_SYSLOG) {
	va_copy(newap, ap);
	if (tag && log_func)
	    log_func(severity, tag, lsname, entry, format, newap);
	else
	    vsyslog(severity, format, newap);
	va_end(newap);
    }

    if (logging_mode & LOGGING_STDERR)
	vlogging_stdout(format, ap);

    if (pri == LOG_EMERG)
	abort();
}

/*
 * logging()
 *
 * In-place replacement for syslog() library routine.
 * Will log to stderr or to system log (via vlogging).
 */

void
logging (int severity, const char *format, ...) 
{
    va_list ap;

    va_start(ap, format);
    vlogging(severity, format, ap);
    va_end(ap);
}

/*
 * If a logging callback function has been invoked, and an ERRMSG tag has
 * been passed, then invoke the callback function, else invoke vsyslog
 */
void
logging_event (int severity, const char *tag, const char **entry, 
	       const char *format, ...) 
{
    va_list ap;

    va_start(ap, format);
    vlogging_event(severity, tag, NULL, entry, format, ap);
    va_end(ap);
}

/*
 * If a logging callback function has been invoked, and an ERRMSG_LS tag has
 * been passed, then invoke the callback function, else invoke vsyslog
 */
void
logging_event_ls (int severity, const char *tag, const char *lsname,
		  const char **entry, const char *format, ...) 
{
    va_list ap;

    va_start(ap, format);
    vlogging_event(severity, tag, lsname, entry, format, ap);
    va_end(ap);
}

/*
 * logging_set_mode
 * Pick system log mode (syslog, stderr, nothing)
 */

int
logging_set_mode (int mode)
{
    int old;

    old = logging_mode;
    logging_mode = mode;

    return old;
}

/*
 * logging_set_level
 * Set the minimum logged error message (in reality, we're going to
 * log anything over a debug or info message whether you like it or not.
 */

int
logging_set_level (int level)
{
    int old;

    old = logging_level;
    logging_level = level;

    setlogmask(LOG_UPTO(level));

    return old;
}

/*
 * Register a callback function to be invoked for logging, instead of vsyslog
 */
void
logging_register_logfunc (logging_log_func_t func)
{
    logging_log_func = func;
}

