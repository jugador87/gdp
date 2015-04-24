#!/bin/sh

# Create a debian package that includes:
# - gdplogd
# - gdp-rest
# - gdp-machine-mon

if [ $# -gt 0 ]; then
    VER=$1
else
    echo "Usage: $0 <version (format: X.Y-Z)>"
    exit 1
fi


PACKAGE="gdp"
curdir=`dirname $0`
topdir="`( cd $curdir/../ && pwd )`"
tmpdir="/tmp/"$PACKAGE"_"$VER

rm -rf $tmpdir
mkdir $tmpdir

# invoke 'make'
cd $topdir && make all 

# copy files
install -D -m=0644  $topdir/deb-pkg/server/gdplogd.conf                $tmpdir/etc/init/gdplogd.conf
install -D -m=0644  $topdir/deb-pkg/server/gdp-rest.conf               $tmpdir/etc/init/gdp-rest.conf
install -D -m=0644  $topdir/deb-pkg/server/gdp-machine-mon.conf        $tmpdir/etc/init/gdp-machine-mon.conf
install -D $topdir/gdplogd/gdplogd       $tmpdir/usr/bin/gdplogd
install -D $topdir/apps/gdp-rest         $tmpdir/usr/bin/gdp-rest
install -D $topdir/examples/machine-mon  $tmpdir/usr/bin/gdp-machine-mon

# deb package control files
cp -a $topdir/deb-pkg/server/DEBIAN $tmpdir/.
sed -i "s/VERSION/$VER/g" $tmpdir/DEBIAN/control

# Build package
dpkg-deb --build $tmpdir .
rm -rf $tmpdir

