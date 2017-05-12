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

#include <err.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pwd.h>
#include <sys/socket.h>

#include <libxml/tree.h>
#include <libxml/dict.h>
#include <libxml/uri.h>
#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/transform.h>
#include <libxml/HTMLparser.h>
#include <libxml/xmlsave.h>
#include <libexslt/exslt.h>
#include <libxslt/xsltutils.h>

#include "juiseconfig.h"
#include <libslax/slax.h>
#include <libpsu/psustring.h>
#include <libpsu/psulog.h>
#include <libslax/slaxdata.h>
#include <libslax/xmlsoft.h>

#include <libjuise/string/strextra.h>
#include <libjuise/time/timestr.h>
#include <libjuise/xml/libxml.h>
#include <libjuise/io/trace.h>
#include <libjuise/io/jtrace.h>
#include <libjuise/xml/jsio.h>
#include <libjuise/xml/extensions.h>
#include <libjuise/xml/juisenames.h>

static trace_file_t *juise_trace_file;

void
juise_log (const char *fmt, ...)
{
    va_list vap;

    va_start(vap, fmt);

    if (juise_trace_file) {
	tracev(juise_trace_file, TRACE_ALL, fmt, vap);
    } else {
        vfprintf(stderr, fmt, vap);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    va_end(vap);
}

static void
juise_trace (void *vfp, lx_node_t *nodep, const char *fmt, ...)
{
    trace_file_t *tp = vfp;
    FILE *fp = tp ? trace_fileptr(tp) : stderr;
    va_list vap;

    va_start(vap, fmt);

    if (nodep) {
	xmlSaveCtxt *handle;

	fprintf(fp, "XML Content (%d)\n", nodep->type);
	fflush(fp);
	handle = xmlSaveToFd(fileno(fp), NULL,
			     XML_SAVE_FORMAT | XML_SAVE_NO_DECL);
	if (handle) {
	    xmlSaveTree(handle, nodep);
	    xmlSaveFlush(handle);
	    xmlSaveClose(handle);
	    fflush(fp);
	}

    } else if (tp) {
	tracev(tp, TRACE_ALL, fmt, vap);
    } else {
	vfprintf(fp, fmt, vap);
    }

    va_end(vap);
}

static void
juise_log_callback (void *vfp, const char *fmt, va_list vap)
{
    trace_file_t *tp = vfp;
    FILE *fp = tp ? trace_fileptr(tp) : stderr;

    if (tp) {
	tracev(tp, TRACE_ALL, fmt, vap);
    } else {
	vfprintf(fp, fmt, vap);
	fprintf(stderr, "\n");
	fflush(stderr);
    }
}

void
juise_trace_init (const char *filename, trace_file_t **tfpp)
{
    trace_file_t *tfp;

    if (slaxFilenameIsStd(filename)) {
	slaxTraceEnable(juise_trace, NULL);
	slaxLogEnableCallback(juise_log_callback, NULL);

    } else {
	tfp = trace_file_open(NULL, filename, 1000000, 10);
	if (tfp == NULL || trace_fileptr(tfp) == NULL)
	    errx(1, "could not open trace file: %s", filename);
		
	juise_trace_file = *tfpp = tfp;
	slaxTraceEnable(juise_trace, tfp);
	slaxLogEnableCallback(juise_log_callback, tfp);
    }
}

