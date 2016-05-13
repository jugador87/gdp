#!/bin/sh

cd `dirname $0`/..
root=`pwd`
. adm/common-support.sh

if [ $# != 2 ]
then
	fatal "Usage: $0 mqtt-broker logname-root"
fi

mqtt_host=$1
groot=$2

info "Using MQTT server at $mqtt_host"
info "Using log names $groot.*"

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
exec sudo -u gdp $gw_prog $args
