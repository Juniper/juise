/*
 * $Id$
 *
 * Copyright (c) 2000-2007, 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 *
 * Miscellaneous xml functions
 */

#ifndef JUNOSCRIPT_XMLLIB_H
#define JUNOSCRIPT_XMLLIB_H

#include <libjuise/xml/xmllib_pub.h>

boolean xml_error (void *peer,
                   boolean parse_error,
                   const char *source_daemon,
                   const char *filename,
                   const char *line_number,
                   const char *column,
                   const char *token,
                   const char *edit_path,
                   const char *fmt, ...);

boolean xml_warning (void *peer,
                     const char *source_daemon,
                     const char *filename,
                     const char *line_number,
                     const char *column,
                     const char *token,
                     const char *fmt, ...);

boolean xml_initial_handshake (void *peer, const char *hostname,
			       int *versp, int flags);

boolean xml_initial_handshake_send (void *peer, const char *hostname,
			       const char *apiname);

boolean xml_check_initial_attributes (char *list, int *versp);

/* 
 * xml_send_method_t that emits to a file or stdout if peer = null
 */
 
void xml_file_send_line (FILE* file, const char* fmt, ...);

/* 
 * emits the daemon version either as XML or as text to the 
 * specified file, or to stdout if file = null
 */
 
void show_daemon_version (FILE* file, const int version_level, 
                          const boolean as_xml);
                          

/*
 * Emit a progress indicator
 */
void
xml_emit_progress_indicator (void *peer, const char *message);

void enable_autoinstallation (void);
void disable_autoinstallation (void);

#endif /* JUNOSCRIPT_XMLLIB_H */



