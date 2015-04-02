#!/bin/bash

#
# usage: mac.sh [libslax source dir] [lighttpd-for-juise source dir]
#
# This is a somewhat hacky script to build a installation of Clira for
# Macintosh.
#
# Note this *must* be run on a properly configured Mac box with appropriate
# developer tools (gcc, libtool, make, etc).  This script must also be run sudo.
#
# It will do the following:
#
# 1) Remove /Applications/Clira (which is where this will be staged for packaging)
# 2) Download MacPorts source bundle and build MacPorts to use
#    /Applications/Clira/.root/usr/local as the root directory of MacPorts
#    installs
# 3) Install required MacPorts for Clira in /Applications/Clira/.root/usr/local
# 4) Build and install libslax into /Applications/Clira/.root/usr/local
# 5) Build and install lighttpd-for-juise into
#    /Applications/Clira/.root/usr/local
# 6) Build and install juise/Clira into /Applications/Clira/.root/usr/local
# 7) Create various startup scripts into destination dir
#
# At this point /Applications/Clira is ready to be backaged into a installable
# .dmg file.  I recommend using DropDMG.
#
# Note that this will take quite a while to complete (Likely 10+ minutes on a
# fast Mac)
#

if [ "$(id -u)" != "0" ]; then
	echo "This script must be run as root (or under sudo)."
	exit
fi

if [ $# -eq 0 ]; then
	echo "usage: mac.sh [libslax source dir] [lighttpd-for-juise source dir]"
	exit
fi

if [ ! -e "$1/slax-config.in" ]; then
	echo "$1 does not exist or is not a valid libslax source directory"
	exit
fi

if [ ! -e "$2/autogen.sh" ]; then
	echo "$2 does not exist or is not a valid lighttpd-for-juise directory"
	exit
fi

CLIRA_APP_ROOT="/Applications/Clira"
CLIRA_HOME="${CLIRA_APP_ROOT}/.root/usr/local"
CWD=`pwd`
BUILDDIR="${CWD}/build"
OUTPUTDIR="${CWD}/output"
MACPORTS_VERSION=2.3.1

echo ""
echo "${CLIRA_APP_ROOT} will be recursively removed as it is used for staging the build."
echo "Hit ENTER to confirm or ^C to cancel"
echo ""
read

rm -rf ${CLIRA_APP_ROOT}
mkdir -p ${CLIRA_HOME}

rm -rf ${BUILDDIR}
mkdir -p ${BUILDDIR}/macports
cd ${BUILDDIR}/macports

if [ ! -f /tmp/MacPorts-${MACPORTS_VERSION}.tar.bz2 ]; then
	echo "Fetching MacPorts v${MACPORTS_VERSION} source..."
	wget -P /tmp https://distfiles.macports.org/MacPorts/MacPorts-2.3.1.tar.bz2
else
	echo "Using /tmp/MacPorts-${MACPORTS_VERSION}.tar.bz2..."
fi

tar -xf /tmp/MacPorts-${MACPORTS_VERSION}.tar.bz2
echo "Building MacPorts into ${CLIRA_HOME}..."
cd MacPorts-${MACPORTS_VERSION}
./configure --prefix=${CLIRA_HOME} --with-applications-dir=${CLIRA_HOME}/Applications
make install

echo "Updating MacPorts & installing necessary prerequisites into ${CLIRA_HOME}..."
cd ${CLIRA_HOME}
echo "rsync://sea.us.rsync.macports.org/release/tarballs/ports.tar [default]" > ${CLIRA_HOME}/etc/macports/sources.conf
bin/port -v selfupdate
bin/port -v install libssh2 libxml2 pcre readline curl
rm -rf var/macports/build/* var/macports/distfiles/* var/macports/packages/* var/macports/software/* var/macports/sources/*
echo "MacPorts preqrequisites installed..."

echo "Building and installing libslax..."
cd $1
autoreconf -f -i
mkdir clira-build
make distclean
cd clira-build
../configure --prefix=${CLIRA_HOME}
make install
echo "libslax installed..."

echo "Building and installing lighttpd-for-juise..."
cd $2
./autogen.sh
make distclean
./configure --prefix=${CLIRA_HOME} --with-websocket=ALL --without-libicu
make install
echo "lighttpd-for-juise installed..."

echo "Building and installing juise..."
cd ${BUILDDIR}/../../..
autoreconf -f -i
make distclean
VERSION=`grep "PACKAGE_VERSION='" ./configure | cut -d "'" -f 2`

cd ${BUILDDIR}
../../../configure --enable-mixer --enable-clira --with-lighttpd-src=$2 --prefix=${CLIRA_HOME} --with-libslax-prefix=${CLIRA_HOME} --with-ssh2=${CLIRA_HOME}
make install
cd ..

mkdir -p ${CLIRA_HOME}/var/run
chmod 777 ${CLIRA_HOME}/var/run

# Start Clira Service App
mkdir -p "${CLIRA_APP_ROOT}/Start Clira Service.app/Contents/MacOS"
mkdir -p "${CLIRA_APP_ROOT}/Start Clira Service.app/Contents/Resources"
cp clira.icns "${CLIRA_APP_ROOT}/Start Clira Service.app/Contents/Resources"
cp Info.plist "${CLIRA_APP_ROOT}/Start Clira Service.app/Contents"
cp clira-start.sh "${CLIRA_APP_ROOT}/Start Clira Service.app/Contents/MacOS/clira.sh"

# Stop Clira Service App

mkdir -p "${CLIRA_APP_ROOT}/Stop Clira Service.app/Contents/MacOS"
mkdir -p "${CLIRA_APP_ROOT}/Stop Clira Service.app/Contents/Resources"
cp clira.icns "${CLIRA_APP_ROOT}/Stop Clira Service.app/Contents/Resources"
cp Info.plist "${CLIRA_APP_ROOT}/Stop Clira Service.app/Contents"
cp clira-stop.sh "${CLIRA_APP_ROOT}/Stop Clira Service.app/Contents/MacOS/clira.sh"

rm -rf ${OUTPUTDIR}
mkdir -p ${OUTPUTDIR}
mv /Applications/Clira ${OUTPUTDIR}

echo ""
echo "Clira version $VERSION has been installed into ${OUTPUTDIR}/Clira.  You may now package it up into a .dmg."
echo ""
