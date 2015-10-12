#!/bin/bash

#
# usage: rpm.sh
#
# This is a somewhat hacky script to use rpmbuild to build a valid .rpm
# package file for juise w/ CLIRA.
#
# Note this will make rpmbuild directories under ~/rpmbuild
#
# See the README.md file in this directory on steps how to use this script.
#

PKG_DIR=`pwd`

mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

if [ $# -ne 1 ]; then
	GIT=`which git`
	LIGHTTPD_FOR_JUISE=`pwd`/lighttpd-for-juise
	
	if [ ! -x $GIT ]; then
		echo "You did not spcify the path to lighttpd-for-juise and git was not found in your PATH"
		exit
	fi

	if [ ! -e lighttpd-for-juise/config.h ]; then
		echo "You did not specify the path to lighttpd-for-juise, attempting to clone via git"
		rm -rf lighttpd-for-juise

		git clone https://github.com/Juniper/lighttpd-for-juise.git
		cd lighttpd-for-juise
		./autogen.sh
		./configure
		make
		cd ..
	fi
else
	LIGHTTPD_FOR_JUISE=$1
fi

if [ ! -e $LIGHTTPD_FOR_JUISE/config.h ]; then
	echo "$LIGHTTPD_FOR_JUISE/config.h not found.  Are you sure lighttpd-for-juise is built?"
	exit
fi

cd ../..
autoreconf -f -i

cd $PKG_DIR
mkdir -p build
cd build

../../../configure --enable-clira --enable-mixer --with-lighttpd-src=$LIGHTTPD_FOR_JUISE

make
make dist
mv juise*tar.gz ~/rpmbuild/SOURCES

rpmbuild -ba packaging/rpm/juise.clira.spec

cd $PKG_DIR
rm -rf build

echo "------------------------------------------------------------"
echo ".rpm has been created in '~/rpmbuild/RPMS/<arch>' directory."
echo "-"
