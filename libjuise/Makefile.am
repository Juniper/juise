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

if JUISE_WARNINGS_HIGH
JUISE_WARNINGS = HIGH
endif
if HAVE_GCC
GCC_WARNINGS = yes
endif
include ${top_srcdir}/warnings.mk

AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS = \
    -DLIBSLAX_XMLSOFT_NEED_PRIVATE \
    -DJUISE_LIBEXECDIR=\"${JUISE_LIBEXECDIR}\" \
    -I${top_srcdir} \
    -I${top_builddir} \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
    ${WARNINGS} 

lib_LTLIBRARIES = libjuise.la

juiseinc_HEADERS = 

noinst_HEADERS = \
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
    io/jtrace.h \
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
    string/snprintf_safe.c \
    time/timestr.c \
    io/fbuf.c \
    io/jtrace.c \
    io/trace.c \
    io/logging.c \
    io/pid_lock.c \
    io/rotate_log.c \
    xml/client.c \
    xml/xml_escape.c \
    xml/xml_escaped_size.c \
    xml/extensions.c \
    xml/jsio.c \
    xml/libxml.c \
    xml/xmlinput.c \
    xml/xmlrpc.c \
    xml/xml_parse_attributes.c \
    xml/xml_unescape.c

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    ${LIBXML_LIBS}

svnignore:
	svn propset svn:ignore -F ${srcdir}/.svnignore ${srcdir}


UGLY_NAME = jcs.prefix:http%3A%2F%2Fxml.juniper.net%2Fjunos%2Fcommit-scripts%2F1.0.ext

#
# No .ext extension so the libslax dynamic loaded knows this isn't an
# "extension" namespace (ala extension-element-prefixes)
#
UGLY_XNM = http%3A%2F%2Fxml.juniper.net%2Fxnm%2F1.1%2Fxnm

install-exec-hook:
	@DLNAME=`sh -c '. ./libjuise.la ; echo $$dlname'`; \
		if [ x"$$DLNAME" = x ]; \
                    then DLNAME=${LIBNAME}.${SLAX_LIBEXT}; fi ; \
		if [ "$(build_os)" = "cygwin" ]; \
		    then DLNAME="../bin/$$DLNAME"; fi ; \
		echo Install link $$DLNAME "->" ${UGLY_NAME} "..." ; \
		mkdir -p ${DESTDIR}${SLAX_EXTDIR} ; \
		cd ${DESTDIR}${SLAX_EXTDIR} \
		&& chmod +w . \
		&& prefix=`echo ${UGLY_NAME} | awk -F: '{ print $$1 }'` \
		&& url=`echo ${UGLY_NAME} | awk -F: '{ print $$2 }'` \
		&& rm -f $$prefix $$url \
		&& ${LN_S} -f ${JUISE_LIBDIR}/$$DLNAME $$url \
		&& ${LN_S} -f $$url $$prefix \
		&& ${LN_S} -f ${UGLY_XNM} xnm.prefix

uninstall-hook:
	@DLNAME=`sh -c '. ./libjuise.la ; echo $$dlname'`; \
		if [ x"$$DLNAME" = x ]; \
                    then DLNAME=${LIBNAME}.${SLAX_LIBEXT}; fi ; \
		if [ "$(build_os)" = "cygwin" ]; \
		    then DLNAME="../bin/$$DLNAME"; fi ; \
		echo Install link $$DLNAME "->" ${UGLY_NAME} "..." ; \
		mkdir -p ${DESTDIR}${SLAX_EXTDIR} ; \
		cd ${DESTDIR}${SLAX_EXTDIR} \
		&& chmod +w . \
		&& prefix=`echo ${UGLY_NAME} | awk -F: '{ print $$1 }'` \
		&& url=`echo ${UGLY_NAME} | awk -F: '{ print $$2 }'` \
		&& rm -f $$prefix $$url \
		&& rm -f xnm.prefix
