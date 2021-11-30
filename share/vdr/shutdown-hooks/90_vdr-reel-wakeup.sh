#!/usr/bin/sh
#
# ReelBoxNG: adjust wakeup time
#
# 20201206/PB: initial
# 20211130/PB: use options from /etc/sysconfig/reel

[ -e /etc/sysconfig/reel ] && source /etc/sysconfig/reel

wakeuptime=${1:-0}

wakeupdaysmax=${WAKEUP_DAYS_MAX:-4} # 4 days default

wakeupdeltamax=$((60 * 60 * 24 * $WAKEUP_DAYS_MAX)) # convert days into seconds
now=$(date '+%s') # current time in seconds
wakeuptimemax=$[ $now + $wakeupdeltamax ] # maximum wakup time in seconds

# use from /etc/sysconfig/reel or default
wakeupfile=${WAKEUP_FILE_DEFAULT:-/run/vdr/next-timer-reel}

## select final wakeup time
if [ $wakeuptime -gt $wakeuptimemax ]; then
        logger -s "Reduce wakeuptime from $wakeuptime ($(date -d @$wakeuptime '+%Y%m%d-%H%M%S')) to $wakeuptimemax ($(date -d @$wakeuptimemax '+%Y%m%d-%H%M%S'))"
	wakeuptimeorig=$wakeuptime
        wakeuptime=$wakeuptimemax
	echo "$wakeuptime $wakeuptimeorig" > $wakeupfile
	vdrmessage="Reduce wakeuptime from $(date -d @$wakeuptime '+%Y-%m-%d %H:%M') to $(date -d @$wakeuptimemax '+%Y-%m-%d %H:%M')"
elif [ $wakeuptime -eq 0 ]; then
        logger -s "Originally no wakeuptime, so wakeup latest $wakeuptimemax ($(date -d @$wakeuptimemax '+%Y%m%d %H:%M'))"
        wakeuptime=$wakeuptimemax
	echo "$wakeuptime $wakeuptimeorig" > $wakeupfile
	vdrmessage="No wakeuptime, so wakeup latest $(date -d @$wakeuptimemax '+%a %d.%m.%Y %H:%M')"
else
        logger -s "Next wakeup $wakeuptime ($(date -d @$wakeuptime '+%Y%m%d %H:%M'))"
	echo "$wakeuptime" > $wakeupfile
	vdrmessage="Next wakeup $(date -d @$wakeuptime '+%a %d.%m.%Y %H:%M')"
fi

svdrpsend MESG "Shutdown (info) - $vdrmessage" >/dev/null
sleep 2
