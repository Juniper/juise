#
# $Id$
#
# Copyright 2012, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

if JUISE_WARNINGS_HIGH
JUISE_WARNINGS = HIGH
endif
if HAVE_GCC
GCC_WARNINGS = yes
endif
include ${top_srcdir}/warnings.mk

AM_CFLAGS = \
    -DLIBSLAX_XMLSOFT_NEED_PRIVATE \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    -I${top_builddir}/libjuise \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
    ${LIBSSH2_CFLAGS} \
    ${WARNINGS}

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    -lexslt \
    ${LIBXML_LIBS} \
    -lssh2 \
    -lsqlite3

noinst_HEADERS = \
    delta.h

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS +=

bin_PROGRAMS = delta

delta_SOURCES = \
    delta.c

delta_LDADD = ../libjuise/libjuise.la
delta_LDFLAGS = -static

#man_MANS = delta.1x
#EXTRA_DIST = delta.1x
