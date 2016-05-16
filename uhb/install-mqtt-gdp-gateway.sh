#!/bin/sh

#
#  Right now assumes you are using Upstart.  Should adapt to systemd.
#
#	Only tested on certain Debian-derived systems.
#	Sorry, will not work on MacOS or FreeBSD.
#

cd `dirname $0`/..
root=`pwd`

. adm/common-support.sh

# default root of installation directories (usually /usr or /usr/local)
: ${INSTALLROOT:=/usr}

# GDP subtree within $INSTALLROOT
: ${GDPROOT:=$INSTALLROOT/gdp}

# Installation program
: ${INSTALL:=install}

if [ `uname -s` != Linux ]
then
	fatal "Only works on (some) Linux systems"
fi

# create GDP user if necessary
if ! grep -q '^gdp:' /etc/passwd
then
	info "Creating new gdp user"
	sudo adduser --system --group gdp
fi

# make system directories if needed
if [ ! -d $GDPROOT ]
then
	info "Creating $GDPROOT"
	sudo mkdir -p $GDPROOT
	sudo chown gdp:gdp $GDPROOT
	for d in bin sbin etc lib adm
	do
		sudo mkdir $GDPROOT/$d
		sudo chown gdp:gdp $GDPROOT/$d
	done
fi

info "Installing mqtt-gdp-gateway program and documentation"
(cd uhb; make mqtt-gdp-gateway)
sudo $INSTALL -o gdp -g gdp uhb/mqtt-gdp-gateway ${INSTALLROOT}/sbin
manroot=${INSTALLROOT}/share/man
test -d $manroot || manroot=${INSTALLROOT}/man
sudo $INSTALL -o gdp -g gdp uhb/mqtt-gdp-gateway.1 $manroot/man1

info "Installing mqtt-gdp-gateway startup scripts"
sudo $INSTALL -o gdp -g gdp uhb/start-mqtt-gdp-gateway.sh $GDPROOT/sbin
sudo $INSTALL -o gdp -g gdp adm/common-support.sh $GDPROOT/adm

info "Installing Upstart system startup configuration"
sudo cp uhb/mqtt-gdp-gateway.conf uhb/mqtt-gdp-gateways.conf /etc/init
sudo initctl check-config --system mqtt-gdp-gateway
sudo initctl check-config --system mqtt-gdp-gateways
if [ ! -e /etc/default/mqtt-gdp-gateway ]
then
	sudo dd of=/etc/default/mqtt-gdp-gateway << EOF
	# list of MQTT servers to monitor (use # to comment out lines)
	MQTT_SERVERS=`sed 's/#.*//' << 'EOF'
		localhost
EOF
`

	# root of GDP log name; the device name will be appended
	MQTT_LOG_ROOT="edu.berkeley.eecs.swarmlab.device"
EOF
	warn "Edit /etc/default/mqtt-gdp-gateway to define"
	warn "   MQTT_SERVERS and MQTT_LOG_ROOT"
fi
