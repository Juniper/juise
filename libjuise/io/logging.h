/*
 * $Id$
 *
 * Copyright (c) 1997-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef __JNX_LOGGING_H__
#define __JNX_LOGGING_H__

/**
 * @file logging.h
 * @brief 
 * Contains front-end APIs to @c syslog for daemons.
 */

#include <syslog.h>			/* caller will need this stuff */
#include <stdarg.h>
#include <libjuise/env/env.h>

#ifdef HOSTPROG
# ifndef LOG_CONFLICT
#   include <jnx/syslog_shared.h>
# endif
#endif

#ifdef HOSTPROG
# ifndef LOG_CONFLICT
#   include <jnx/syslog_shared.h>
# endif
#endif

#define	LOGGING_STDERR	(1<<0)		/**< logging info to stderr */
#define	LOGGING_SYSLOG	(1<<1)		/**< logging info to syslog */

#define STRERROR_BUFSIZ 64 

/* Handle compilation hosts on FreeBSD 6.0-RELEASE or newer */
#ifndef _BSD_VA_LIST_
# if defined HOSTPROG
# include <sys/param.h>
#  if __FreeBSD_version >= 600000
#   define _BSD_VA_LIST_ __va_list
#  endif /* __FreeBSD_version */
# endif /* HOSTPROG */
#endif /* _BSD_VA_LIST_ */

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" {
    namespace junos {
#endif

/**
 * @brief
 * Callback function for use with logging_register_logfunc().
 */
typedef void (*logging_log_func_t) (int severity, const char *tag,
				    const char *lsname, const char **entry,
				    const char *fmt, va_list);


/**
 * @brief
 * In-place replacement for @c syslog library routine.
 *
 * This function will log to @c stderr or to system log (via vlogging).
 * This function is thread safe.
 *
 * @param[in] severity
 *     Message severity level
 * @param[in] format
 *     Format string (printf-like)
 */
void
logging	(int severity, const char *format, ...) PRINTFLIKE(2, 3);

/**
 * @brief
 * In-place replacement for @c vsyslog library routine.
 *
 * This function will log to stderr or to system log.
 * This function is thread safe.
 *
 * @param[in] severity
 *     Message severity level
 * @param[in] format
 *     Format string (printf-like)
 * @param[in] ap
 *     Variable argument list
 */
void
vlogging (int severity, const char *format, va_list ap);

/**
 * @brief
 * Writes a log message using the registered logging callback function, 
 * or @c vsyslog if no callback function has been registered.
 *
 * If a logging callback function has been registered, and an @c ERRMSG tag has
 * been passed, then invoke the callback function, else invoke @c vsyslog.
 * This API is thread safe.
 * 
 * @param[in] severity
 *     Message severity level
 * @param[in] tag
 *     TODO
 * @param[in] entry
 *     TODO
 * @param[in] format
 *     Format string (printf-like)
 */
void
logging_event (int severity, const char *tag, const char **entry,
	       const char *format, ...) PRINTFLIKE(4, 5);

/**
 * @brief
 * Writes a log message using the registered logging callback function, 
 * or @c vsyslog if no callback function has been registered.
 *
 * If a logging callback function has been registered, and an @c ERRMSG_LS tag
 * has been passed, then invoke the callback function, else invoke @c vsyslog.
 * This API is thread safe.
 * 
 * @param[in] severity
 *     Message severity level
 * @param[in] tag
 *     Error message tag
 * @param[in] lsname
 *     Logical system name
 * @param[in] entry
 *     Array of attributes
 * @param[in] format
 *     Format string (printf-like)
 */
void
logging_event_ls (int severity, const char *tag, const char *lsname,
		  const char **entry,
		  const char *format, ...) PRINTFLIKE(5, 6);

/**
 * @brief
 * Writes a log message using the registered logging callback function, 
 * or @c vsyslog.
 *
 * If a logging callback function has been invoked, and an @c ERRMSG tag has
 * been passed, then invoke the callback function, else invoke @c vsyslog.
 * This API is thread safe.
 *
 * @param[in] severity
 *     Message severity level
 * @param[in] tag
 *     Error message tag
 * @param[in] lsname
 *     Logical system name
 * @param[in] entry
 *     Array of attributes
 * @param[in] format
 *     Format string (printf-like)
 * @param[in] ap
 *     Variable argument list
 */
void
vlogging_event (int severity, const char *tag, const char *lsname,
		const char **entry, const char *format, va_list ap);

/**
 * @brief
 * Sets the system logging mode (@c syslog, @c stderr, nothing)
 *
 * The logging mode can be a combination of:
 * @li @c LOGGING_STDERR
 * @li @c LOGGING_SYSLOG
 *
 * You can force the logging mode to "nothing" by setting the mode to 0.
 * 
 * The default logging mode is @c LOGGING_STDERR.
 * This API is thread safe.
 *
 * @param[in] mode
 *     Selected system log mode
 *
 * @return 
 *     The previous system log mode
 */
int
logging_set_mode (int mode);

/**
 * @brief
 * Sets the minimum logged error message.
 *
 * This function logs anything over a debug or info message, regardless of 
 * the level set by this function.
 * This function is thread safe.
 *
 * @param[in] level
 *     Minimum logged error message level 
 *
 * @return 
 *     The previous logging level.
 */
int
logging_set_level (int level);

/**
 * @brief
 * Registers a callback function to be invoked for logging.
 *
 * If no callback function is registered, @c vsyslog will be 
 * used instead.
 * This API is thread safe.
 *
 * @param[in] func
 *     Callback function to be invoked
 */
void
logging_register_logfunc (logging_log_func_t func);


#ifdef __cplusplus
    }
}
#endif /* __cplusplus */

#ifndef INSIST
# include <assert.h>
# define INSIST(x) assert(x)
# define INSIST_ERR(x) assert(x)
#endif /* INSIST */

#endif /* __JNX_LOGGING_H__ */

