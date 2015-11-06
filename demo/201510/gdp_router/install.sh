#!/bin/bash

## Run this as root.
## >> Installs click in /opt/
##



## make sure we have the required packages installed first
apt-get update && apt-get install -y git wget build-essential

## Do all this junk in /opt
rm -rf /opt/click && rm -rf /opt/click_gdp && mkdir -p /opt && cd /opt


## install jansson manually. Using ubuntu-packaged version should work too,
##  but let's not experiment too much
wget http://www.digip.org/jansson/releases/jansson-2.7.tar.bz2
bunzip2 -c jansson-2.7.tar.bz2 | tar xf -
cd jansson-2.7/ && ./configure && make && make check && make install
cd .. && rm -rf jansson*

## first get click code
git clone https://github.com/kohler/click.git

## then get gdp_router code
git clone https://github.com/nikhildps/gdp_router_click.git

## copy files
cp gdp_router_click/gdp_* click/elements/local/.

## run ./configure for click
cd click
./configure --enable-local --disable-linuxmodule
make elemlist

## Change the makefile to include jansson. Any cleaner way of doing it?
sed -i 's/LIBS\ =\ \ `$(CLICK_BUILDTOOL)\ --otherlibs`\ $(ELEMENT_LIBS)/LIBS\ =\ \ `$(CLICK_BUILDTOOL)\ --otherlibs`\ $(ELEMENT_LIBS)\ \/usr\/local\/lib\/libjansson.a/g' userlevel/Makefile


## Make
make install

## Create symlink
cd /opt && ln -s click/userlevel/click click_gdp

## cleanup
rm -rf /opt/gdp_router_click
