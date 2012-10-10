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
include ${top_srcdir}/warnings.mk

AM_CFLAGS = \
    -DLIBSLAX_XMLSOFT_NEED_PRIVATE \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    -Ilibssh2 \
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
    ${LIBSSH2_LIBS}

noinst_HEADERS = \
    buffer.h \
    channel.h \
    console.h \
    debug.h \
    forwarder.h \
    listener.h \
    local.h \
    mtypes.h \
    netconf.h \
    request.h \
    session.h \
    util.h \
    websocket.h

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS +=

bin_PROGRAMS = mixer

mixer_SOURCES = \
    buffer.c \
    channel.c \
    console.c \
    debug.c \
    forwarder.c \
    listener.c \
    mixer.c \
    mtypes.c \
    request.c \
    session.c \
    util.c \
    websocket.c

mixer_LDADD = ../libjuise/libjuise.la
mixer_LDFLAGS = -static

#man_MANS = mixer.1x
#EXTRA_DIST = mixer.1x
