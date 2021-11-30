#!/bin/bash

# call original shutdown script, but overwrite wakeup time if set before

[ -e /etc/sysconfig/reel ] && source /etc/sysconfig/reel

# use from /etc/sysconfig/reel or default
wakeupfile=${WAKEUP_FILE_DEFAULT:-/run/vdr/next-timer-reel}

wakeuptime=${1:-0}

if [ -e $wakeupfile -a -s $wakeupfile ]; then
	wakeuptimereel=$(awk '{ print $1 }' $wakeupfile)

	if [ -n "$wakeuptimereel" ]; then
		if [ $wakeuptimereel -gt $wakeuptime ]; then
			logger -s "Overwrite original wakeup time by adjusted one $wakeuptime -> $wakeuptimereel"
			wakeuptime=$wakeuptimereel
		fi
	fi
fi

logger -s "Call original shutdown script with wakeup time: $wakeuptime"
svdrpsend MESG "Shutdown (now)" >/dev/null
sleep 1
/usr/lib64/vdr/bin/vdr-shutdown.sh $wakeuptime
