#!/bin/bash
#
# ReelBox control script
#
# (P) & (C) 2020-2021 by Peter Bieringer (pb@bieringer.de) - pbev
# License: GPLv2
#
# supporting
#  - ReelBox II with AVR
#
# 20200304/pbev: merge required frontpanel add-ons from reelvdrd
# 20200324/pbev: merge required parts from fpleds
# 20200328/pbev: review
# 20200607/pbev: bugfixes regarding setup_frontpanel
# 20200715/pbev: cosmetics
# 20200726/pbev: suppress stderr of ping6 (Warning: source address might be selected on device other than:...)
# 20210128/pbev: optional read of confing /etc/sysconfig/reel
# 20210131/pbev: honor global BCONTROL and add option for brightness to set_led_button, use BCOLOR for "on"
# 20210201/pbev: keep (defined) button LED brightness on status LED change
# 20210212/pbev: add support for eHD (kernel/boot/network)
# 20210214/pbev: add support for eHD boot "reboot"
# 20210214/pbev: add support for eHD command

[ -e /etc/default/reel-globals ] && . /etc/default/reel-globals
[ -e /etc/sysconfig/reel ] && . /etc/sysconfig/reel
progname="$(basename "$0")"

# binaries
REELFPCTL=${REELFPCTL:-/opt/reel/sbin/reelfpctl}

# Frontpanel LED definitions
STANDBY_LED=1
REMOTE_LED=2
RECORD_LED=4
POWER_LED=8
ALL_LED=15

# Frontpanel LCD definitions
NOCLOCK=0
CLOCK=1
BLACKDISP=${BLACKDISP:-no}

# ReelBox eHD
HD_NETD_IP_EHD="192.168.99.129" # hardcoded in eHD boot image


# set brightness depending on display
if [ "$BLACKDISP" = "yes" ] ; then
	DBRIGHT=${DBRIGHT:-8}
else
	DBRIGHT=${DBRIGHT:-0}
fi


## Syslog function
Syslog() {
	local level="$1"
	shift
	local message="$*"

	if [ -t 0 ]; then
		printf "%-6s: %s\n" "$level" "$progname: $message" >&2
	fi
	logger -t "$progname" -p "$level" "$message"
}


## start_pre 'reelbox-ctrld'
start_pre() {
        #. /etc/default/reel-globals
	# test for plausible date (>01/01/2020), if not, get date from frontpanel
	Syslog "INFO" "test for plausible date of local clock"
	testdate=$(date +%s -d "01/01/2020")
	now=$(date +%s)

	if [ $now -lt $testdate ] ; then
	    # wrong date, retrieve time from frontpanel
	    Syslog "INFO" "wrong date of local clock, retrieve time from frontpanel"
	    i=0
	    while [ -z "$clock" ] ; do
		i=$((i+1))
		sleep 1
		clock=$($REELFPCTL -getclock)
	    done
	    fptime=$((0x`echo $clock | cut -f4- -d" " | tr -d " "`))
	    Syslog "INFO" "clock: $clock fptime: $fptime, had to ask $i times"
	    if [ $(($fptime)) -gt 1000 ]; then
		Syslog "INFO" "setting date to $fptime"
		date -s @$(($fptime)) &
	    fi
	else
		# we trust the system time
		$REELFPCTL -setclock
		sleep 0.1
	fi

        # prepare uinput driver, load kernel module if neccessary
        if ! grep -q uinput /proc/modules ; then
                modprobe uinput
        fi

        # wait for /dev/uinput to come ready, otherwise vdr will ask to learn the keys...
	Syslog "INFO" "wait for /dev/uinput to come ready"
        j=0
        while [ ! -c /dev/uinput ] ; do
                j=$((j+1))
                [ $j -gt 15 ] && break
                sleep 1
        done
        Syslog "INFO" "waited $j secs for /dev/uinput"

        # wait for /var to be mounted
        j=0
        while [ ! -d /var/run ] ; do
                j=$((j+1))
                [ $j -gt 15 ] && break
                sleep 1
        done
        Syslog "INFO" "waited $j secs for /var"

        # Get the wakeup reason from the frontpanel
        FP=`$REELFPCTL -getwakeup`
        WAKEUP=`echo $FP | cut -f3 -d" "`
        echo "$WAKEUP" > /var/run/.reelbox.wakeup
        Syslog "INFO" "wakeup reason was: $WAKEUP"
}

## daemon args for 'reelbox-ctrld'
print_daemon_args() {
    DAEMON_ARGS="-f -d /dev/frontpanel"

    [ -e /etc/default/reel-sysconfig ] && /etc/default/reel-sysconfig

        if  [ "$USE_SERIAL_REMOTE" = "yes" ]; then
                case $SERIAL_SPEED in
                        57.600)
                                PORT_SPEED=":57600"
                                ;;
                        38.400)
                                PORT_SPEED=":38400"
                                ;;
                        19.200)
                                PORT_SPEED=":19200"
                                ;;
                        9.600)
                                PORT_SPEED=":9600"
                                ;;
                        115.200|default|*)
                                PORT_SPEED=""
                                ;;
                esac
                DAEMON_ARGS="$DAEMON_ARGS -X rs232:/dev/ttyS0$PORT_SPEED"
                #echo "DEBUG: $DAEMON_ARGS"
        fi

    if [ "$USE_UDP_REMOTE_PORT" = "" ] ; then
        USE_UDP_REMOTE_PORT="2003"
    fi

    # UDP control always on
    DAEMON_ARGS="$DAEMON_ARGS -X udp:$USE_UDP_REMOTE_PORT"

    echo "REEL_BOXCTRLD_ARGS=\"$DAEMON_ARGS\""

    #exec /opt/reel/sbin/reelbox-ctrld $DAEMON_ARGS
}

## stop_post 'reelbox-ctrld'
stop_post() {
        . /etc/default/sysconfig
        case "$RUNLEVEL" in
                6)
                        #/sbin/reelfpctl -blinkled 4
                        ;;
                0)
                        $REELFPCTL -shutdown 20
			sleep 0.1
                        #/sbin/reelfpctl -blinkled 4
                        ;;
        esac


	## TODO: verify requirement
        if grep -q ICE /dev/.frontpanel.caps ; then
		if [ -e /sys/class/net/eth1 ] ; then
		    #configure WOL on eth1
		    if=eth1
		fi

            if [ "$WOL_ENABLE" = "yes" ] ; then
                /sbin/ice_wol $if
            fi
            /sbin/ice_pm
            #tell AVG power pcb to turn off mainboard power detection on AVGIII
            if grep -q AVR /dev/.frontpanel.caps ; then
                $REELFPCTL -pwrctl 0x16000000
		sleep 0.1
            fi
        elif grep -q AVR /dev/.frontpanel.caps ; then
            if [ "$WOL_ENABLE" = "yes" ] ; then
                $REELFPCTL -pwrctl 13000000
		sleep 0.1
            else
                $REELFPCTL -pwrctl 13000001
		sleep 0.1
            fi
        fi
}

start_post() {
        #. /opt/reel/etc/default/reel-globals

	for i in $(seq 1 10); do
		# search for input device
		input=$(grep -l "Reelbox Frontpanel Part I" /sys/class/input/*/name | awk -F/ '{ print $5 }')

		if [ -z "$input" ]; then
			sleep 1
		else
			break
		fi
	done

	if [ -z "$input" ]; then
		Syslog "INFO" "input device not found with name: Reelbox Frontpanel Part I"
		return 1
	fi

	event=$(ls -1 /sys/class/input/$input |grep ^event)
	if [ -z "$event" ]; then
		Syslog "INFO" "event device not found for name: Reelbox Frontpanel Part I ($input)"
		return 1
	fi

	ln -sf $event /dev/input/rbfp0
	if [ $? -ne 0 ]; then
		Syslog "INFO" "error creating softlink rbfp0 with event device $event (input=$input)"
	else
		Syslog "INFO" "softlink rbfp0 with event device $event (input=$input) successfully created"
	fi
}

## SetWakeup
SetWakeup () {
	# Tell FP to wakeup at x
	if [ -z "$1" -a -e "/tmp/vdr.wakeup" ]; then
		wakeuptime=`cat /tmp/vdr.wakeup`
	elif [ -z "$1" -a -e "/run/vdr/next-timer" ]; then
		wakeuptime=`cat /run/vdr/next-timer`
	elif [ -z "$1" ]; then
		# no wakeup time given
		wakeuptime=2147483647
		Syslog "NOTICE" "no wakeuptime given, disable wakeup (by setting to infinity)"
	else
		wakeuptime=$1
	fi

	# make sure wakeuptime for fp is at least 120s in the future
	now=$(date +%s)
	if [ $wakeuptime -lt $(($now+120)) ] ; then
		wakeuptime=$(($now+120))
		Syslog "NOTICE" "wakeuptime is < 120s in the future, increase to: $wakeuptime ($(date -d @$wakeuptime '+%Y%m%d-%H:%M:%S'))"
	fi

	if [ $wakeuptime -eq 2147483647 ]; then
		$REELFPCTL -toptext "NO-WAKEUP" -displaymode $CLOCK
		sleep 0.1
	else
		if [ -f /tmp/vdr.wakeupmax ]; then
			$REELFPCTL -toptext "REGUL-WU:$(date -d @$wakeuptime '+%d.%m.·%H:%M')" -displaymode $CLOCK
			sleep 0.1
		else 
			$REELFPCTL -toptext "TIMER-WU:$(date -d @$wakeuptime '+%d.%m.·%H:%M')" -displaymode $CLOCK
			sleep 0.1
		fi
	fi

	Syslog "INFO" "setting wakeuptime to $wakeuptime ($(date -d @$wakeuptime '+%Y%m%d-%H:%M:%S'))"
	$REELFPCTL -wakeup $wakeuptime
	sleep 0.1
}

## set ReelBox frontpanel
SetFrontpanel() {
	[ ! -e /dev/frontpanel ] && return

	local action="$1"
	shift

	case $action in
	    stopping)
		# nothing todo
		true
		;;
	    start)
		Syslog "INFO" "disable display of clock, set brightness to full"
		$REELFPCTL -displaymode $NOCLOCK -brightness 99
		sleep 0.1
		;;
	    stop)
		Syslog "INFO" "clear LCD, display clock, set brightness depending on display type: $DBRIGHT, enable wakeup by remote control"
		# TODO: call stop function
		# 13000001: with WoL (orig reelvdrd)
		# 13000000: without WoL (orig reelvdrd)
		$REELFPCTL -displaymode $CLOCK -clearlcd -brightness $DBRIGHT -pwrctl 13000000
		sleep 0.1
		;;
	    text)
		if [ "$1" = "-clock" ]; then
			local option="-displaymode $CLOCK"
			shift
		fi
		if [ -n "$*" ]; then
			Syslog "INFO" "display 'toptext': $* (option: $option)"
			$REELFPCTL -toptext "$*" $option
			sleep 0.1
		else
			Syslog "INFO" "no text given to display 'text'"
		fi
		;;
	esac
}

## set ReelBox Status LEDs
SetLEDStatus() {
	[ ! -e /dev/frontpanel ] && return

	local led="$1"
	local action="$2"
	local brightness="${3:-8}"
	local led_arg=""
	local action_arg=""

	case $led in
	    standby)
		led_arg=$STANDBY_LED
		;;
	    remote)
		led_arg=$REMOTE_LED
		;;
	    record)
		led_arg=$RECORD_LED
		;;
	    power)
		led_arg=$POWER_LED
		;;
	    ALL)
		led_arg=$ALL_LED
		;;
	    *)
		Syslog "ERROR" "unsupported LED value: $led"
		return 1
		;;
	esac

	# include button color if set
	if [ -n "$BCOLOR" -a -n "$BCONTROL" ]; then
		case $BCOLOR in
		    red)
			BBLUE=0
			BRED=$BCONTROL
			BGREEN=0
			;;
		    blue)
			BRED=0
			BBLUE=$BCONTROL
			BGREEN=0
			;;
		    pink)
			BRED=$BCONTROL
			BBLUE=$BCONTROL
			BGREEN=0
			;;
		    *)
			BRED=0
			BBLUE=0
			BGREEN=0
			;;
		esac
	fi

	VAL=$[ $brightness+256*($BRED+256*($BBLUE+256*$BGREEN)) ]

	case $action in
	    blink)
		action_arg="-setledbrightness $VAL -blinkled"
		;;
	    on)
		action_arg="-setledbrightness $VAL -setled"
		;;
	    off)
		action_arg="-clearled"
		;;
	    *)
		Syslog "ERROR" "unsupported LED action: $action"
		return 1
		;;
	esac

	Syslog "INFO" "LED action: $action $led ($action_arg $led_arg)"
	$REELFPCTL $action_arg $led_arg
	sleep 0.1
}

## set ReelBox Button LED color
SetLEDButton() {
	[ ! -e /dev/frontpanel ] && return

	local color="$1"
	local brightness="$2"

	BCONTROL=${BCONTROL:-15}	# default
	if [ "$brightness" = "max" ]; then
	       BCONTROL="15"
	elif [ -n "$brightness" ]; then
	       BCONTROL="$brightness"
	fi
	BSTATUS=${BSTATUS:-8}	# default
	BGREEN=0 # not supported

	BCOLOR="${BCOLOR:-blue}" # default
	[ "$color" = "on" ] && color="$BCOLOR"

	case $color in
	    red)
		BBLUE=0
		BRED=$BCONTROL
		BGREEN=0
		;;
	    blue)
		BRED=0
		BBLUE=$BCONTROL
		BGREEN=0
		;;
	    pink)
		BRED=$BCONTROL
		BBLUE=$BCONTROL
		BGREEN=0
		;;
	    off)
		BRED=0
		BBLUE=0
		BGREEN=0
		;;
	    *)
		Syslog "ERROR" "unsupported LED button color: $color"
		return 1
		;;
	esac

	VAL=$[ $BSTATUS+256*($BRED+256*($BBLUE+256*$BGREEN)) ]
	Syslog "INFO" "set LED button color to status=$BSTATUS red=$BRED blue=$BBLUE ($VAL)"
	$REELFPCTL -setledbrightness $VAL
	sleep 0.1
}

## show image on ReelBox frontpanel
ImageFrontpanel() {
	[ ! -e /dev/frontpanel ] && return

	local image="$1"

	if [ -z "$image" ]; then
		Syslog "ERROR" "no image given to show on frontpanel"
		return 1
	fi

	if [ ! -e "$image" -a ! -r "$image" ]; then
		Syslog "ERROR" "image not existing/readable to show on frontpanel: $image"
		return 1
	fi

	Syslog "INFO" "show image on frontpanel: $image"
	$REELFPCTL -showpnm "$image"
	sleep 0.1
}

## setup ReelBox frontpanel
# TODO: add support for USB
# arg1: <serial device>
SetupFrontpanel() {
	local device="$1"
	[ -z "$device" ] && return
	[ -e "$device" ] || return
	[ -c "$device" ] || return

	if [ -e /dev/frontpanel ]; then
		if [ -L /dev/frontpanel -a "$(readlink /dev/frontpanel)" = "$device"  ]; then
			Syslog "INFO" "softlink already exists and is proper: /dev/frontpanel -> $device"
		else
			Syslog "NOTICE" "remove existing device: /dev/frontpanel"
		       	rm -f /dev/frontpanel
		fi
	fi

	if [ ! -e /dev/frontpanel ]; then
		Syslog "INFO" "create softlink: /dev/frontpanel -> $device"
		ln -s $device /dev/frontpanel
	fi

	# check for running reelbox-ctrld which blocks the serial port
	reelbox_ctrld_pid=$(pidof reelbox-ctrld)
	if [ -n "$reelbox_ctrld_pid" ]; then
		Syslog "NOTICE" "found running reelbox-ctrld with pid: $reelbox_ctrld_pid (need to be killed)"
		kill $reelbox_ctrld_pid
		for i in $(seq 1 10); do
			if [ -z "$(pidof reelbox-ctrld)" ]; then
				Syslog "NOTICE" "reelbox-ctrld successfully killed with SIGTERM"
				break
			fi
			sleep 1
		done
		if [ -n "$(pidof reelbox-ctrld)" ]; then
			kill -9 $reelbox_ctrld_pid
			Syslog "WARN" "reelbox-ctrld requires to terminate with SIGKILL"
			sleep 1
		fi
	fi

	Syslog "INFO" "retrieve and store capabilities: /dev/.frontpanel.caps"
	/opt/reel/sbin/reelfpctl -capability >/dev/.frontpanel.caps

	local caps=$(cat /dev/.frontpanel.caps)
	Syslog "INFO" "retrieved capabilities of /dev/frontpanel: $caps"

	if [[ $device =~ tty ]]; then
		if [[ $caps =~ AVR ]]; then
			Syslog "INFO" "capabilities containing AVR for device: $device"
		else
			Syslog "ERROR" "capabilities not containing AVR while device is: $device"
			return 1
		fi
	fi

	Syslog "INFO" "trigger 'start' for LEDs and frontpanel"
	SetLEDStatus ALL on
}

## setup ReelBox netceiver
# arg1: <network interface>
SetupNetceiver() {
	local device="$1"
	[ -z "$device" ] && return

	if ! ip -o link show dev $device >/dev/null; then
		Syslog "ERROR" "configured interface not found: device=$device"
		return 1
	fi

	Syslog "INFO" "look for ReelBox Netceiver on network interface: $device"

	# bring device up
	Syslog "INFO" "bring ethernet device connected to NetCv up: $device"
	/sbin/ip link set dev $device up
	if [ $? -ne 0 ]; then
		Syslog "ERROR" "problem bringing ethernet device connected to NetCv up: $device"
		return 1
	fi
	Syslog "INFO" "bring ethernet device connected to NetCv up done: $device"


	i=5; while [ $i -gt 0 ]; do if ! /sbin/ip link show dev $device | grep -q "NO-CARRIER"; then break; else sleep 1; fi; i=$[ $i - 1 ]; done
	if [ $i -eq 0 ]; then
		logger "NOTICE" "device still having NO-CARRIER, trigger down/up sequence: $device"
		/sbin/ip link set dev $device down
		sleep 1
		/sbin/ip link set dev $device up
		logger "NOTICE" "device check again for disapearing NO-CARRIER: $device"
		i=30; while [ $i -gt 0 ]; do if ! /sbin/ip link show dev $device | grep -q "NO-CARRIER"; then break; else sleep 1; fi; i=$[ $i - 1 ]; done
		if [ $i -eq 0 ]; then
			Syslog "ERROR" "device not disapearing NO-CARRIER: $device"
			return 1
		fi
	fi

	Syslog "INFO" "check for device for disappearing NO-CARRIER done: $device"

	# wait for LOWER_UP
	Syslog "INFO" "check for ethernet device LOWER_UP: $device"
	i=30; while [ $i -gt 0 ]; do /sbin/ip link show dev $device | grep -q "LOWER_UP" && break; sleep 1; i=$[ $i - 1 ]; if [ $i -eq 0 ]; then exit 1; fi; done
	if [ $? -ne 0 ]; then
		Syslog "ERROR" "ethernet device LOWER_UP not successful: $device"
		return 1
	fi
	Syslog "INFO" "check for ethernet device LOWER_UP done: $device"

	# wait for UP
	Syslog "INFO" "check for ethernet device UP: $device"
	i=30; while [ $i -gt 0 ]; do /sbin/ip link show dev $device | grep -q ",UP," && break; sleep 1; i=$[ $i - 1 ]; if [ $i -eq 0 ]; then exit 1; fi; done
	if [ $? -ne 0 ]; then
		Syslog "ERROR" "ethernet device UP not successful: $device"
		return 1
	fi
	Syslog "INFO" "check for ethernet device UP done: $device"

	# check for IPv6 addr on interface no longer in tentative mode
	Syslog "INFO" "check for IPv6 link-local: $device"
	i=30; while [ $i -gt 0 ]; do /sbin/ip -6 -o addr show dev $device | grep " fe80" | grep -qvw tentative && break; sleep 1; i=$[ $i - 1 ]; if [ $i -eq 0 ]; then exit 1; fi; done
	if [ $? -ne 0 ]; then
		Syslog "ERROR" "IPv6 link-local not existing: $device"
		return 1
	fi
	Syslog "INFO" "check for IPv6 link-local done: $device"

	# check for pingable NetCv
	Syslog "INFO" "check for reachable NetCv on device: $device"
	ping6="/usr/sbin/ping6"; if [ -x "/usr/bin/ping6" ]; then ping6="/usr/bin/ping6"; fi
	i=30; while [ $i -gt 0 ]; do $ping6 -c 1 -I $device ff02::16 -W 1 >/dev/null 2>&1 && break; i=$[ $i - 1 ]; if [ $i -eq 0 ]; then exit 1; fi; done
	if [ $? -ne 0 ]; then
		Syslog "ERROR" "NetCv not reachable on device: $device"
		return 1
	fi
	Syslog "INFO" "check for reachable NetCv done on device: $device"

	return 0
}


## setup ReelBox remote control via reelbox-ctrld
SetupRemote() {
	if [ ! -e /dev/uinput ]; then
		Syslog "INFO" "load kernel module 'uinput'"
		/sbin/modprobe uinput
		Syslog "INFO" "load kernel module 'uinput' done"
	else
		Syslog "INFO" "kernel module already loaded or compiled-in: 'uinput'"
	fi

	# start pre
	Syslog "INFO" "call: /opt/reel/sbin/reelbox-control.sh start_pre"
	/opt/reel/sbin/reelbox-control.sh start_pre

	# start reelbox-control.sh and generate environment
	Syslog "INFO" "retrieve environment with 'reelbox-control.sh'"
	/opt/reel/sbin/reelbox-control.sh print_daemon_args >/var/run/reel-boxctrld.env
	Syslog "INFO" "retrieve environment with 'reelbox-control.sh' done"

	. /var/run/reel-boxctrld.env
	Syslog "INFO" "start 'reelbox-ctrld' with REEL_BOXCTRLD_ARGS=\"${REEL_BOXCTRLD_ARGS}\""
	/opt/reel/sbin/reelbox-ctrld ${REEL_BOXCTRLD_ARGS}
	Syslog "INFO" "call: /opt/reel/sbin/reelbox-control.sh start_post"
	/opt/reel/sbin/reelbox-control.sh start_post
	i=10
	while [ $i -gt 0 ]; do
		if [ -e /dev/input/rbfp0 ]; then
			break
		else
			sleep 1
		fi
       		i=$[ $i - 1 ]
	done;
       	if [ $i -eq 0 ]; then
		Syslog "ERROR" "start of 'reelbox-ctrld' was not successful"
		return 1
	fi
	Syslog "INFO" "start 'reelbox-ctrld' done"
	return 0
}


## stop reelbox-ctrld
StopRemote() {
	Syslog "NOTICE" "stop of 'reelbox-ctrld' disabled to avoid break of shutdown"
	return 0
	local reelbox_ctrld_pid=$(pidof reelbox-ctrld)
	if [ -z "$reelbox_ctrld_pid" ]; then
		Syslog "NOTICE" "'reelbox-ctrld' is not running"
		return 0
	fi

	Syslog "INFO" "found running 'reelbox-ctrld' with pid: $reelbox_ctrld_pid (need to be killed)"
	kill $reelbox_ctrld_pid
	for i in $(seq 1 10); do
		if [ -z "$(pidof reelbox-ctrld)" ]; then
			Syslog "INFO" "'reelbox-ctrld' successfully killed with SIGTERM"
			break
		fi
		sleep 1
	done
	if [ -n "$(pidof reelbox-ctrld)" ]; then
		Syslog "NOTICE" "'reelbox-ctrld' requires to terminate with SIGKILL"
		kill -9 $reelbox_ctrld_pid
		sleep 1
	fi
	return 0
}

## setup eHD Kernel
SetupEhdKernel() {
	if [ "$HD_SHM" != "yes" ]; then
		Syslog "ERROR" "setup of eHD is not enabled: HD_SHM=$HD_SHM (requires 'yes')"
		return 0
	fi

	if [ -z "$HD_SHM_MODULE" ]; then
		Syslog "ERROR" "setup of eHD/kernel requires HD_SHM_MODULE set"
		return 1
	fi

	if [ -z "$HD_SHM_OPTIONS" ]; then
		Syslog "ERROR" "setup of eHD/kernel requires HD_SHM_OPTIONS set"
		return 1
	fi

	local hdshm_module_file="/lib/modules/$(uname -r)/$HD_SHM_MODULE"
	if [ ! -e "$hdshm_module_file" ]; then
		Syslog "ERROR" "setup of eHD/kernel requires module: $hdshm_module_file"
		return 1
	fi

	insmod $hdshm_module_file $HD_SHM_OPTIONS
	rc=$?
	if [ $rc -ne 0 ]; then
		Syslog "ERROR" "setup of eHD/kernel can't load module: $hdshm_module_file $HD_SHM_OPTIONS"
		return 1
	fi

	HD_SHM_DEVICE="/dev/hdshm"
	if [ ! -e $HD_SHM_DEVICE ]; then
		Syslog "ERROR" "setup of eHD/kernel misses device: $HD_SHM_DEVICE"
		return 1
	fi

	HD_SHM_GROUP="${HD_SHM_GROUP:-video}"
	chgrp $HD_SHM_GROUP $HD_SHM_DEVICE
	if [ $rc -ne 0 ]; then
		Syslog "ERROR" "setup of eHD/kernel can't change group: $HD_SHM_DEVICE ($HD_SHM_GROUP)"
		return 1
	fi

	chmod g+rw $HD_SHM_DEVICE
	if [ $rc -ne 0 ]; then
		Syslog "ERROR" "setup of eHD/kernel can't adjust permissions to g+rw: $HD_SHM_DEVICE"
		return 1
	fi

	Syslog "INFO" "setup of eHD/kernel module successfully loaded: $hdshm_module_file $HD_SHM_OPTIONS"
}

## setup eHD Boot
SetupEhdBoot() {
	if [ "$HD_SHM" != "yes" ]; then
		Syslog "ERROR" "setup of eHD is not enabled: HD_SHM=$HD_SHM (requires 'yes')"
		return 0
	fi

	if [ -z "$HD_BOOT_IMAGE" ]; then
		Syslog "ERROR" "setup of eHD/boot requires defined HD_BOOT_IMAGE"
		return 1
	fi

	if [ ! -e "$HD_BOOT_IMAGE" ]; then
		Syslog "ERROR" "setup of eHD/boot requires image: $HD_BOOT_IMAGE"
		return 1
	fi

	if [ -z "$HD_BOOT_TIMEOUT" ]; then
		HD_BOOT_TIMEOUT=20
		Syslog "NOTICE" "setup of eHD/boot uses default timeout: $HD_BOOT_TIMEOUT"
	fi

	if [ -z "$HD_BOOT_BIN" ]; then
		HD_BOOT_BIN="/opt/reel/sbin/hdboot"
		Syslog "NOTICE" "setup of eHD/boot uses default binary: $HD_BOOT_BIN"
	fi

	if [ ! -x "$HD_BOOT_BIN" ]; then
		Syslog "ERROR" "setup of eHD/boot requires executable: $HD_BOOT_BIN"
		return 1
	fi

	$HD_BOOT_BIN -r -w $HD_BOOT_TIMEOUT -i $HD_BOOT_IMAGE
	rc=$?
	if [ $rc -ne 0 ]; then
		Syslog "ERROR" "setup of eHD/boot can't load image: $HD_BOOT_IMAGE"
		return 1
	fi

	Syslog "INFO" "setup of eHD/boot successfully loaded image: $HD_BOOT_IMAGE"
}


## setup eHD network
SetupEhdNetwork() {
	if [ "$HD_SHM" != "yes" ]; then
		Syslog "ERROR" "setup of eHD is not enabled: HD_SHM=$HD_SHM (requires 'yes')"
		return 0
	fi

	if [ -z "$HD_NETD_BIN" ]; then
		HD_NETD_BIN="/opt/reel/sbin/shmnetd"
		Syslog "NOTICE" "setup of eHD/network uses default binary: $HD_NETD_BIN"
	fi

	if [ ! -x "$HD_NETD_BIN" ]; then
		Syslog "ERROR" "setup of eHD/network requires executable: $HD_NETD_BIN"
		return 1
	fi

	if [ -z "$HD_NETD_TIMEOUT" ]; then
		HD_NETD_TIMEOUT=10
		Syslog "NOTICE" "setup of eHD/network uses default timeout: $HD_NETD_TIMEOUT"
	fi

	case $1 in
	    stop)
		local pid=$(pidof $(basename $HD_NETD_BIN))
		if [ -n "$pid" ]; then
			Syslog "NOTICE" "kill on request 'stop' running: $HD_NETD_BIN (pid=$pid)"
			kill $pid
		fi
		return 0
		;;

	    start)
		$HD_NETD_BIN &
		local pid=$!

		local i=$HD_NETD_TIMEOUT

		# check whether daemon has started successfully
		while [ $i -gt 0 ]; do
			sleep 1
			i=$[ $i - 1 ]
			# daemon still running?
			if ! ps --no-headers -p $pid >/dev/null; then
				Syslog "ERROR" "setup of eHD/network failed, daemon no longer running: $HD_NETD_BIN"
				return 1
			fi

			# tun interface active?
			if ! ip -o link show dev tun0 >/dev/null; then
				continue
			fi

			# tun IPv4 active?
			if ! ip -o -4 addr show dev tun0 >/dev/null; then
				continue
			fi

			# eHD pingable
			if ! ping -q -c 1 $HD_NETD_IP_EHD >/dev/null; then
				continue
			fi

			# eHD reachable via telnet
			local output=$(echo "exit" | nc -d 0.2 -t $HD_NETD_IP_EHD 23)
			if ! echo "$output" | grep -q "extensionHD"; then
				continue
			fi

			break
		done

		if [ $i -eq 0 ]; then
			Syslog "ERROR" "setup of eHD/network can't reach eHD"
			if ps --no-headers -p $pid >/dev/null; then
				Syslog "NOTICE" "kill running but not working: $HD_NETD_BIN"
				kill $pid
			fi
			return 1
		fi

		Syslog "INFO" "setup of eHD/network successfully working, eHD reachable: $HD_NETD_IP_EHD"
		;;

	    status)
		local pid=$(pidof $(basename $HD_NETD_BIN))
		if [ -z "$pid" ]; then
			Syslog "WARN" "process not running: $HD_NETD_BIN"
			return 1
		fi

		# eHD pingable
		if ! ping -c 1 $HD_NETD_IP_EHD >/dev/null; then
			Syslog "WARN" "can't reach eHD: $HD_NETD_IP_EHD"
			return 1
		fi

		# eHD reachable via telnet
		local output=$(echo "exit" | nc -d 0.2 -t $HD_NETD_IP_EHD 23)
		if ! echo "$output" | grep -q "extensionHD"; then
			Syslog "WARN" "can reach eHD via telnet but output is not expected: $HD_NETD_IP_EHD ($output)"
			return 1
		fi
		;;

	    *)
		Syslog "NOTICE" "unsupported option: $1"
		;;
	esac
}


## command eHD
CommandEhd() {
	SetupEhdNetwork status || return 1

	Syslog "INFO" "execute eHD command: $*"
	echo -e "\n$*\n" | nc -i 3 -d 0.2 -t $HD_NETD_IP_EHD 23
}

## Basic checks
if [ ! -x "$REELFPCTL" ]; then
	Syslog "WARN" "No executable found: $REELFPCTL"
	exit 1
fi

Syslog "INFO" "called by: $(ps --no-headers -o cmd $PPID) with: $*"

arg1="$1"
shift

case $arg1 in
    print_daemon_args)
	print_daemon_args $*
	;;
    start_post)
	start_post
	;;
    start_pre)
	start_pre
	;;
    set_wakeup)
	SetWakeup $*
	;;
    setup_remote)
	SetupRemote $*
	;;
    stop_remote)
	StopRemote $*
	;;
    setup_netceiver)
	SetupNetceiver $*
	;;
    setup_frontpanel)
    	SetupFrontpanel $*
	;;
    image_frontpanel)
	ImageFrontpanel $*
	;;
    text_frontpanel)
	SetFrontpanel text $*
	;;
    set_frontpanel)
	SetFrontpanel $*
	;;
    set_led_status)
	SetLEDStatus $*
	;;
    set_led_button)
	SetLEDButton $*
	;;
    setup_ehd_kernel)
	SetupEhdKernel $*
	;;
    setup_ehd_boot)
	case $1 in
	    reboot)
		SetupEhdNetwork stop && SetupEhdBoot && SetupEhdNetwork start
		;;
	    *)
		SetupEhdBoot $*
		;;
	esac
	;;
    setup_ehd_network)
	SetupEhdNetwork $*
	;;
    command_ehd)
	CommandEhd $*
	;;
    *)
	cat <<END
Supported arg1
	start_pre
		setup/check reelbox frontpanel "pre" daemon start
	start_post
		setup reelbox frontpanel "post" daemon start
	set_wakeup <wakeup time>
		setup wakeup time via frontpanel clock
	setup_frontpanel <serial device>
		setup serial device for frontpanel
	setup_netceiver <network interface>
		setup interface to netceiver and test reachability
	setup_remote
		setup (start) remote control by preparing and starting 'reelbox-ctrld'
	stop_remote
		stop 'reelbox-ctrld'
	set_frontpanel start|stop|stopping|text <text>
		set frontpanel options
	text_frontpanel [-clock] <text>
		display text on frontpanel, optional with clock
	image_frontpanel <file>
		display image file on frontpanel
	set_led_status {standby|remote|power|record|ALL} {on|off|blink} [<brightness 1-15>]
		enable/disable/blink LED
	set_led_button <color>|off|on [<brightness 1-15>|max]
		set color of button LEDs
	setup_ehd_kernel
		setup eHD load kernel module
	setup_ehd_boot [reboot]
		setup eHD load boot image (reboot includes network stop/start)
	setup_ehd_network start|stop|status
		setup eHD network action
	command_ehd <command>
		send command to eHD
END
	;;
esac
