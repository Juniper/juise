/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef JUISE_H
#define JUISE_H

/*
 * Formats that can be specified when executing an RPC
 */
#define RPC_FORMAT_HTML "html"
#define RPC_FORMAT_JSON "json"
#define RPC_FORMAT_TEXT "text"
#define RPC_FORMAT_XML "xml"

/*
 * Available media types
 */
#define MEDIA_TYPE_APPLICATION_JSON "application/json"
#define MEDIA_TYPE_APPLICATION_XML "application/xml"
#define MEDIA_TYPE_TEXT_HTML "text/html"
#define MEDIA_TYPE_TEXT_PLAIN "text/plain"

extern trace_file_t *trace_file;
extern int opt_indent;
extern int opt_debugger;

/*
 * Structure to hold mapping between media type and rpc input/output format
 */
typedef struct {
    const char *rmtm_media_type;
    const char *rmtm_output_format;
} rpc_media_type_map_t;

void run_server (int fdin, int fdout, session_type_t stype);

void srv_add_dir (const char *jdir);
void srv_add_path (const char *jdir);

#endif /* JUISE_H */
