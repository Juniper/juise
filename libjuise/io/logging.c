/*
 * $Id: logging.c 346460 2009-11-14 05:06:47Z ssiano $
 *
 * logging.c - general user-friendly front-end to syslog() for daemons
 * Paul Traina, December 1997
 *
 * Copyright (c) 1997-2008, Juniper Networks, Inc.
 * All rights reserved.
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
 * Format of the magic cookie passed through the stdio hook
 */
struct bufcookie {
	char	*base;	/* start of buffer */
	int	left;
};

/*
 * stdio write hook for writing to a static string buffer
 * XXX: Maybe one day, dynamically allocate it so that the line length
 *      is `unlimited'.
 */
static int
writehook (void *cookie, const char *buf, int len)
{
	struct bufcookie *h;	/* private `handle' */

	h = (struct bufcookie *)cookie;
	if (len > h->left) {
		/* clip in case of wraparound */
		len = h->left;
	}
	if (len > 0) {
		(void)memcpy(h->base, buf, len); /* `write' it. */
		h->base += len;
		h->left -= len;
	}
	return 0;
}

/*
 * This inline function strerror_ri() is to fix the return type of 
 * strerror_r() from int to const char *, it also takes care of EINVAL 
 * return value.
 */
static inline const char *
strerror_ri (int errnum, char *strerrbuf, size_t buflen)
{
    if (strerror_r(errnum, strerrbuf, buflen) != 0)
	strerror_r(EINVAL, strerrbuf, buflen);

    return strerrbuf;
}

static void
vlogging_stdout (const char *fmt, va_list ap)
{
	struct bufcookie fmt_cookie;
	char fmt_cpy[BUFSIZ], ch, errstr[STRERROR_BUFSIZ];
	FILE *fmt_fp;
	int saved_errno = errno;

	/* Check to see if we can skip expanding the %m */
	if (strstr(fmt, "%m")) {

		/* Create the second stdio hook */
		fmt_cookie.base = fmt_cpy;
		fmt_cookie.left = sizeof(fmt_cpy) - 1;
		fmt_fp = fwopen(&fmt_cookie, writehook);
		if (fmt_fp == NULL) {
			(void)vfprintf(stderr, fmt, ap);
			errno = saved_errno;
			return;
		}

		/* Substitute error message for %m. */
		for ( ; (ch = *fmt); ++fmt)
			if (ch == '%' && fmt[1] == 'm') {
				++fmt;
				fputs(strerror_ri(saved_errno, errstr, sizeof(errstr)), fmt_fp);	   
			} else
				fputc(ch, fmt_fp);

		/* Null terminate if room */
		fputc(0, fmt_fp);
		fclose(fmt_fp);

		/* Guarantee null termination */
		fmt_cpy[sizeof(fmt_cpy) - 1] = '\0';

		fmt = fmt_cpy;
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

