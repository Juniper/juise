#!/bin/sh

CLIRA_ROOT=/Applications/Clira/.root

export DYLD_LIBRARY_PATH="${CLIRA_ROOT}/usr/local/lib"
export SLAX_EXTDIR="${CLIRA_ROOT}/usr/local/lib/slax/extensions"
export CLIRA_HOME="${CLIRA_ROOT}/usr/local"

cd ${CLIRA_ROOT} && sh usr/local/etc/lighttpd-wrapper.sh stop
