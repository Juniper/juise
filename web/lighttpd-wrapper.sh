#!/bin/bash

#
# Simple start/stop wrapper script for lighttpd
#

CLIRA_HOME=${CLIRA_HOME-/usr/local}
LIGHTTPD_PID=xXxXx
if [ ! -d $CLIRA_HOME/var/run ]; then
	mkdir -p $CLIRA_HOME/var/run
fi
if [ -f "$CLIRA_HOME/var/run/lighttpd.pid" ]; then
	LIGHTTPD_PID=`cat $CLIRA_HOME/var/run/lighttpd.pid`
fi
LIGHTTPD_PIDS=`ps cax | grep -o '^[ ]*[0-9]*' | grep $LIGHTTPD_PID`
MIXER_PIDS=`ps cax | grep mixer | grep -o '^[ ]*[0-9]*'`

case $1 in
"start")
	echo "starting lighttpd..."
	if [ -z "$LIGHTTPD_PIDS" ]; then
		$CLIRA_HOME/sbin/lighttpd -f $CLIRA_HOME/etc/lighttpd.conf
		echo "lighttpd started"
	else
		echo "lighttpd is already running [pid: $LIGHTTPD_PID]"
	fi ;;
"stop") echo "stopping lighttpd..."
	if [ -z "$LIGHTTPD_PIDS" ]; then
		echo "lighttpd is not running"
	else
		kill -9 $LIGHTTPD_PID
		echo "lighttpd stopped"
	fi
	for PID in $MIXER_PIDS; do
		kill -9 $PID
	done ;;
*) echo "usage: lighttpd-wrapper.sh [start|stop]";;
esac
