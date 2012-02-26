#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

juiseincdir = ${includedir}/libjuise

if JUISE_WARNINGS
include ${top_srcdir}/warnings.mk
endif

AM_CFLAGS = \
    -DLIBSLAX_XMLSOFT_NEED_PRIVATE \
    -DJUISE_LIBEXECDIR=\"${JUISE_LIBEXECDIR}\" \
    -I${top_srcdir} \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
    ${WARNINGS} 

lib_LTLIBRARIES = libjuise.la

juiseinc_HEADERS = 

noinst_HEADERS = \
    juiseconfig.h \
    common/allocadup.h \
    common/aux_types.h \
    common/bits.h \
    data/parse.h \
    data/parse_ip.h \
    data/patricia.h \
    env/env.h \
    env/env_paths.h \
    io/dbgpr.h \
    io/fbuf.h \
    io/filecopy.h \
    io/logging.h \
    io/pid_lock.h \
    io/rotate_log.h \
    io/trace.h \
    io/trace_priv.h \
    string/strextra.h \
    time/time_const.h \
    time/timestr.h \
    xml/client.h \
    xml/extensions.h \
    xml/jsio.h \
    xml/juisenames.h \
    xml/libxml.h \
    xml/xmllib.h \
    xml/xmllib_pub.h \
    xml/xmlrpc.h \
    xml/xmlutil.h

JUISEHEADERS = ${noinst_HEADERS}

libjuise_la_SOURCES = \
    data/patricia.c \
    data/parse_ipaddr.c \
    string/strndup.c \
    string/strdupf.c \
    time/timestr.c \
    io/fbuf.c \
    io/trace.c \
    io/logging.c \
    io/rotate_log.c \
    xml/extensions.c \
    xml/jsio.c \
    xml/libxml.c

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    ${LIBXML_LIBS}
