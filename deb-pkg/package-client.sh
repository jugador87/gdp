#!/bin/sh

# Create a debian package that includes:
# - gdplogd
# - gdp-machine-mon

if [ $# -gt 0 ]; then
    VER=$1
    MAJVER=`echo $VER | cut -d '.' -f 1`
    MINVER=`echo $VER | cut -d '.' -f 2`
else
    echo "Usage: $0 <version (format: X.Y>"
    exit 1
fi


PACKAGE="gdp-client"
curdir=`dirname $0`
topdir="`( cd $curdir/../ && pwd )`"
tmpdir="/tmp/"$PACKAGE"_"$VER

rm -rf $tmpdir
mkdir $tmpdir

# invoke 'make'
cd $topdir && make all 

# copy files

# ep header files
mkdir -p $tmpdir/usr/include/ep
install -m 0644 $topdir/ep/*.h          $tmpdir/usr/include/ep/
install -m 0644 $topdir/ep/LICENSE      $tmpdir/usr/include/ep/

# gdp header files
mkdir -p $tmpdir/usr/include/gdp
install -m 0644 $topdir/gdp/*.h         $tmpdir/usr/include/gdp/
install -m 0644 $topdir/gdp/LICENSE     $tmpdir/usr/include/gdp/

# documentation
mkdir -p $tmpdir/usr/share/doc/gdp
install -m 0644 $topdir/doc/*.html      $tmpdir/usr/share/doc/gdp/
mkdir -p $tmpdir/usr/share/doc/gdp/tutorial
install -m 0644 $topdir/doc/tutorial/*  $tmpdir/usr/share/doc/gdp/tutorial/

# examples
mkdir -p $tmpdir/usr/share/doc/gdp/examples
install -m 0644 $topdir/examples/*      $tmpdir/usr/share/doc/gdp/examples/

# programs
mkdir -p $tmpdir/usr/bin/
for file in gcl-create gdp-writer gdp-reader log-view; do
    install -D  $topdir/apps/$file      $tmpdir/usr/bin/$file
done

# actual library code
mkdir -p $tmpdir/usr/lib
for l in ep gdp ; do
    install -m 0644 $topdir/$l/lib$l.a           $tmpdir/usr/lib/

    solibname=`basename $topdir/$l/lib$l.so.*`
    dylibname=$solibname.dylib
    solibname1=`echo $solibname | rev | cut -d '.' -f 2- | rev` 
    solibname2=`echo $solibname | rev | cut -d '.' -f 3- | rev` 
    
    install -m 0644 $topdir/$l/$solibname        $tmpdir/usr/lib/
    (cd $tmpdir/usr/lib && ln -s $solibname $dylibname)
    (cd $tmpdir/usr/lib && ln -s $solibname $solibname1)
    (cd $tmpdir/usr/lib && ln -s $solibname $solibname2)

done


# deb package control files
cp -a $topdir/deb-pkg/client/DEBIAN $tmpdir/.
sed -i "s/VERSION/$VER/g" $tmpdir/DEBIAN/control
sed -i "s/PACKAGE/$PACKAGE/g" $tmpdir/DEBIAN/control
sed -i "s/ARCH/`dpkg --print-architecture`/g" $tmpdir/DEBIAN/control

# Build package
fakeroot dpkg-deb --build $tmpdir .
rm -rf $tmpdir

