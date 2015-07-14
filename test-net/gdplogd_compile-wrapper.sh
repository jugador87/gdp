#!/bin/bash

# A script that gets executed while creating a docker image.
# Takes in the source and creates a .deb package, then installs it.
# The reason we'd like to do that is:
# - Makes sure that there is no chance of incompatibility on the machine
#   the code is compiled with the machine the code is executed.
# - Makes sure we can run post-install scripts as it is from the debian
#   package
# - Does not require debian build packages on the host machine (Mac OS)
#
# This is executed as root

VER=$1
DEBNAME="gdp-server_"$VER"_amd64.deb"

apt-get update 
apt-get -y install build-essential libevent-dev libssl-dev lighttpd libjansson-dev
cp -av /gdp /tmp/gdp
cd /tmp/gdp/ && ./deb-pkg/package-server.sh $VER 
cp /tmp/gdp/$DEBNAME /gdp/test-net/.
chown --reference=$0 /gdp/test-net/$DEBNAME
