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
extern int indent;
extern int use_debugger;

void
run_server (int fdin, int fdout, int use_junoscript);

#endif /* JUISE_H */
