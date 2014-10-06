#
# $Id$
#
# Copyright 2011-2012, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

AC_PREREQ(2.2)
AC_INIT([juise], [0.6.1], [phil@juniper.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])

case $prefix in
     NONE)
	prefix=/usr/local
	;;
esac

case $exec_prefix in
     NONE)
	exec_prefix=$prefix
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
LIBSLAX_REQUIRED_VERSION=0.18.0

AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_CONFIG_MACRO_DIR([m4])

# Must be after AM_PROG_AR
LT_INIT([dlopen])

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
AC_DEFINE_UNQUOTED(PATH_SSH, ["$PATH_SSH"], [Path to SSH binary])
AC_DEFINE_UNQUOTED(PATH_GZIP, ["$PATH_GZIP"], [Path to GZIP binary])

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
AC_CHECK_FUNCS([strndup])

AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([ctype.h errno.h stdio.h stdlib.h])
AC_CHECK_HEADERS([string.h sys/param.h unistd.h])
AC_CHECK_HEADERS([sys/sysctl.h])
AC_CHECK_HEADERS([stdint.h sys/statfs.h])


AC_CHECK_LIB([crypto], [MD5_Init])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "$HAVE_LIBCRYPTO" != "no"])

AC_CHECK_LIB([m], [lrint])
AM_CONDITIONAL([HAVE_LIBM], [test "$HAVE_LIBM" != "no"])

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

AC_MSG_CHECKING([whether to build with warnings])
AC_ARG_ENABLE([warnings],
    [  --enable-warnings     Turn on compiler warnings],
    [JUISE_WARNINGS=$enableval],
    [JUISE_WARNINGS=no])
AC_MSG_RESULT([$JUISE_WARNINGS])
AM_CONDITIONAL([JUISE_WARNINGS_HIGH], [test "$JUISE_WARNINGS" != "no"])

AC_MSG_CHECKING([whether to build CLIRA web interface])
AC_ARG_ENABLE([clira],
    [  --enable-clira        Build the CLIRA web interface],
    [NEED_CLIRA=$enableval],
    [NEED_CLIRA=no])
AC_MSG_RESULT([$NEED_CLIRA])
AM_CONDITIONAL([NEED_CLIRA], [test "$NEED_CLIRA" != "no"])
AC_SUBST(NEED_CLIRA)

AC_MSG_CHECKING([whether to build mixer])
AC_ARG_ENABLE([mixer],
    [  --enable-mixer        Build the mixer binary],
    [NEED_MIXER=$enableval],
    [NEED_MIXER=$NEED_CLIRA])
AC_MSG_RESULT([$NEED_MIXER])
AM_CONDITIONAL([NEED_MIXER], [test "$NEED_MIXER" != "no"])
AC_SUBST(NEED_MIXER)

AC_MSG_CHECKING([whether to build with debugging])
AC_ARG_ENABLE([debug],
    [  --enable-debug        Turn on debugging],
    [JUISE_DEBUG=$enable_debug],
    [JUISE_DEBUG=no])
AC_MSG_RESULT([$JUISE_DEBUG])
AM_CONDITIONAL([JUISE_DEBUG], [test "$JUISE_DEBUG" = "yes"])
if test "JUISE_DEBUG" = "yes" ; then
    AC_DEFINE([JUISE_DEBUG], [1], [Enable debugging])
fi
AC_SUBST(JUISE_DEBUG)

PATH_JUISE=`eval echo $bindir/juise`
AC_DEFINE_UNQUOTED(PATH_JUISE, ["$PATH_JUISE"], [Path to juise binary])

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

AC_MSG_CHECKING([whether to build with libedit])
AC_ARG_ENABLE([libedit],
    [  --enable-libedit    Enable support for libedit (BSD readline)],
    [HAVE_LIBEDIT=yes; AC_DEFINE([HAVE_LIBEDIT], [1], [Support libedit])],
    [HAVE_LIBEDIT=no])
AC_MSG_RESULT([$HAVE_LIBEDIT])
AM_CONDITIONAL([HAVE_LIBEDIT], [test "$HAVE_LIBEDIT" != "no"])

AC_MSG_CHECKING([compiler for gcc])
HAVE_GCC=no
if test "${CC}" != ""; then
    HAVE_GCC=`${CC} --version 2>&1 | grep -i ' GCC'`
    if test "${HAVE_GCC}" != ""; then
        HAVE_GCC=yes
    else
        HAVE_GCC=no
    fi
fi
AC_MSG_RESULT([$HAVE_GCC])
AM_CONDITIONAL([HAVE_GCC], [test "$HAVE_GCC" = "yes"])

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
	LIBXML_VERSION=`$XML_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $LIBXML_VERSION) -ge VERSION_TO_NUMBER(echo $LIBXML_REQUIRED_VERSION)
	then
		LIBXML_LIBS="$LIBXML_LIBS `$XML_CONFIG --libs`"
		LIBXML_CFLAGS="$LIBXML_CFLAGS `$XML_CONFIG --cflags`"
		AC_MSG_RESULT($LIBXML_VERSION found)
	else
		AC_MSG_ERROR(Version $LIBXML_VERSION found. You need at least libxml2 $LIBXML_REQUIRED_VERSION for this version of libxslt)
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
	LIBXSLT_VERSION=`$XSLT_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $LIBXSLT_VERSION) -ge VERSION_TO_NUMBER(echo $LIBXSLT_REQUIRED_VERSION)
	then
		LIBXSLT_LIBS="$LIBXSLT_LIBS `$XSLT_CONFIG --libs`"
		LIBXSLT_CFLAGS="$LIBXSLT_CFLAGS `$XSLT_CONFIG --cflags`"
		AC_MSG_RESULT($LIBXSLT_VERSION found)
	else
		AC_MSG_ERROR(Version $LIBXSLT_VERSION found. You need at least libxslt $LIBXSLT_REQUIRED_VERSION for this version of libxslt)
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
	LIBSLAX_VERSION=`$SLAX_CONFIG --version`
	if test VERSION_TO_NUMBER(echo $LIBSLAX_VERSION) -ge VERSION_TO_NUMBER(echo $LIBSLAX_REQUIRED_VERSION)
	then
		LIBSLAX_LIBS="$LIBSLAX_LIBS `$SLAX_CONFIG --libs`"
		LIBSLAX_CFLAGS="$LIBSLAX_CFLAGS `$SLAX_CONFIG --cflags`"
		SLAX_EXTDIR="`$SLAX_CONFIG --extdir | head -1`"
		SLAX_LIBDIR="`$SLAX_CONFIG --libdir | head -1`"
		SLAX_BINDIR="`$SLAX_CONFIG --bindir | head -1`"
		SLAX_OXTRADOCDIR="`$SLAX_CONFIG --oxtradoc | head -1`"
		AC_MSG_RESULT($LIBSLAX_VERSION found)
	else
		AC_MSG_ERROR(Version $LIBSLAX_VERSION found. You need at least libslax $LIBSLAX_REQUIRED_VERSION for this version of juise)
	fi
else
	AC_MSG_ERROR([Could not find libslax anywhere, check https://github.com/Juniper/libslax.])
fi

AC_SUBST(SLAX_CONFIG)
AC_SUBST(LIBSLAX_CFLAGS)
AC_SUBST(LIBSLAX_LIBS)
AC_SUBST(SLAX_BINDIR)
AC_SUBST(SLAX_EXTDIR)
AC_SUBST(SLAX_LIBDIR)
AC_SUBST(SLAX_OXTRADOCDIR)

#
# ---- handle libssh2 (which does not have a libssh2-config)
#

AC_ARG_WITH(ssh2,
  AC_HELP_STRING([--with-ssh2=PATH],[Use libssh2 installed under PATH]),
  [
    case $with_ssh2 in
      *)
        LDFLAGS="$LDFLAGS -L$with_ssh2/lib"
        CPPFLAGS="$CPPFLAGS -I$with_ssh2/include"
        ;;
    esac
  ])

HAVE_LIBSSH2=no
AC_CHECK_LIB(ssh2, libssh2_channel_open_ex, HAVE_LIBSSH2=yes, HAVE_LIBSSH2=no)
if test "$HAVE_LIBSSH2" == "no" -a "$NEED_MIXER" == "yes"
then
        AC_MSG_ERROR(--enable-mixer specified but libssh2 libraries not installed)
fi

#
# ---- handle sqlite3
#

AC_ARG_WITH(sqlite3,
  AC_HELP_STRING([--with-sqlite3=PATH],[Use sqlite3 installed under PATH]),
  [
    case $with_sqlite3 in
      *)
        LDFLAGS="$LDFLAGS -L$with_sqlite3/lib"
        CPPFLAGS="$CPPFLAGS -I$with_sqlite3/include"
        ;;
    esac
  ])

HAVE_SQLITE3=no
AC_CHECK_LIB(sqlite3, sqlite3_open, HAVE_SQLITE3=yes, HAVE_SQLITE3=no)

if test "$HAVE_SQLITE3" == "no" -a "$NEED_MIXER" == "yes"
then
        AC_MSG_ERROR(--enable-mixer specified but libsqlite3 libraries not installed)
fi

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

AC_ARG_WITH(lighttpd-lib,
        [  --with-lighttpd-lib=[DIR]      Specify location of lighttpd libraries],
        [LIGHTTPD_LIBDIR=$withval],
        [LIGHTTPD_LIBDIR=$prefix/lib]
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

AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])

case $host_os in
     darwin*)
        LIBTOOL=glibtool
	SLAX_LIBEXT=dylib
	;;
     Linux*|linux*)
        CFLAGS="-D_GNU_SOURCE $CFLAGS"
	LDFLAGS=-ldl
	SLAX_LIBEXT=so
	;;
     cygwin*|CYGWIN*)
	LDFLAGS=-no-undefined
	SLAX_LIBEXT=ddl
	;;
esac

JUISEDIR=""

AC_ARG_WITH(juise-dir,
        [  --with-juise-dir=[DIR]           Specify location of juise files],
        [JUISE_DIR=$withval],
        [JUISE_DIR=$prefix/share/juise]
)
AC_SUBST(JUISE_DIR)

AC_ARG_WITH(script-dir,
        [  --with-script-dir=[DIR]           Specify location of juise scripts],
        [JUISE_SCRIPT_DIR=$withval],
        [JUISE_SCRIPT_DIR=$JUISE_DIR/scripts]
)
AC_SUBST(JUISE_SCRIPT_DIR)

AC_ARG_WITH(web-dir,
        [  --with-web-dir=[DIR]           Specify location of web server],
        [JUISE_WEB_DIR=$withval],
        [JUISE_WEB_DIR=$JUISE_DIR/web]
)
AC_SUBST(JUISE_WEB_DIR)

AC_ARG_WITH(cgi-dir,
        [  --with-cgi-dir=[DIR]           Specify location of cgi files],
        [JUISE_CGI_DIR=$withval],
        [JUISE_CGI_DIR=$JUISE_DIR/web/local]
)
AC_SUBST(JUISE_CGI_DIR)

AC_ARG_WITH(php-cgi,
        [  --with-php-cgi=[DIR]           Specify location of php-cgi binary],
        [PHP_CGI=$withval],
        [PHP_CGI=$prefix/bin/php-cgi]
)
AC_SUBST(PHP_CGI)

JUISE_LIBS=-ljuise
AC_SUBST(JUISE_LIBS)
JUISE_SRCDIR='${srcdir}'
AC_SUBST(JUISE_SRCDIR)
JUISE_LIBDIR='${libdir}'
AC_SUBST(JUISE_LIBDIR)
JUISE_INCLUDEDIR='${includedir}'
AC_SUBST(JUISE_LIBEXECDIR)
JUISE_LIBEXECDIR='${pkglibexecdir}'
AC_SUBST(JUISE_INCLUDEDIR)

AC_SUBST(SLAX_LIBEXT)

dnl for the spec file
RELDATE=`date +'%Y-%m-%d%n'`
AC_SUBST(RELDATE)
UNAME=`uname -a`

AC_MSG_RESULT(Using configure dir $ac_abs_confdir)

if test -d $ac_abs_confdir/.git ; then
  extra=`git branch | awk '/\*/ { print $2 }'`
  if test "$extra" != "" -a "$extra" != "master"
  then
      LIBJUISE_VERSION_EXTRA="-git-$extra"
  fi
fi

LIBJUISE_VERSION=$PACKAGE_VERSION
LIBJUISE_VERSION_NUMBER=VERSION_TO_NUMBER(echo $PACKAGE_VERSION)
AC_SUBST(LIBJUISE_VERSION)
AC_SUBST(LIBJUISE_VERSION_NUMBER)
AC_SUBST(LIBJUISE_VERSION_EXTRA)

AC_DEFINE_UNQUOTED(LIBJUISE_VERSION, ["$LIBJUISE_VERSION"],
    [Version number as dotted value])
AC_DEFINE_UNQUOTED(LIBJUISE_VERSION_NUMBER, [$LIBJUISE_VERSION_NUMBER],
    [Version number as a number])
AC_DEFINE_UNQUOTED(LIBJUISE_VERSION_STRING, ["$LIBJUISE_VERSION_NUMBER"],
    [Version number as string])
AC_DEFINE_UNQUOTED(LIBJUISE_VERSION_EXTRA, ["$LIBJUISE_VERSION_EXTRA"],
    [Version number extra information])
AC_DEFINE_UNQUOTED(JUISE_DIR, ["$JUISE_DIR"],
    [Directory for JUISE shared files])

AC_CONFIG_HEADERS([libjuise/juiseconfig.h])
AC_CONFIG_FILES([
  Makefile
  juise-config
  libjuise/Makefile
  juise/Makefile
  doc/Makefile
  juise-askpass/Makefile
  mod_juise/Makefile
  extensions/Makefile
  mixer/Makefile
  web/Makefile
  web/lighttpd.conf
  import/Makefile
  tests/Makefile
  packaging/juise.pc
  packaging/juise.spec
  packaging/juise.space.spec
  packaging/juise.rb.base
  packaging/freebsd/port-Makefile
])
AC_OUTPUT


AC_MSG_NOTICE([summary of build options:

  build system:     ${UNAME}

  juise version:    ${VERSION} ${LIBJUISE_VERSION_EXTRA}
  host type:        ${host}
  install prefix:   ${prefix}

  compiler:         ${CC} (${HAVE_GCC:-no})
  compiler flags:   ${CFLAGS}
  library types:    Shared=${enable_shared}, Static=${enable_static}

  libxml version:   ${LIBXML_VERSION}
  libxml cflags:    ${LIBXML_CFLAGS}
  libxml libs:      ${LIBXML_LIBS}

  libxslt version:  ${LIBXSLT_VERSION}
  libxslt cflags:   ${LIBXSLT_CFLAGS}
  libxslt libs:     ${LIBXSLT_LIBS}

  libslax version:  ${LIBSLAX_VERSION}
  libslax cflags:   ${LIBSLAX_CFLAGS}
  libslax libs:     ${LIBSLAX_LIBS}
  libslax libdir:   ${SLAX_LIBDIR}
  libslax extdir:   ${SLAX_EXTDIR}

  libpcre cflags:   ${PCRE_CFLAGS}
  libpcre libs:     ${PCRE_LIB}

  lighttpd src:     ${LIGHTTPD_SRC}
  lighttpd obj:     ${LIGHTTPD_OBJ}
  lighttpd libdir:  ${LIGHTTPD_LIBDIR}
  lighttpd modules: ${LIGHTTPD_MOD}
  php cgi binary:   ${PHP_CGI}

  warnings:         ${JUISE_WARNINGS:-no}
  debug:            ${JUISE_DEBUG:-no}
  readline:         ${HAVE_READLINE:-no}
  libedit:          ${HAVE_LIBEDIT:-no}
  printf-like:      ${HAVE_PRINTFLIKE:-no}
  build clira:      ${NEED_CLIRA:-no}
  build mixer:      ${NEED_MIXER:-no}
])
