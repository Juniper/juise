#
# $Id$
#

include ${top_srcdir}/warnings.mk

AM_CFLAGS = \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    ${LIBXML_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBSLAX_CFLAGS} \
    ${WARNINGS}

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    -lexslt \
    ${LIBXML_LIBS}

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

if HAVE_READLINE
LIBS += -lreadline
endif

bin_PROGRAMS = juise

juise_SOURCES = juise.c
juise_LDADD = ../libjuise/libjuise.la
juise_LDFLAGS = -static

man_MANS = juise.1x

EXTRA_DIST = juise.1x
