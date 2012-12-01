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

extern trace_file_t *trace_file;
extern int opt_indent;
extern int opt_debugger;

void run_server (int fdin, int fdout, session_type_t stype);

void srv_add_dir (const char *jdir);
void srv_add_path (const char *jdir);

#endif /* JUISE_H */
