#!/bin/bash

#
# usage: cygwin.sh [lighttpd-for-juise dir]
#
# This is a somewhat hacky script to generate the necessary cygwin
# tarball to distribute juise.
#
# It basically will run autoreconf, configure (with appropriate arguments),
# build juise and tar it up.  It will be up to you to rename it and upload
# it to the proper location.
#
# It will temporarily create a 'build' directory under this directory
#

if [ $# -eq 0 ]; then
	echo "usage: cygwin.sh [lighttpd-for-juise dir]"
	exit
fi

if [ ! -e "$1/autogen.sh" ]; then
	echo "$1 does not exist or is not a valid lighttpd-for-juise directory"
	exit
fi

CWD=`pwd`
BUILDDIR="$CWD/build"
STAGEDIR="$BUILDDIR/stage"

mkdir -p ${STAGEDIR}
cd ../..
autoreconf -f -i

VERSION=`grep "PACKAGE_VERSION='" ./configure | cut -d "'" -f 2`

cd $BUILDDIR
../../../configure --enable-mixer --enable-clira --with-lighttpd-src=$1
make install DESTDIR=$STAGEDIR
cd $STAGEDIR && tar -cjf ../../juise-$VERSION.tar.bz2 *
cd $STAGEDIR
echo "juise-$VERSION.tar.bz2 successfully created."
