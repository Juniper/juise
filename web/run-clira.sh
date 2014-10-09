#!/bin/bash
#
# This script starts/stops the Clira (CLI Remote Access) service
#

JUISEHOME=$HOME/.juise
BASE_CLIRA_CONF=/usr/share/doc/juise/clira.conf
CLIRA_CONF=$JUISEHOME/lighttpd-clira.conf
LIGHTTPD_PIDFILE=$JUISEHOME/lighttpd.pid
MODULES_DIR=/usr/lib

usage ()
{
	echo "run-clira.sh usage:    run-clira.sh [start|stop]"
	echo ""
	exit 1
}

start ()
{
	if [ "$LIGHTTPD_RUNNING" = "yes" ]; then
		echo "PID $LIGHTTPD_PID already running.  Use 'run-clira.sh stop' to stop it."
		exit
	fi

	# Replace /var/run/lighttpd.pid with our user pid file
	if [ ! -e $CLIRA_CONF ]; then
		cp $BASE_CLIRA_CONF $CLIRA_CONF
		sed -i "s,/var/run/lighttpd.pid,$LIGHTTPD_PIDFILE,g" $CLIRA_CONF
	fi

	if [ ! -x /usr/bin/mixer ]; then
		echo "/usr/bin/mixer not found!  Are you sure you have it installed?"
		exit
	fi

	/usr/bin/mixer --create-db > /dev/null

	if [ ! -x /usr/sbin/lighttpd-for-juise ]; then
		echo "/usr/sbin/lighttpd-for-juise not found!  Are you sure you have it installed?"
		exit
	fi

	# Fire 'er up
	/usr/sbin/lighttpd-for-juise -m $MODULES_DIR -f $CLIRA_CONF > /dev/null

	echo "Clira started!"
}

stop ()
{
	if [ "$LIGHTTPD_RUNNING" = "no" ]; then
		echo "Clira is not running.  Use 'run-clira.sh start' to start it."
		exit
	fi

	echo "Shutting down Clira and all related processes..."

	kill $LIGHTTPD_PID > /dev/null
	killall mixer -u $USER > /dev/null
	
	rm -f $LIGHTTPD_PIDFILE

	echo "...done"
}

if [ $# -ne 1 ]; then
	usage
fi

if [ ! -e $JUISEHOME ]; then
	mkdir -p $JUISEHOME
fi

LIGHTTPD_PID=xxx
LIGHTTPD_RUNNING=no
if [ -e $LIGHTTPD_PIDFILE ]; then
	LIGHTTPD_PID=`cat $LIGHTTPD_PIDFILE`

	if ps -p $LIGHTTPD_PID > /dev/null
	then
		LIGHTTPD_RUNNING=yes
	fi
fi

# Figure out our lighttpd-for-juise modules directory
if [ -d /usr/lib64/lighttpd-for-juise ]; then
	MODULES_DIR=/usr/lib64/lighttpd-for-juise
elif [ -d /usr/lib/lighttpd-for-juise ]; then
	MODULES_DIR=/usr/lib/lighttpd-for-juise
fi

if [ "$1" = "start" ]; then
	start
elif [ "$1" = "stop" ]; then
	stop
else
	usage
fi
