/*
 * $Id: trace.c 392616 2010-08-04 02:31:10Z builder $
 *
 * trace.c - tracing facility
 *
 * Copyright (c) 1998, 2001-2008, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <regex.h>

#include <libjuise/io/logging.h>
#include <libjuise/io/trace_priv.h>
#include <libjuise/io/trace.h>

#include <libjuise/io/rotate_log.h>
#include <regex.h>

#include "config.h"
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif /* HAVE_SYS_STATFS_H */

#if defined(HOSTPROG) && !defined(va_copy)
#define va_copy(dest, src) ((dest) = (src))
#endif

/*
 * Tracing facility
 *
 * The facility provides a way to generate timestamped trace messages to
 * a series of rotated files, to syslog, or both.  Up to 126 different
 * types of trace messages can be generated, and each trace type may
 * be enabled or disabled.
 *
 *
 * Calling sequence:
 *
 * trace_file_t *trace_file_open (trace_file_t *traceptr, const char *file_name,
 *                                u_int32_t file_size, u_int32_t file_count)
 *  Open a trace file.  The max file size is specified by file_size,
 *  and the max number of files by file_count.  As the file fills,
 *  it is rotated out;  file.0 is the second newest, file.1 is the next
 *  older, etc.  If no file is opened, tracing can still be done to
 *  syslog but will not be saved in a file. The trace_file_t * returned
 *  by trace_file_open must be passed to all other trace functions that
 *  log to this file.
 *
 * void trace_file_close (trace_file_t *traceptr)
 *  Close a trace file and free the trace_file_t.
 *
 * void trace_flag_set (trace_file_t *traceptr, u_int32_t trace_flag)
 *  Enable the specified trace flag for tracing.  Setting TRACE_ALL
 *  will enable all trace flags.
 *
 * void trace_flag_clear (trace_file_t *traceptr, u_int32_t trace_flag)
 *  Disable the specified trace flag.  Clearing TRACE_ALL will disable
 *  all trace flags.
 *
 * void trace (trace_file_t *traceptr, u_int32_t trace_flag, const char *fmt, ...)
 *  Possibly issue a trace message, depending on if the specified
 *  trace flag is enabled.  The trace flag may be 'or'ed with TRACE_LOG
 *  if the message should be syslogged as well as traced.  If this is
 *  set, the trace flag should also be 'or'ed with the desired LOG_xxx
 *  value.  If the specified trace flag is enabled, the trace message is
 *  formatted with a timestamp prepended, and written to the file (if
 *  the file has been opened), and possibly written to the syslog (if
 *  TRACE_LOG is included).
 *
 * u_int32_t trace_flag_is_set (trace_file_t *traceptr, u_int32_t trace_flag)
 *  Returns TRUE if the specified trace flag is enabled.
 *
 * int	trace_set_file_perms(trace_file_t *traceptr, mode_t mode)
 *  Sets the trace file permissions per mode.
 *  mode indicates the permissions to be used (a la creat(2)/chmod(2)). 
 *
 *  Returns 0 on success.
 *  Returns -1 on failure.
 * 
 */


/* Interesting constants. */

#define TRACE_BUFSIZE 512 /* Size of the trace buffer */
#define TIME_STRSIZE 26 /* Size of the time string */
#define TRACE_PREAMBLE_SIZE 15		/* "Mmm dd hh:mm:ss" */
#define TRACE_POSTAMBLE_SIZE 1		/* "\n" */
#define TRACE_MAX_PATHLEN FILENAME_MAX	/* Maximum length of file name */
#define TRACE_MSG_LEN 9000

/*
 * trace_aux_flags_set
 *
 * Set the aux flags to the specified value.  Note that we don't use
 * set/clear primitives here;  the caller needs to define all the flags
 * desired.
 */
void
trace_aux_flags_set (trace_file_t *tp, u_int32_t flags)
{
    tp->trace_aux_flags = flags;
}

/*
 * trace_flag_set
 *
 * Set a trace flag.  If TRACE_ALL is specified, all trace flags are set.
 */
void
trace_flag_set (trace_file_t *tp, u_int32_t flag)
{
    if (flag == TRACE_ALL) {
	tp->trace_bits = TRACE_ALL_BITS;
    } else {
	tp->trace_bits |= (1 << (flag >> TRACE_SHIFT)); /* Set the specified bit. */
    }
}

/*
 * trace_flag_clear
 *
 * Clear a trace flag.  If TRACE_ALL is specified, all trace flags are cleared.
 */
void
trace_flag_clear (trace_file_t *tp, u_int32_t flag)
{
    if (flag == TRACE_ALL) {
	tp->trace_bits = 0;
    } else {
	tp->trace_bits &= ~(1 << (flag >> TRACE_SHIFT)); /* Clear the bit. */
    }
}

/*
 * trace_flag_is_set
 *
 * Returns TRUE if the specified trace flag is set, or FALSE if not.
 *
 * Always returns true if TRACE_ALL is specified.
 */
u_int32_t
trace_flag_is_set (trace_file_t *tp, u_int32_t flag)
{
    if (flag == TRACE_ALL)
	return(1);
    return((tp->trace_bits & (1 << (flag >> TRACE_SHIFT))) != 0);
}

/*
 * trace_flags_are_set
 *
 * Returns true if any trace flag is set or false if not.
 *
 */
bool
trace_flags_are_set (trace_file_t *tp)
{
    return (tp->trace_bits == 0 ? false : true);
}

/*
 * trace_file_close_internal - close the trace file if it is open.
 */
static void
trace_file_close_internal (trace_file_t *traceptr)
{
    if (traceptr && traceptr->trace_fileptr) {
	fclose(traceptr->trace_fileptr);
	traceptr->trace_fileptr = NULL;
    }
}

/*
 * trace_file_close - close the trace file and free our control block.
 */
void
trace_file_close (trace_file_t *traceptr)
{
    if (traceptr == NULL) return;
    trace_file_close_internal(traceptr);
    if (traceptr->trace_file)
        free(traceptr->trace_file);
    if (traceptr->trace_match) regfree(&traceptr->trace_regex);
    free(traceptr);
}

int
trace_fileno (trace_file_t *tp)
{
    if (!tp || !tp->trace_fileptr) return -1;
    return fileno(tp->trace_fileptr);
}

FILE *
trace_fileptr (trace_file_t *tp)
{
    if (!tp || !tp->trace_fileptr) return NULL;
    return tp->trace_fileptr;
}

void
trace_file_flush (trace_file_t *tp)
{
    if (tp && tp->trace_fileptr)
	fflush(tp->trace_fileptr);
}

/*
 * trace_file_open_with_perms - open the trace file with the given permissions
 *
 * Closes any previously selected trace file.
 *
 * Returns NULL if we didn't succeed.
 */
static trace_file_t *
trace_file_open_with_perms (trace_file_t *traceptr, const char *file_name,
                            u_int32_t file_size, u_int32_t file_count, 
                            mode_t mode, char *stream_buffer, 
                            int stream_buffer_size,
                            bool free_traceptr)
{
    struct stat stbuf;
    const char* path = 0;
    int trace_size = 0;
    int trace_perms = 0;
    int fd;

    /* Choose the file name we'll be using */
    
    if (file_name) 
        path = file_name; /* we'll be using file_name */
    else if (traceptr) 
        path = traceptr->trace_file; /* we'll keep the existing path */
    else 
        return NULL;

    /* 
     * Make sure file is one we can use, and if it exists, the current 
     * size and permissions 
     */

    if (stat(path, &stbuf) < 0) {
	switch (errno) {
	  case ENOENT:
	    trace_size = 0;
	    /*
 	     * fopen() creates the file with this mode. See fopen(3).
	     */
	    trace_perms = mode ?: (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|
				   S_IWOTH); 
	    break;

	  default:
	    /* close and free the current trace file */
            trace_file_close_internal(traceptr);
            if (traceptr && free_traceptr) {
		free(traceptr->trace_file);
		free(traceptr);
                return NULL;
            } else {
                return traceptr;
            }
             
	}
    } else {
	/* file existed, make sure it's a regular file */
	if (!(stbuf.st_mode & S_IFREG)) {
            trace_file_close_internal(traceptr);
            if (traceptr && free_traceptr) {
		free(traceptr->trace_file);
		free(traceptr);
                return NULL;
            } else {
                return traceptr;
            }
	}

	trace_size = stbuf.st_size;
	trace_perms = mode ?: stbuf.st_mode;
    }

    /* Allocate a trace pointer, if we need one */
    
    if (traceptr == NULL) {
        traceptr = calloc(1, sizeof *traceptr);
        if (traceptr == NULL) return NULL;
    } else {
        trace_file_close_internal(traceptr);
    }
    
    /* Copy file_name, if needed */

    if (file_name) {
        char *new_file = strdup(file_name);
        
        if (traceptr->trace_file)
            free(traceptr->trace_file);
        traceptr->trace_file = new_file;
    }

    /* If we don't have a trace file path, die */
    
    if (!traceptr->trace_file) {
	if (traceptr) {
	    trace_file_close_internal(traceptr);
	    free(traceptr->trace_file);
	    free(traceptr);
	}
	return NULL;
    }
    
    /* Store the values */
    
    traceptr->trace_perms = trace_perms;
    traceptr->trace_size = trace_size;
    traceptr->trace_max_size = file_size;
    traceptr->trace_max_files = file_count;
    traceptr->stream_buffer = stream_buffer;
    traceptr->stream_buffer_size = stream_buffer_size;
    traceptr->trace_file_opened = true;
    traceptr->trace_file_fs_full = false;

    /* 
     * Open the file, silently fail if fopen fails, we never 
     * use trace_fileptr unless it's non-NULL
     */

    fd = open(traceptr->trace_file, O_WRONLY | O_APPEND | O_CREAT, 
	      trace_perms);
    traceptr->trace_fileptr = (fd >= 0) ? fdopen(fd, "a") : NULL;

    if (traceptr->trace_fileptr) {
        if (traceptr->stream_buffer && traceptr->stream_buffer_size) {
            setvbuf(traceptr->trace_fileptr, traceptr->stream_buffer, _IOFBF, 
                    traceptr->stream_buffer_size);
        } else {
            setlinebuf(traceptr->trace_fileptr);
        } 
    }

    return traceptr;
}

/*
 * trace_file_open_buffered - open the trace file with a stream buffer
 *
 * Closes any previously selected trace file.
 *
 * Returns NULL if we didn't succeed.
 */
trace_file_t *
trace_file_open_buffered (trace_file_t *traceptr, const char *file_name,
                          u_int32_t file_size, u_int32_t file_count, 
                          char *stream_buffer, int stream_buffer_size)
{
    return trace_file_open_with_perms(traceptr, file_name, file_size,
                                      file_count, 0, stream_buffer,
                                      stream_buffer_size, true);
}

/*
 * trace_file_open - open the trace file
 *
 * Closes any previously selected trace file.
 *
 * Returns NULL if we didn't succeed.
 */
trace_file_t *
trace_file_open (trace_file_t *traceptr, const char *file_name,
                 u_int32_t file_size, u_int32_t file_count)
{
    return trace_file_open_with_perms(traceptr, file_name, file_size,
                                      file_count, 0, NULL, 0, true);
}

/*
 * Change the trace file permissions.
 */
int
trace_set_file_perms(trace_file_t *traceptr, mode_t mode)
{
	if (traceptr->trace_perms != mode) {
		if (chmod(traceptr->trace_file, mode) < 0) {
			return(-1);
		}
		traceptr->trace_perms = mode;
	}
	return(0);
}

/*
 *  Rotate the trace files.
 */
static void
trace_rotate (trace_file_t *traceptr)
{
    unsigned flags = RLF_EMPTY;
    mode_t mode = traceptr->trace_perms;

    if (traceptr->trace_fileptr)
	fputs("Rotating trace files\n", traceptr->trace_fileptr);

    /* Close the current file. */

    trace_file_close_internal(traceptr);

    if (!(traceptr->trace_aux_flags & TRACE_AUX_FLAG_NOCOMPRESS)) {
        /* Set the flag to compress the log files. */
        flags |= RLF_COMPRESS;
    }

    rotate_log(traceptr->trace_file, traceptr->trace_max_files, flags);

    /* Open the new file and retain the existing permissions */
    trace_file_open_with_perms(traceptr, NULL, traceptr->trace_max_size,
                               traceptr->trace_max_files, mode,
                               traceptr->stream_buffer,
                               traceptr->stream_buffer_size, true);
}


/*
 * trace_msg - Write a message to the trace file.
 *
 * Returns a pointer to the formatted string.
 */

static char *
trace_msg (trace_file_t *traceptr, char *trace_buffer, u_int trace_it, int *msg_len,
           const char *fmt, va_list ap)
{
    char time_string[TIME_STRSIZE];
    struct timeval cur_time;
    char *time_buffer;
    char *return_buffer;
    int  trace_buf_remaining;
    int  trace_msg_len;
    int  trace_msg_start;
    int  fmt_len;
#ifdef HAVE_STATFS
    struct statfs fs_stat;
    int64_t fs_bytes_avail;
#endif

    trace_msg_len = 0;
    trace_buf_remaining = TRACE_BUFSIZE - TRACE_POSTAMBLE_SIZE;	/* save room */

    /* Get the current time and copy it into the buffer. */

    gettimeofday(&cur_time, NULL);
    time_buffer = ctime_r(&cur_time.tv_sec, time_string);
    bcopy(&time_buffer[4], trace_buffer, TRACE_PREAMBLE_SIZE);
    trace_msg_len += TRACE_PREAMBLE_SIZE;
    trace_buf_remaining -= TRACE_PREAMBLE_SIZE;

    /* If milliseconds are desired, add those. */
    if (traceptr && (traceptr->trace_aux_flags & TRACE_AUX_FLAG_MSEC)) {
	fmt_len = sprintf(trace_buffer + trace_msg_len, ".%06lu",
			  (unsigned long) cur_time.tv_usec);
	trace_msg_len += fmt_len;
	trace_buf_remaining -= fmt_len;
    }

    /* Add the blank. */

    trace_buffer[trace_msg_len] = ' ';
    trace_msg_len++;
    trace_buf_remaining--;
    trace_msg_start = trace_msg_len;

    /* Format the string.  Deal with overflows. */

    fmt_len = vsnprintf(trace_buffer + trace_msg_len, trace_buf_remaining,
			fmt, ap);
    if (fmt_len >= trace_buf_remaining)
	fmt_len = trace_buf_remaining - 1;

    trace_msg_len += fmt_len;
    trace_buf_remaining -= fmt_len;

    /* Add the trailing newline. */

    trace_buffer[trace_msg_len++] = '\n';
    trace_buffer[trace_msg_len++] = '\0';

    return_buffer = trace_buffer;
    *msg_len = trace_msg_len; 

    /* Bail if not actually tracing. */

    if (!traceptr || !traceptr->trace_file_opened || !trace_it) {
            return return_buffer;
    }

#ifdef HAVE_STATFS
    /* 
     * If FS containing the trace file was marked full, check if there is 
     * free space sufficient to store trace message now
     */
    if (traceptr->trace_file_fs_full) {
        if (statfs(traceptr->trace_file, &fs_stat) == -1) {
            return return_buffer;
        }

        fs_bytes_avail = fs_stat.f_bavail * (int64_t) fs_stat.f_bsize;
        if (fs_bytes_avail < (int64_t) trace_msg_len) {
            return return_buffer;
        } else {
            traceptr->trace_file_fs_full = false;
        }
    }
#endif	/* HAVE_STATFS */

    /* Log only if the line matches the regex */
    if (traceptr && traceptr->trace_match != REGEX_NONE) {
	int rc = regexec(&traceptr->trace_regex, return_buffer, 0, 0, 0);
	
	if ((traceptr->trace_match == REGEX_NEGATIVE_MATCH) && !rc)
	    return return_buffer;
	if ((traceptr->trace_match == REGEX_MATCH) && rc)
	    return return_buffer;
    }

    /* Rotate the files if the message won't fit. */

    if (traceptr->trace_size + trace_msg_len > traceptr->trace_max_size) {
	trace_rotate(traceptr);
        if (!traceptr->trace_fileptr) /* Bail if couldn't reopen trace file */
            return return_buffer;
    }

    /* Write it out. */

    if (!traceptr->trace_fileptr || 
          (fputs(trace_buffer, traceptr->trace_fileptr) == EOF)) { 
        /*
         * In case of GRES, PICS using NFS, need to reopen the trace file.
         */
        traceptr = trace_file_open_with_perms(traceptr, NULL, 
                               traceptr->trace_max_size, 
                               traceptr->trace_max_files,
                               traceptr->trace_perms,
                               traceptr->stream_buffer,
                               traceptr->stream_buffer_size,
                               false);

        if(traceptr->trace_fileptr) {
            fputs(trace_buffer, traceptr->trace_fileptr);
        }
    }

    /* FS containing the trace file is full */
    if (errno == ENOSPC) {
        traceptr->trace_file_fs_full = true;
    }

    /* Update the file size. */

    traceptr->trace_size += trace_msg_len;

    return return_buffer;
}

/*
 * trace
 *
 * Issue a trace message.  If a logging level is set in the trace
 * type, we should syslog the message as well.
 *
 * If called with traceptr == NULL, trace() will log the message
 * to syslog if TRACE_LOG is included in ``type''.
 */
void
tracev (trace_file_t *traceptr, u_int32_t type, const char *fmt, va_list ap)
{
    tracev_event(traceptr, type, NULL, NULL, fmt, ap);
}

void
tracev_event (trace_file_t *traceptr, u_int32_t type, const char *tag,
	      const char **entry, const char *fmt, va_list ap)
{
    char trace_buffer[TRACE_BUFSIZE];
    u_int log_it, trace_it = 0;
    int log_level;
    char *message;
    int msg_len;
    va_list newap;

    /* Note whether we're tracing. */
    if (traceptr)
        trace_it = trace_flag_is_set(traceptr, type & TRACE_ALL);

    /* Note whether we're logging it as well. */
    log_it = ((type & TRACE_LOG) != 0);
    log_level = type & (~TRACE_MASK);

    /* Bail if we're not tracing or logging. */
    if ((!trace_it || (traceptr && !traceptr->trace_file_opened)) && 
        !log_it)
	return;

    /* Format the trace message. */
    va_copy(newap, ap);
    message = trace_msg(traceptr, trace_buffer, trace_it, &msg_len, fmt, newap);
    va_end(newap);

    /* If logging, log it as well */
    if (log_it) {
        va_copy(newap, ap);
	vlogging_event(log_level, tag, NULL, entry, fmt, newap);
        va_end(newap);
    }
}

void
trace (trace_file_t *tp, u_int32_t type, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tracev(tp, type, fmt, ap);
    va_end(ap);
}

void
trace_event (trace_file_t *tp, u_int32_t type, const char *tag, 
	     const char **entry, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tracev_event(tp, type, tag, entry, fmt, ap);
    va_end(ap);
}

/*
 * This function will set the match regex filter for the trace file.
 * The trace function will log message only if it matches this regular-expression.
 */
int  
trace_set_file_match(trace_file_t *tp, char *regex)
{
    if (*regex == '!') {
	tp->trace_match = REGEX_NEGATIVE_MATCH;
	++regex;
    } else {
	tp->trace_match = REGEX_MATCH;
    }

    if (regcomp(&tp->trace_regex, regex, 
				    REG_EXTENDED | REG_ICASE | REG_NOSUB))
	tp->trace_match = REGEX_NONE;
    return (0);
}

/*
 * Format the trace message and return the pointer to the formatted string
 * Reentrant version of function trace_format_msg()
 */
char *
trace_format_msg_r (trace_file_t *traceptr, char *trace_buffer, int *msg_len,
        const char *fmt, va_list ap)
{
    return trace_msg(traceptr, trace_buffer, 0, msg_len, fmt, ap);
}
/*
 * Format the trace message and return the pointer to the formatted string
 */
char *
trace_format_msg (trace_file_t *traceptr, int *msg_len, const char *fmt,
        va_list ap)
{
    static char trace_buffer[TRACE_BUFSIZE];

    return trace_format_msg_r(traceptr, trace_buffer, msg_len, fmt, ap);
}
