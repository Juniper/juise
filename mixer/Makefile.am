#
# $Id$
#
# Copyright 2012, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

if JUISE_WARNINGS
include ${top_srcdir}/warnings.mk
endif

AM_CFLAGS = \
    -DLIBSLAX_XMLSOFT_NEED_PRIVATE \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
    ${WARNINGS}

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    -lexslt \
    ${LIBXML_LIBS}

noinst_HEADERS = \
    local.h

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS +=

bin_PROGRAMS = mixer

mixer_SOURCES = \
  afunix.c \
  channel.c \
  mixer.c \
  session.c \
  websocket.c

mixer_LDADD = ../libjuise/libjuise.la
mixer_LDFLAGS = -static

#man_MANS = mixer.1x
#EXTRA_DIST = mixer.1x
