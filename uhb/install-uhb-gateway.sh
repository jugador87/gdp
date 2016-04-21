#!/bin/sh

cd `dirname $0`
root=`pwd`

info() {
	echo "$1"
}

fatal() {
	echo "$1" 1>&2
	exit 1
}


if [ `uname -s` != "Linux" ]
then
	fatal "This script only runs on Linux"
fi

VERSION_ID="0"
if [ -e /etc/os-release ]
then
	. /etc/os-release
fi

if [ "$ID" = "debian" ] && expr $VERSION_ID \< 8 > /dev/null
then
	fatal "Must be running Debian 8 (Jessie) or higher (have $VERSION_ID)"
fi

if ! ls /etc/apt/sources.list.d/mosquitto* > /dev/null 2>&1
then
	info "##### Setting up mosquitto repository"
	if [ "$ID" = "ubuntu" ]
	then
		sudo apt-get install -y \
			software-properties-common \
			python-software-properties
		sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa
	elif [ "$ID" = "debian" ]
	then
		sudo apt-get install -y wget
		wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
		sudo apt-key add mosquitto-repo.gpg.key
		if expr $VERSION_ID = 8 > /dev/null
		then
			dver="jessie"
		else
			fatal "unknown debian version $VERSION_ID"
		fi
		cd /etc/apt/sources.list.d
		sudo wget http://repo.mosquitto.org/debian/mosquitto-$dver.list
		cd $root
	else
		fatal "Unknown linux distribution $ID"
	fi
fi

# set up a few defaults
gdprepo=repo.eecs.berkeley.edu
gdpreporoot=projects/swarmlab
if [ `uname -m` = "armv7l" ]
then
	# beaglebone, not much disk space
	gitdepth="--depth 1"
else
	gitdepth=""
fi

echo 	""
info "##### Installing Debian packages"
sudo apt-get update
sudo apt-get install -y \
	avahi-daemon \
	curl \
	g++ \
	gcc \
	git \
	libavahi-compat-libdnssd-dev \
	libmosquitto-dev \
	locales \
	make \
	mosquitto-clients \

if [ `uname -m` != "armv7l" ]
then
	# we are not on a beaglebone --- done
	echo ""
	info "##### Not a Beaglebone; skipping the rest of the install"
	exit 0;
fi

echo ""
info "##### Installing additional Beaglebone-specific packages"
mkdir /var/lib/bluetooth && chmod 700 /var/lib/bluetooth
sudo apt-get install -y \
	bluetooth \
	bluez \
	libbluetooth-dev \
	libudev-dev

info "Enabling bluetooth daemon"
sudo update-rc.d bluetooth defaults

# check out the git tree from UMich
echo ""
info "##### Checking out Gateway source tree from Michigan"
cd $root
git clone $gitdepth https://github.com/lab11/gateway.git
cd gateway

# verify that we have checked things out
if [ ! -d software -o ! -d systemd ]
then
	fatal "$0 must be run from root of gateway git tree" 1>&2
fi

echo ""
info "##### Set up node.js"
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
	info "Initializing for package $i"
	npm install $i
done

echo ""
info "##### Clearing NPM cache"
npm cache clean

# install system startup scripts
cd $root/gateway/systemd
echo ""
info "##### Installing system startup scripts"
sudo cp *.service /etc/systemd/system

echo ""
info "##### Selectively enabling system startup scripts"

enable() {
	info "Enabling service $1"
	sudo systemctl enable $1
}

skip() {
	if [ -z "$2" ]
	then
		info "Skipping service $1"
	else
		info "Skipping service $1: $2"
	fi
}

enable	adv-gateway-ip
enable	ble-address-sniffer-mqtt
enable	ble-gateway-mqtt
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


# Should this be up above, so it works on all platforms?
# Or should it just install packages rather than download and compile?
echo ""
info "##### Fetching GDP from repo"
cd $root
if ! git clone $gitdepth repoman@$gdprepo:$gdpreporoot/gdp.git &&
   ! git clone $gitdepth https://$gdprepo/$gdpreporoot/gdp.git
then
	fatal "Cannot clone GDP repo ${gdprepo}:$gdpreporoot/gdp.git"
fi
cd gdp

echo ""
info "##### Installing GDP prerequisites"
sh adm/gdp-setup.sh
# lighttpd uses port 80, as does gateway-server
sudo servicectl disable lighttpd

info "##### Compiling GDP"
make || fatal "Cannot build gdp"
cd uhb
make || fatal "Cannot build urban heartbeat kit support"
