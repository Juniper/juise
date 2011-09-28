/*
 * $Id$
 *
 * Copyright (c) 2011, Juniper Networks, Inc.
 * All rights reserved.
 * See ../Copyright for the status of this software
 */

#ifndef JUISE_H
#define JUISE_H

extern trace_file_t *trace_file;
extern int opt_indent;
extern int opt_debugger;

void
run_server (int fdin, int fdout, session_type_t stype);

void
srv_set_juise_dir (const char *jdir);

#endif /* JUISE_H */
