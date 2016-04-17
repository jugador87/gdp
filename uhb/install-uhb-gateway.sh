#!/bin/sh

# check to make sure we're on a Beaglebone (or something close)
if [ `uname -m` != "armv7l" ]
then
	echo "This script only runs on a Beaglebone"
	exit 1
fi

VERSION_ID="0"
if [ -e /etc/os-release ]
then
	. /etc/os-release
fi

if expr $VERSION_ID \< 8 > /dev/null
then
	echo "Must be running Debian 8 (Jessie) or higher (have $VERSION_ID)"
	exit 1
fi

echo ""
echo "##### Installing Debian packages"
sudo apt-get update
sudo apt-get install \
	avahi-daemon \
	bluetooth \
	bluez \
	git \
	libavahi-compat-libdnssd-dev \
	libbluetooth-dev \
	libmosquitto-dev \
	libudev-dev \
	mosquitto-clients \

root=`pwd`

# check out the git tree from UMich
echo ""
echo "##### Checking out Gateway source tree from Michigan"
git clone https://github.com/lab11/gateway.git
cd $root/gateway

# verify that we have checked things out
if [ ! -d software -o ! -d systemd ]
then
	echo "$0 must be run from root of gateway git tree" 1>&2
	exit 1
fi

echo ""
echo "##### Set up node.js"
curl -sL https://deb.nodesource.com/setup_5.x | sudo -E bash -
sudo apt-get install -y nodejs

# get the names of the packages that might run
cd systemd
pkgs=`ls -d *.service | sed 's/\.service//'`

# initialize remaining dependencies for each service
#	We also install the packages themselves even though they are
#	run out of the source tree; doing npm install in each source
#	directory causes duplicate dependencies, and our disks are
#	just too small.
cd $root/gateway/software
for i in $pkgs
do
	echo ""
	echo Initializing for package $i
	npm install $i
done

# install system startup scripts
cd $root/gateway/systemd
echo ""
echo "##### Installing system startup scripts"
sudo cp *.service /etc/systemd/system

echo ""
echo "##### Selectively enabling system startup scripts"

enable() {
	echo "Enabling service $1"
	sudo systemctl enable $i
}

skip() {
	if [ -z "$2" ]
	then
		echo "Skipping service $1"
	else
		echo "Skipping service $1: $2"
	fi
}

enable	adv-gateway-ip
enable	ble-address-sniffer-mqtt
skip	ble-gateway-mqtt	"startup failure"
skip	ble-nearby
skip	gateway-mqtt-emoncms	"not in use at Berkeley"
skip	gateway-mqtt-gatd	"not in use at Berkeley"
skip	gateway-mqtt-log	"not in use at Berkeley"
enable	gateway-mqtt-topics
enable	gateway-publish
enable	gateway-server
skip	gateway-ssdp
skip	gateway-watchdog-email
skip	ieee802154-monjolo-gateway


echo ""
echo "##### Installing GDP from repo"
cd $root
git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git ||
	git clone https://repo.eecs.berkeley.edu/projects/swarmlab/gdp.git ||
	{
		echo "Cannot clone GDP repo: bailing out" 1>&2
		exit 1
	}

