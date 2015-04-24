#!/bin/sh

# Create a debian package. Some reasons this should be a deb-package:
# - Certain dependencies (python/ctypes/psutil/...)
# - Ideal if installed in default python path
# - depends on location of libgdp and libep etc

if [ $# -gt 0 ]; then
    VER=$1
else
    echo "Usage: $0 <version (format: X.Y-Z)>"
    exit 1
fi

PACKAGE="python-gdp"
curdir=`dirname $0`
topdir="`( cd $curdir/../ && pwd )`"
tmpdir="/tmp/"$PACKAGE"_"$VER
pydir="/usr/lib/python2.7/dist-packages/"
sharedir="/usr/share/doc/gdp/python-gdp"


rm -rf $tmpdir
mkdir $tmpdir
mkdir -p $tmpdir/$pydir/gdp
mkdir -p $tmpdir/$sharedir/


cp -a $topdir/gdp/*.py $tmpdir/$pydir/gdp/.
# fix library locations
sed -i "s/\"..\", \"..\", \"..\", \"libs\"/\"\/\", \"usr\", \"lib\"/g" $tmpdir/$pydir/gdp/MISC.py

# documentation and examples
cp $topdir/README $tmpdir/$sharedir/
cp -a $topdir/apps/*.py $tmpdir/$sharedir/
sed -i "s/sys.path.append/# sys.path.append/g" $tmpdir/$sharedir/*.py

mkdir $tmpdir/DEBIAN
echo "Package: $PACKAGE
Version: $VER
Priority: optional
Architecture: all
Depends: python (<< 2.8), python (>= 2.7~), libgdp (>=0.2-1), libevent-dev (>=2.0~), python-psutil (>= 0.5~)
Maintainer: Nitesh Mor <mor@eecs.berkeley.edu>
Description: A python interface for libGDP
 Some sample programs are located at $sharedir/examples
 Enjoy!" >> $tmpdir/DEBIAN/control

dpkg-deb --build $tmpdir .

rm -rf $tmpdir
