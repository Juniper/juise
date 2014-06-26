#
# $Id$
#
# Copyright 2011, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

if JUISE_WARNINGS_HIGH
JUISE_WARNINGS = HIGH
endif
include ${top_srcdir}/warnings.mk

#
# This Makefile tells us how to build our lighttpd module mod_juise,
# which is a fairly trivial module.  Unfortunately, building it is
# not trivial.  Or fun.  Or simple.  Or sturdy.  My solution here
# is entirely too fragile, but the complexity should be contained
# and when it fails, you should get a compile error rather than
# some sort of bizarre runtime behavior.
#
# For example, we need our mod_juise.c needs lighttpd's <server.h>,
# but that needs lighttpd's "config.h", so we need to avoid finding
# our own "juiseconfig.h".  Having included that file, we need to make sure
# our CFLAGS and LIBS are identical to lighttpd.  If not, our
# structure references will not align with the rest of lighttpd and
# some criminally bizarre results will occur.
#

#
# We need to remove the "-I.." from DEFAULT_INCLUDES, but automake
# does not make this easy.
#
MY_DEFAULT_INCLUDES = -I. -I$(srcdir)

COMPILE = $(CC) $(DEFS) $(MY_DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
        $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) $(DEFS) \
        $(MY_DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CFLAGS) $(CFLAGS)

#
# lighttpd doesn't handle some of our normal compile flags
#
W1 = $(subst -Wmissing-prototypes,,${WARNINGS})
W2 = $(subst -Wstrict-prototypes,,${W1})
MYWARNINGS = ${W2}

AM_CFLAGS = \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    -I.. \
    -I${LIGHTTPD_OBJ} \
    -I${LIGHTTPD_SRC}/src \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
    ${PCRE_CFLAGS} \
    ${MYWARNINGS}

OLD_CFLAGS = \
    -I${top_srcdir} \
    -I${top_srcdir}/libjuise \
    ${LIBSLAX_CFLAGS} \
    ${LIBXSLT_CFLAGS} \
    ${LIBXML_CFLAGS} \
ignored

if JUISE_DEBUG
AM_CFLAGS += -g -DJUISE_DEBUG
endif

AM_CFLAGS += \
 -DJUISE_DIR=\"${JUISE_DIR}\" \
 -DJUISE_SCRIPT_DIR=\"${JUISE_SCRIPT_DIR}\"

noinst_HEADERS = \
    mod_juise.h

lib_LTLIBRARIES = mod_juise.la
mod_juise_la_SOURCES = mod_juise.c
mod_juise_la_LDFLAGS = -module -export-dynamic -avoid-version -no-undefined
#mod_juise_la_LIBADD = liblightcomp.la
mod_juise_la_LDFLAGS += -L${LIGHTTPD_MOD}

LIBS = \
    ${LIBSLAX_LIBS} \
    ${LIBXSLT_LIBS} \
    -lexslt \
    ${LIBXML_LIBS} \
    ${LIBCURL_LIBS}
