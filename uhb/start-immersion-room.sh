#!/bin/sh

groot="edu.berkeley.eecs.swarmlab.immersion-room.device"
mqtt_host="uhkbbb001.eecs.berkeley.edu"
args="-s -M $mqtt_host -d"
gw_prog="uhb/mqtt-gdp-gateway"
devices=`mosquitto_sub -h $mqtt_host -C 1 -t gateway-topics | \
	sed -e 's/^..//' -e 's/..$//' -e 's/", *"/ /g' -e 's/device\/[a-zA-Z]*\///g'`
echo Devices: $devices
for i in $devices
do
	if apps/gcl-create -q -e none $groot.$i
	then
		echo "Created log $groot.$i"
	fi
	args="$args device/+/$i $groot.$i"
done

echo "running $gw_prog $args"
$gw_prog $args
