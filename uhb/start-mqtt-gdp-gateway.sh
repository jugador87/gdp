#!/bin/sh

cd `dirname $0`/..
root=`pwd`
. $root/adm/common-support.sh

# set defaults
groot="edu.berkeley.eecs.swarmlab.device"
mqtt_host="uhkbbb001.eecs.berkeley.edu"

args=`getopt M:r: $*`
if [ $? != 0 ]
then
	fatal "Usage: $0 [ -M mqtt-broker ] [ -r logname-root ]"
fi

set -- $args
for i
do
	case "$i"
	in
		-M)
			mqtt_host="$2"
			shift; shift;;

		-r)
			groot="$2"
			shift; shift;;
		--)
			shift; break;;
	esac
done

info "Using log names $groot/*"
info "Using MQTT server at $mqtt_host"

args="-s -M $mqtt_host -d"
gw_prog="uhb/mqtt-gdp-gateway"
devices=`mosquitto_sub -h $mqtt_host -C 1 -t gateway-topics | \
	sed -e 's/^..//' -e 's/..$//' -e 's/", *"/ /g' -e 's/device\/[a-zA-Z]*\///g'`
if [ -z "$devices" ]
then
	fatal "Cannot locate any devices to log"
fi
info "Devices: $devices"
for i in $devices
do
	if apps/gcl-create -q -e none $groot.$i
	then
		info "Created log $groot.$i"
	fi
	args="$args device/+/$i $groot.$i"
done

info "running $gw_prog $args"
$gw_prog $args
