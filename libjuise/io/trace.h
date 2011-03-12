/*
 * $Id: trace.h 346460 2009-11-14 05:06:47Z ssiano $
 *
 * Copyright (c) 1998, 2000-2008, Juniper Networks, Inc.
 * All rights reserved.
 */
#ifndef __JNX_TRACE_H__
#define __JNX_TRACE_H__

#include <sys/errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <libjuise/env/env.h>
#include <libjuise/io/logging.h>

/*
 * Use C linkage when using a C++ compiler
 */
#ifdef __cplusplus
extern "C" { 
    namespace junos {
#endif /* __cplusplus */

/*
 * Trace types.  These may be 'or'ed with LOG_xxx symbols in order
 * to force syslogging of the trace information as well (in which
 * case the TRACE_LOG bit should be set as well).
 *
 * The application should define a set of trace values, with values
 * starting at 0x100 and increasing by 0x100 each time.  This gives
 * 126 possible trace flags (0x7F00 is reserved for TRACE_ALL).
 */
#define TRACE_ALL 0x7F00        /**< Trace all types */

#define TRACE_LOG 0x8000	/**< Set if logging should be done */
#define TRACE_MASK 0xff00	/**< Extract TRACE_xx from LOG_xx  */
#define TRACE_SHIFT 8		/**< Shift TRACE_xx down to low order */
#define TRACE_ALL_BITS 0xffffffff	/* All trace bits set */
#define TRACE_NSEC_PER_MSEC 1000	/* Nanoseconds per microsecond */

/*
 * Convenience constants that will log to syslog as well as trace file.
 * Appropriate for apps that redefine ERRMSG to call trace()
 */
#define TRACE_LOG_DEBUG     (TRACE_ALL | TRACE_LOG | LOG_DEBUG)
#define TRACE_LOG_INFO      (TRACE_ALL | TRACE_LOG | LOG_INFO)
#define TRACE_LOG_NOTICE    (TRACE_ALL | TRACE_LOG | LOG_NOTICE)
#define TRACE_LOG_WARNING   (TRACE_ALL | TRACE_LOG | LOG_WARNING)
#define TRACE_LOG_ERR       (TRACE_ALL | TRACE_LOG | LOG_ERR)
#define TRACE_LOG_CRITICAL  (TRACE_ALL | TRACE_LOG | LOG_CRIT)
#define TRACE_LOG_EMERG     (TRACE_ALL | TRACE_LOG | LOG_EMERG)
#define TRACE_LOG_CONFLICT  (TRACE_ALL | TRACE_LOG | LOG_CONFLICT)

/*
 * Auxiliary trace flags.  These are modifiers for the base tracing.
 */
#define TRACE_AUX_FLAG_MSEC (1<<0)     /**< Log milliseconds */
#define TRACE_AUX_FLAG_NOCOMPRESS (1<<1) /**< Do not compress log files */

/*
 * Trace file structure.
 */
struct trace_file_s;
typedef struct trace_file_s trace_file_t;

/*
 * External definitions.
 */

/**
 * @brief
 * Opens a trace file.  
 *
 * The max file size is specified by @a file_size,
 * and the max number of files by @a file_count.  As the file fills,
 * it is rotated out;  file.0 is the second newest, file.1 is the next
 * older, etc.  
 * 
 * If no file is opened, tracing can still be done to
 * syslog but will not be saved in a file. 
 *
 * The trace_file_t * returned by trace_file_open must be 
 * passed to all other trace functions that log to this file.
 * 
 * If @a tp is @c NULL, @a file_name will be used for the trace file. Otherwise,
 * if @a file_name is @c NULL but @a tp is non-null, trace_file_open() will 
 * reuse the file name in @a tp (after closing and freeing the current trace 
 * file).
 * 
 * @param[in] tp
 *     Pointer to the previous trace file
 * @param[in] file_name
 *     The name of the new trace file to open
 * @param[in] file_size
 *     Maximum size of the trace file
 * @param[in] file_count
 *     
 * @return
 *    A pointer to a new trace file structure.
 *    If both @a tp and @a file_name are @c NULL, this function returns @c NULL.
 */
trace_file_t *
trace_file_open (trace_file_t *tp, const char *file_name, u_int32_t file_size,
		 u_int32_t file_count);

/**
 * @brief
 * Opens a fully buffered trace file.  
 *
 * This function behave exactly like trace_file_open(), with the exception that 
 * when a buffer is supplied to it, it is used to fully buffer the trace file 
 * stream
 * 
 * @param[in] tp
 *     Pointer to the previous trace file
 * @param[in] file_name
 *     The name of the new trace file to open
 * @param[in] file_size
 *     Maximum size of the trace file
 * @param[in] file_count
 * @param[in] stream_buffer
 *     The buffer to be used as a stream buffer for the trace file
 * @param[in] stream_buffer_size
 *     The size of the supplied stream buffer
 *     
 * @return
 *    A pointer to a new trace file structure.
 *    If both @a tp and @a file_name are @c NULL, this function returns @c NULL.
 */
trace_file_t *
trace_file_open_buffered (trace_file_t *tp, const char *file_name, 
                          u_int32_t file_size, u_int32_t file_count, 
                          char *stream_buffer, int stream_buffer_size);

/**
 * @brief
 * Closes a trace file and free the @c trace_file_t.
 *
 * @param[in] tp
 *    Pointer to a trace file
 */
void
trace_file_close (trace_file_t *tp);

/**
 * @brief
 * Issues a trace message.  
 *
 * If a logging level is set in @a type 
 * the message is sent to syslog as well.
 *
 * If called with @a tp set to @c NULL, trace() will log the message
 * to syslog if @c TRACE_LOG is included in @a type.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] type
 *     A @c TRACE_xxx constant     
 * @param[in] fmt
 *     Format string
 * @param[in] ap
 *     Variable-length argument list
 */
void
tracev (trace_file_t *tp, u_int32_t type, const char *fmt, va_list ap);

/**
 * @brief
 * TODO
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] type
 *     A TRACE_xxx constant
 * @param[in] tag
 *
 * @param[in] entry
 *
 * @param[in] fmt
 *     Format string
 * @param[in] ap
 *     Variable-length argument list
 */
void
tracev_event (trace_file_t *tp, u_int32_t type, const char *tag,
			  const char **entry, const char *fmt, va_list ap);

/**
 * @brief
 * Possibly issue a trace message, depending on if the specified
 * trace flag is enabled.  
 *
 * The trace flag may be 'or'ed with @c TRACE_LOG
 * if the message should be syslogged as well as traced.  If this is
 * set, the trace flag should also be 'or'ed with the desired @c LOG_xxx
 * value.  If the specified trace flag is enabled, the trace message is
 * formatted with a timestamp prepended, and written to the file (if
 * the file has been opened), and possibly written to the syslog (if
 * @c TRACE_LOG is included).
 *
 * @param[in] tp 
 *     Pointer to a trace file
 * @param[in] type
 *     A @c TRACE_xxx constant
 * @param[in] fmt
 *     A printf-like format string
 */
void
trace (trace_file_t *tp, u_int32_t type, const char *fmt, ...) 
		 PRINTFLIKE(3, 4);

/**
 * @brief
 * TODO
 * 
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] type
 *     A TRACE_xxx constant
 * @param[in] tag
 * 
 * @param[in] entry
 *
 * @param[in] fmt
 *     Format string
 */
void
trace_event (trace_file_t *tp, u_int32_t type, const char *tag,
	     const char **entry, const char *fmt, ...) PRINTFLIKE(5, 6);

/**
 * @brief
 * Enables the specified trace flag for tracing.  
 *
 * Setting @c TRACE_ALL will enable all trace flags.
 * 
 * @param[in] tp 
 *     Pointer to a trace file
 * @param[in] flag
 *     The trace flag to set
 */
void
trace_flag_set (trace_file_t *tp, u_int32_t flag);

/**
 * @brief
 * Disables the specified trace flag.  
 *
 * Clearing @c TRACE_ALL will disable all trace flags.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] flag
 *     The trace flag to clear
 */
void
trace_flag_clear (trace_file_t *tp, u_int32_t flag);

/**
 * @brief
 * Sets the aux flags to the specified value.  
 *
 * Note that we don't use set/clear primitives here;  
 * the caller needs to define all the flags
 * desired.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] flags
 *     The new trace flag value
 */
void
trace_aux_flags_set (trace_file_t *tp, u_int32_t flags);

/**
 * @brief
 * Determines if a particular trace flag is set.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] flag
 *     Flag to check the status of
 *
 * @return
 *     @c TRUE if the specified trace flag is enabled.
 */
u_int32_t
trace_flag_is_set (trace_file_t *tp, u_int32_t flag);

/**
 * @brief
 * Determines if any trace flag is set.
 *
 * @param[in] tp
 *     Pointer to a trace file
 *
 * @return
 *     @c true if any trace flag is enabled; @c false otherwise.
 */
bool
trace_flags_are_set (trace_file_t *tp);

/**
 * @brief
 * Sets the trace file permissions per mode.
 *  
 * @a mode indicates the permissions to be used (a la creat(2)/chmod(2)). 
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] mode
 *     Permission mode to be used
 * 
 * @return
 *     0 on success;
 *     -1 on failure
 */
int
trace_set_file_perms (trace_file_t *tp, mode_t mode);

/**
 * @brief
 * Returns the descriptor for the trace file.
 *
 * @param[in] tp
 *     Pointer to a trace file
 */
int
trace_fileno (trace_file_t *tp);

/**
 * @brief
 * Flushes buffered output to the trace file.
 * 
 * @param[in] tp
 *     Pointer to a trace file
 */
void
trace_file_flush (trace_file_t *tp);

/**
 * @brief
 * Sets the match regex filter for the trace file.
 *
 * The trace function will log message only if it matches this
 * regular-expression.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] regex
 *     Regular expression to match
 * 
 * @return
 *     The return value of this function is 0.
 */
int
trace_set_file_match (trace_file_t *tp, char *regex);

/**
 * @brief
 * Format the trace message 
 *
 * Format the trace message and return the pointer to the formatted string
 *
 * @note This API is not reentrant.
 *
 * @param[in] tp
 *     Pointer to a trace file
 * @param[in] msg_len
 *     Stores the length of the returned string here
 * @param[in] fmt
 *     Format string
 * @param[in] ap
 *     Variable-length argument list
 *
 * @return
 *     The pointer to the formatted string
 *
 * @sa trace_format_msg_r
 */
char *trace_format_msg (trace_file_t *traceptr, int *msg_len,
        const char *fmt, va_list ap);

/**
 * @brief
 * Format the trace message and return the pointer to the formatted string
 *
 * @param[in] traceptr
 *     Pointer to a trace file
 * @param[in] trace_buffer
 *     Pointer to a trace buffer. The caller has to provide a buffer.
 * @param[in] msg_len
 *     Stores the length of the returned string here
 * @param[in] fmt
 *     Format string
 * @param[in] ap
 *     Variable-length argument list
 *
 * @return
 *     The pointer to the formatted string
 *
 * @sa trace_format_msg
 */

char *trace_format_msg_r (trace_file_t *traceptr, char *trace_buffer,
        int *msg_len, const char *fmt, va_list ap);

FILE *trace_fileptr (trace_file_t *tp);


#ifdef __cplusplus
    }
}

#endif /* __cplusplus */

#endif /* __JNX_TRACE_H__ */

