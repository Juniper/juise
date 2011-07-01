#
# $Id$
#

include ${top_srcdir}/warnings.mk

AM_CFLAGS = \
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
    juise.h

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS += \
 -DJUISE_DIR=\"${JUISE_DIR}\" \
 -DJUISE_SCRIPT_DIR=\"${JUISE_SCRIPT_DIR}\"

if HAVE_READLINE
LIBS += -lreadline
endif

bin_PROGRAMS = juise

juise_SOURCES = juise.c server.c
juise_LDADD = ../libjuise/libjuise.la
juise_LDFLAGS = -static

man_MANS = juise.1x

EXTRA_DIST = juise.1x
