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

AM_CFLAGS += -DJUISE_DIR=\"${JUISE_DIR}\"

bin_PROGRAMS = juise-askpass

juise_askpass_SOURCES = juise-askpass.c
juise_askpass_LDADD = ../libjuise/libjuise.la
juise_askpass_LDFLAGS = -static
