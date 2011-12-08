#
# $Id$
#

#
# Release numbering: even numbered dot releases are official ones, and
# odd numbers are development ones.  The svn version of this file will
# only (ONLY!) ever (EVER!) contain odd numbers, so I'll always know if
# a particular user has the dist or svn release.
#

AC_PREREQ(2.2)
AC_INIT([juise], [0.3.0], [phil@juniper.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
LT_INIT([])
AC_DISABLE_SHARED

case $prefix in
     NONE)
	prefix=/usr/local
	;;
esac

# Support silent build rules.  Requires at least automake-1.11.
# Disable with "configure --disable-silent-rules" or "make V=1"
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

#
# libxslt 1.1.26 has a fix for AVT terminating with close braces
#
LIBXML_REQUIRED_VERSION=2.7.7
LIBXSLT_REQUIRED_VERSION=1.1.26
LIBSLAX_REQUIRED_VERSION=0.10.0

AC_PROG_CC
AC_PROG_INSTALL
AC_CONFIG_MACRO_DIR([m4])

AC_PATH_PROG(BASENAME, basename, /usr/bin/basename)
AC_PATH_PROG(CAT, cat, /bin/cat)
AC_PATH_PROG(CHMOD, chmod, /bin/chmod)
AC_PATH_PROG(CP, cp, /bin/cp)
AC_PATH_PROG(DIFF, diff, /usr/bin/diff)
AC_PATH_PROG(MKDIR, mkdir, /bin/mkdir)
AC_PATH_PROG(MV, mv, /bin/mv)
AC_PATH_PROG(RM, rm, /bin/rm)
AC_PATH_PROG(SED, sed, /bin/sed)

AC_PATH_PROG(PATH_SSH, ssh, /usr/bin/ssh)
AC_PATH_PROG(PATH_GZIP, gzip, /usr/bin/gzip)

AC_PROG_LN_S

AC_STDC_HEADERS

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero memmove strchr strcspn strerror strspn])
AC_CHECK_FUNCS([sranddev srand strlcpy])
AC_CHECK_FUNCS([fdopen getrusage])
AC_CHECK_FUNCS([gettimeofday ctime])
AC_CHECK_FUNCS([getpass])
AC_CHECK_FUNCS([sysctlbyname])
AC_CHECK_FUNCS([flock])
AC_CHECK_FUNCS([statfs])
AC_CHECK_FUNCS([strnstr])

AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([ctype.h errno.h stdio.h stdlib.h])
AC_CHECK_HEADERS([string.h sys/param.h unistd.h])
AC_CHECK_HEADERS([sys/sysctl.h])
AC_CHECK_HEADERS([stdint.h sys/statfs.h])

AC_CHECK_LIB([xml2], [xmlNewParserCtxt])
AC_CHECK_LIB([xslt], [xsltInit])
AC_CHECK_LIB([readline], [readline])

AC_CHECK_MEMBER([struct passwd.pw_class],
    [HAVE_PWD_CLASS=yes ;
        AC_DEFINE([HAVE_PWD_CLASS], [1], [Have struct pwd.pw_class])],
    [HAS_PWD_CLASS=no], [[#include <pwd.h>]])

AC_CHECK_MEMBER([struct sockaddr_un.sun_len],
    [HAVE_SUN_LEN=yes ;
        AC_DEFINE([HAVE_SUN_LEN], [1], [Have struct sockaddr_un.sun_len])],
    [HAS_SUN_LEN=no], [[#include <sys/un.h>]])

dnl
dnl Some packages need to be checked against version numbers so we
dnl define a function here for later use
dnl
AC_DEFUN([VERSION_TO_NUMBER],
[`$1 | sed -e 's/lib.* //' | awk 'BEGIN { FS = "."; } { printf "%d", ([$]1 * 1000 + [$]2) * 1000 + [$]3;}'`])

AC_MSG_CHECKING([whether to build with debugging])
AC_ARG_ENABLE([debug],
    [  --enable-debug    Turn on debugging],
    [JUISE_DEBUG=$enable_debug],
    [JUISE_DEBUG=no])
AC_MSG_RESULT([$JUISE_DEBUG])
AM_CONDITIONAL([JUISE_DEBUG], [test "$JUISE_DEBUG" = "yes"])
if test "JUISE_DEBUG" = "yes" ; then
    AC_DEFINE([JUISE_DEBUG], [1], [Enable debugging])
fi

AC_MSG_CHECKING([whether to build with readline])
AC_ARG_ENABLE([readline],
    [  --enable-readline    Enable support for GNU readline],
    [HAVE_READLINE=$enable_readline],
    [HAVE_READLINE=$ac_cv_lib_readline_readline])
AC_MSG_RESULT([$HAVE_READLINE])
AM_CONDITIONAL([HAVE_READLINE], [test "$HAVE_READLINE" = "yes"])
if test "$HAVE_READLINE" = "yes" ; then
    AC_DEFINE([HAVE_READLINE], [1], [Enable support for GNU readline])
fi

AC_MSG_CHECKING([whether to build with __printflike])
AC_ARG_ENABLE([printflike],
    [  --enable-printflike    Enable use of GCC __printflike attribute],
    [HAVE_PRINTFLIKE=$enable_printflike],
    [HAVE_PRINTFLIKE=no])
AC_MSG_RESULT([$HAVE_PRINTFLIKE])
AM_CONDITIONAL([HAVE_PRINTFLIKE], [test "$HAVE_PRINTFLIKE" = "yes"])
if test "$HAVE_PRINTFLIKE" = "yes" ; then
    AC_DEFINE([HAVE_PRINTFLIKE], [1], [Enable use of GCC __printflike])
fi

#
# ---- start of noise
#

#
# ---- handle libxml2
#

LIBXML_CONFIG_PREFIX=""
LIBXML_SRC=""

AC_ARG_WITH(libxml-prefix,
        [  --with-libxml-prefix=[PFX]           Specify location of libxml config],
        LIBXML_CONFIG_PREFIX=$withval
)
        
AC_ARG_WITH(libxml-include-prefix,
        [  --with-libxml-include-prefix=[PFX]   Specify location of libxml headers],
        LIBXML_CFLAGS="-I$withval"
)

AC_ARG_WITH(libxml-libs-prefix,
        [  --with-libxml-libs-prefix=[PFX]      Specify location of libxml libs],
        LIBXML_LIBS="-L$withval"
)

AC_ARG_WITH(libxml-src,
        [  --with-libxml-src=[DIR]              For libxml thats not installed yet (sets all three above)],
        LIBXML_SRC="$withval"
)
AC_SUBST(LIBXML_SRC)

dnl
dnl where is xml2-config
dnl

AC_SUBST(LIBXML_REQUIRED_VERSION)
AC_MSG_CHECKING(for libxml libraries >= $LIBXML_REQUIRED_VERSION) 
if test "x$LIBXML_CONFIG_PREFIX" != "x"
then
        XML_CONFIG=${LIBXML_CONFIG_PREFIX}/bin/xml2-config
else
        XML_CONFIG=xml2-config
fi

dnl
dnl make sure xml2-config is executable,
dnl test version and init our variables
dnl

if ${XML_CONFIG} --libs > /dev/null 2>&1
then
	XMLVERS=`$XML_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $XMLVERS) -ge VERSION_TO_NUMBER(echo $LIBXML_REQUIRED_VERSION)
	then
		LIBXML_LIBS="$LIBXML_LIBS `$XML_CONFIG --libs`"
		LIBXML_CFLAGS="$LIBXML_CFLAGS `$XML_CONFIG --cflags`"
		AC_MSG_RESULT($XMLVERS found)
	else
		AC_MSG_ERROR(Version $XMLVERS found. You need at least libxml2 $LIBXML_REQUIRED_VERSION for this version of libxslt)
	fi
else
	AC_MSG_ERROR([Could not find libxml2 anywhere, check ftp://xmlsoft.org/.])
fi

AC_SUBST(XML_CONFIG)
AC_SUBST(LIBXML_LIBS)
AC_SUBST(LIBXML_CFLAGS)



#
# ---- handle libxslt
#

LIBXSLT_CONFIG_PREFIX=""
LIBXSLT_SRC=""

AC_ARG_WITH(libxslt-prefix,
        [  --with-libxslt-prefix=[PFX]           Specify location of libxslt config],
        LIBXSLT_CONFIG_PREFIX=$withval
)
        
AC_ARG_WITH(libxslt-include-prefix,
        [  --with-libxslt-include-prefix=[PFX]   Specify location of libxslt headers],
        LIBXSLT_CFLAGS="-I$withval"
)

AC_ARG_WITH(libxslt-libs-prefix,
        [  --with-libxslt-libs-prefix=[PFX]      Specify location of libxslt libs],
        LIBXSLT_LIBS="-L$withval"
)

AC_ARG_WITH(libxslt-src,
        [  --with-libxslt-src=[DIR]              For libxslt thats not installed yet (sets all three above)],
        LIBXSLT_SRC="$withval"
)
AC_SUBST(LIBXSLT_SRC)

dnl
dnl where is xslt-config
dnl

AC_SUBST(LIBXSLT_REQUIRED_VERSION)
AC_MSG_CHECKING(for libxslt libraries >= $LIBXSLT_REQUIRED_VERSION) 
if test "x$LIBXSLT_CONFIG_PREFIX" != "x"
then
        XSLT_CONFIG=${LIBXSLT_CONFIG_PREFIX}/bin/xslt-config
else
        XSLT_CONFIG=xslt-config
fi

dnl
dnl make sure xslt-config is executable,
dnl test version and init our variables
dnl

if ${XSLT_CONFIG} --libs > /dev/null 2>&1
then
	XSLTVERS=`$XSLT_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $XSLTVERS) -ge VERSION_TO_NUMBER(echo $LIBXSLT_REQUIRED_VERSION)
	then
		LIBXSLT_LIBS="$LIBXSLT_LIBS `$XSLT_CONFIG --libs`"
		LIBXSLT_CFLAGS="$LIBXSLT_CFLAGS `$XSLT_CONFIG --cflags`"
		AC_MSG_RESULT($XSLTVERS found)
	else
		AC_MSG_ERROR(Version $XSLTVERS found. You need at least libxslt $LIBXSLT_REQUIRED_VERSION for this version of libxslt)
	fi
else
	AC_MSG_ERROR([Could not find libxslt anywhere, check ftp://xmlsoft.org/.])
fi

AC_SUBST(XSLT_CONFIG)
AC_SUBST(LIBXSLT_LIBS)
AC_SUBST(LIBXSLT_CFLAGS)



#
# ---- handle libslax
#

LIBSLAX_CONFIG_PREFIX=""
LIBSLAX_SRC=""

AC_ARG_WITH(libslax-prefix,
        [  --with-libslax-prefix=[PFX]           Specify location of libslax config],
        LIBSLAX_CONFIG_PREFIX=$withval
)
        
AC_ARG_WITH(libslax-include-prefix,
        [  --with-libslax-include-prefix=[PFX]   Specify location of libslax headers],
        LIBSLAX_CFLAGS="-I$withval"
)

AC_ARG_WITH(libslax-libs-prefix,
        [  --with-libslax-libs-prefix=[PFX]      Specify location of libslax libs],
        LIBSLAX_LIBS="-L$withval"
)

AC_ARG_WITH(libslax-src,
        [  --with-libslax-src=[DIR]              For libslax thats not installed yet (sets all three above)],
        LIBSLAX_SRC="$withval"
)
AC_SUBST(LIBSLAX_SRC)

dnl
dnl where is slax-config
dnl

AC_SUBST(LIBSLAX_REQUIRED_VERSION)
AC_MSG_CHECKING(for libslax libraries >= $LIBSLAX_REQUIRED_VERSION) 
if test "x$LIBSLAX_CONFIG_PREFIX" != "x"
then
        SLAX_CONFIG=${LIBSLAX_CONFIG_PREFIX}/bin/slax-config
else
        SLAX_CONFIG=slax-config
fi

dnl
dnl make sure slax-config is executable,
dnl test version and init our variables
dnl

if ${SLAX_CONFIG} --libs > /dev/null 2>&1
then
	SLAXVERS=`$SLAX_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $SLAXVERS) -ge VERSION_TO_NUMBER(echo $LIBSLAX_REQUIRED_VERSION)
	then
		LIBSLAX_LIBS="$LIBSLAX_LIBS `$SLAX_CONFIG --libs`"
		LIBSLAX_CFLAGS="$LIBSLAX_CFLAGS `$SLAX_CONFIG --cflags`"
		SLAX_EXTDIR="`$SLAX_CONFIG --extdir`"
		SLAX_LIBDIR="`$SLAX_CONFIG --libdir`"
		AC_MSG_RESULT($SLAXVERS found)
	else
		AC_MSG_ERROR(Version $SLAXVERS found. You need at least libslax $LIBSLAX_REQUIRED_VERSION for this version of juise)
	fi
else
	AC_MSG_ERROR([Could not find libslax anywhere, check http://code.google.com/p/libslax.])
fi

AC_SUBST(SLAX_CONFIG)
AC_SUBST(LIBSLAX_CFLAGS)
AC_SUBST(LIBSLAX_LIBS)
AC_SUBST(SLAX_EXTDIR)
AC_SUBST(SLAX_LIBDIR)

#
# ---- end of noise
#

AC_MSG_CHECKING(for perl regular expressions support)
AC_ARG_WITH(pcre, AC_HELP_STRING([--with-pcre],[Enable pcre support (default yes)]),
    [WITH_PCRE=$withval],[WITH_PCRE=yes])
AC_MSG_RESULT([$WITH_PCRE])

if test "$WITH_PCRE" != "no"; then
  AC_PATH_PROG(PCRECONFIG, pcre-config)

  if test x"$PCRECONFIG" != x; then
    PCRE_LIB=`$PCRECONFIG --libs`
    PCRE_CFLAGS="$CPPFLAGS `$PCRECONFIG --cflags`"
    AC_DEFINE([HAVE_LIBPCRE], [1], [libpcre])
    AC_DEFINE([HAVE_PCRE_H], [1], [pcre.h])
  fi
fi

AC_SUBST(PCRE_LIB)
AC_SUBST(PCRE_CFLAGS)

AC_ARG_WITH(lighttpd-src,
        [  --with-lighttpd-src=[DIR]        Specify location of lighttpd src],
        [LIGHTTPD_SRC=$withval],
        [LIGHTTPD_SRC=]
)

AC_ARG_WITH(lighttpd-obj,
        [  --with-lighttpd-obj=[DIR]        Specify location of lighttpd obj],
        [LIGHTTPD_OBJ=$withval],
        [LIGHTTPD_OBJ=$LIGHTTPD_SRC]
)

AC_ARG_WITH(lighttpd-mod,
        [  --with-lighttpd-mod=[DIR]      Specify location of lighttpd mods],
        [LIGHTTPD_MOD=$withval],
        [LIGHTTPD_MOD=$prefix/lib]
)

AC_SUBST(LIGHTTPD_MOD)
AC_SUBST(LIGHTTPD_SRC)
AC_SUBST(LIGHTTPD_OBJ)

AM_CONDITIONAL([HAVE_LIGHTTPD], [test "${LIGHTTPD_SRC}" != ""])
if test "${HAVE_LIGHTTPD_TRUE}" = "" -a "${WITH_PCRE}" = "yes"; then
    AC_DEFINE([HAVE_LIGHTTPD], [1], [Enable lighttpd])
fi

#
# lighttpd does not ship a 'lighttpd-config' script, so we
# can either hurt ourselves finding the current version or
# just punt.  For now, we just pray for hangtime....
#
LIGHTTPD_REQUIRED_VERSION=1.4.28
LIGHTTPD_VERSION=${LIGHTTPD_REQUIRED_VERSION}
LIGHTTPD_VERSION_ID=`echo $LIGHTTPD_VERSION | $AWK -F '.' '{print "(" $1 " << 16 | " $2 " << 8 | " $3 ")"}'`
AC_DEFINE_UNQUOTED([LIGHTTPD_VERSION_ID], [$LIGHTTPD_VERSION_ID], [lighttpd-version-id])

AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])

case $host_os in
     darwin*)
        LIBTOOL=glibtool
	;;
     cygwin*|CYGWIN*)
	LDFLAGS=-no-undefined
	SLAX_WARNINGS=no
	;;
esac

JUISEDIR=""

AC_ARG_WITH(juise-dir,
        [  --with-juise-dir=[DIR]           Specify location of juise files],
        [JUISE_DIR=$withval],
        [JUISE_DIR=$prefix/share/juise]
)

AC_ARG_WITH(script-dir,
        [  --with-script-dir=[DIR]           Specify location of juise scripts],
        [JUISE_SCRIPT_DIR=$withval],
        [JUISE_SCRIPT_DIR=$JUISE_DIR/scripts]
)

AC_ARG_WITH(web-dir,
        [  --with-web-dir=[DIR]           Specify location of web server],
        [JUISE_WEB_DIR=$withval],
        [JUISE_WEB_DIR=$JUISE_DIR/web]
)

JUISE_SRCDIR='${srcdir}'
JUISE_LIBDIR='${libdir}'
JUISE_INCLUDEDIR='${includedir}'
AC_SUBST(JUISE_DIR)
AC_SUBST(JUISE_SCRIPT_DIR)
AC_SUBST(JUISE_WEB_DIR)
AC_SUBST(JUISE_SRCDIR)
AC_SUBST(JUISE_LIBDIR)
AC_SUBST(JUISE_INCLUDEDIR)
AC_SUBST(JUISE_CFLAGS)
AC_SUBST(JUISE_LIBTOOLLIBS)
AC_SUBST(JUISE_LIBS)

dnl for the spec file
RELDATE=`date +'%Y-%m-%d%n'`
AC_SUBST(RELDATE)

AC_MSG_RESULT(Using configure dir $ac_abs_confdir)

if test -d $ac_abs_confdir/.svn ; then
  extra=`svn info $ac_abs_confdir | grep Revision | sed 's+Revision: ++'`
  if test "$extra" != ""
  then
      LIBJUISE_VERSION_EXTRA="-SVN$extra"
  fi
fi

LIBJUISE_VERSION=$PACKAGE_VERSION
LIBJUISE_VERSION_NUMBER=VERSION_TO_NUMBER(echo $PACKAGE_VERSION)
AC_SUBST(LIBJUISE_VERSION)
AC_SUBST(LIBJUISE_VERSION_NUMBER)
AC_SUBST(LIBJUISE_VERSION_EXTRA)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  juise-config
  libjuise/juiseconfig.h
  libjuise/Makefile
  juise/Makefile
  doc/Makefile
  juise-askpass/Makefile
  mod_juise/Makefile
  extensions/Makefile
  web/Makefile
  web/lighttpd.conf
])
AC_OUTPUT


AC_MSG_NOTICE([summary of build options:

  juise version:  ${VERSION} ${LIBJUISE_VERSION_EXTRA}
  host type:        ${host}
  install prefix:   ${prefix}
  compiler:         ${CC}
  compiler flags:   ${CFLAGS}
  library types:    Shared=${enable_shared}, Static=${enable_static}

  libxml version:   ${XMLVERS}
  libxslt cflags:   ${LIBXSLT_CFLAGS}
  libxml libs:      ${LIBXML_LIBS}
  libxslt version:  ${XSLTVERS}
  libxslt cflags:   ${LIBXSLT_CFLAGS}
  libxslt libs:     ${LIBXSLT_LIBS}
  libslax version:  ${SLAXVERS}
  libslax cflags:   ${LIBSLAX_CFLAGS}
  libslax libs:     ${LIBSLAX_LIBS}
  libslax libdir:   ${SLAX_LIBDIR}
  libslax extdir:   ${SLAX_EXTDIR}
  libpcre cflags:   ${PCRE_CFLAGS}
  libpcre libs:     ${PCRE_LIB}
  lighttpd src:     ${LIGHTTPD_SRC}
  lighttpd obj:     ${LIGHTTPD_OBJ}
  lighttpd modules: ${LIGHTTPD_MOD}

  debug:            ${JUISE_DEBUG:-no}
  readline:         ${HAVE_READLINE:-no}
  printf-like:      ${HAVE_PRINTFLIKE:-no}
])
