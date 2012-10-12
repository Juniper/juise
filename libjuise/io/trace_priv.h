/*
 * $Id$
 *
 * Copyright (c) 2009-2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * @file trace_priv.h
 * @brief 
 * Tracing facility APIs used internally
 */
#ifndef __JNX_TRACE_PRIV_H__
#define __JNX_TRACE_PRIV_H__

#include "juiseconfig.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

typedef enum {
    REGEX_NONE,
    REGEX_NEGATIVE_MATCH,
    REGEX_MATCH
} match_t;

/*
 * Tracing structure
 */
struct trace_file_s {
    uint32_t    trace_bits;		/* Settings of trace bits */
    uint32_t    trace_aux_flags;	/* Auxiliary trace flags */
    FILE        *trace_fileptr;		/* File pointer */
    char        *trace_file;		/* File name */
    uint32_t    trace_size;		/* Size of current trace file */
    uint32_t    trace_max_size;		/* Maximum desired file size */
    u_int       trace_max_files;	/* Maximum number of files desired */
    mode_t      trace_perms;		/* Permissions on trace file */
    match_t	trace_match;		/* Is regex matching needed */
    regex_t	trace_regex;		/* Regex to match */
    char        *stream_buffer;		/* Optional stream buffer */
    int         stream_buffer_size;	/* Stream buffer size (if exists) */
    bool        trace_file_opened;      /* check whether needed to reopen */
    bool        trace_file_fs_full;     /* FS containing trace file is FULL */
};

    
#endif /* __JNX_TRACE_H__ */

