#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
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
    juise.h

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS += \
 -DJUISE_DIR=\"${JUISE_DIR}\" \
 -DJUISE_SCRIPT_DIR=\"${JUISE_SCRIPT_DIR}\" \
 -DJUISE_CGI_DIR=\"${JUISE_CGI_DIR}\"

if HAVE_READLINE
LIBS += -L/opt/local/lib -lreadline
endif

if HAVE_LIBEDIT
LIBS += -ledit
endif

bin_PROGRAMS = juise

juise_SOURCES = juise.c server.c
juise_LDADD = ../libjuise/libjuise.la
juise_LDFLAGS = -static

man_MANS = juise.1x

EXTRA_DIST = juise.1x

old-install-exec-hook:
	mkdir -p ${libexecdir} ${bindir}
	cd ${bindir} && fromdir= && todir= && \
	    ( for file in xml-mode juise.cgi juise.fastcgi ; do \
		[ -f $${todir}$$file${EXEEXT} ] \
                     && mv -f $${todir}$$file${EXEEXT} \
                              $${todir}$$file${EXEEXT}.old ; \
		$(LN_S) $${fromdir}juise${EXEEXT} $${todir}$$file${EXEEXT} ; \
                fromdir=${bindir}/ ; \
                todir=${libexecdir}/ ; \
	    done )
