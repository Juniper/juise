#
# $Id$
#

include ${top_srcdir}/warnings.mk

curlincdir = ${includedir}/libslax

EXTRA_DIST = curl.txt

AM_CFLAGS = \
    -DCURL_DISABLE_TYPECHECK \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAG} \
    ${LIBCURL_CFLAG} \
    ${WARNINGS}

AM_CFLAGS += \
 -DSLAX_EXTDIR=\"${SLAX_EXTDIR}\"

lib_LTLIBRARIES = libcurl.la
LIBS = \
    ${LIBXSLT_LIBS} \
    -lexslt \
    ${LIBXML_LIBS} \
    ${CURL_LIBS} \
    -L../../libjuise -ljuise \
    -L${SLAX_LIBDIR} -lslax

libcurl_la_SOURCES = \
    ext_curl.c

noinst_HEADERS = \
    ext_curl.h

libdir = ${SLAX_EXTDIR}

UGLY_NAME = http%3A%2F%2Fxml.juniper.net%2Fextensions%2Fcurl.ext

install-exec-hook:
	@DLNAME=`sh -c '. ${libdir}/libcurl.la ; echo $$dlname'`; \
                    echo Install link $$DLNAME "->" ${UGLY_NAME} "..." \
                    && cd ${SLAX_EXTDIR} \
	            && rm -f ${UGLY_NAME} \
		    && ${LN_S} $$DLNAME ${UGLY_NAME}
