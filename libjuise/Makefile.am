#
# $Id$
#

include ${top_srcdir}/warnings.mk

juiseincdir = ${includedir}/libjuise

AM_CFLAGS = \
    -I${top_srcdir} \
    ${LIBXML_CFLAG} \
    ${LIBXSLT_CFLAGS} \
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
    io/jsio.h \
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
    io/jsio.c \
    xml/extensions.c \
    xml/libxml.c
