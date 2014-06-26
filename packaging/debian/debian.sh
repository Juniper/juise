#!/bin/bash

#
# usage: debian.sh [optional path to lighttpd-for-juise source]
#
# This is a somewhat hacky script to use debian tools to build a valid .deb
# package file for juise.  Since juise contains mod_juise and clira, it will
# need to know where the lighttpd-for-juise source is.  If it is not passed
# into this script, it will attempt to clone one via git.
#
# See the README file in this directory on steps how to use this script.
#

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

# dpkg-buildpackage requires the debian directory here
ln -s packaging/debian debian
LIGHTTPD_FOR_JUISE=$LIGHTTPD_FOR_JUISE dpkg-buildpackage -us -uc -rfakeroot > debian/build.log

# remove all the files dpkg-buildpackage leaves around
rm -rf debian/files debian/tmp debian/juise debian/juise.substvars \
	debian/juise*debhelper* debian/build.log debian/lighttpd-for-juise

# clean up our symlink
rm debian
cd packaging/debian
mkdir -p output

# dpkg-buildpackage doesn't support output directory argument
mv ../../../juise_* output

echo "-----------------------------------------------------------------"
echo ".deb (and related files) have been created in 'output' directory."
echo ""
