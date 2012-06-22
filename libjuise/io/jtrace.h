/*
 * $Id$
 *
 * Copyright (c) 2010-2011, Juniper Networks, Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

#ifndef LIBJUISE_IO_JTRACE_H
#define LIBJUISE_IO_JTRACE_H

void juise_log (const char *fmt, ...);
void juise_trace_init (const char *filename, trace_file_t **tfpp);

#endif /* LIBJUISE_IO_JTRACE_H */
