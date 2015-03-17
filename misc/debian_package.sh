#!/usr/bin/env bash
VERSION=019c
DIR=bitfighter-$VERSION
TARBALL=bitfighter-$VERSION.tar.gz
./misc/build_debian_tarball.sh
rm -r debian-package
mkdir debian-package
mv $TARBALL debian-package/
cd debian-package
tar -xf $TARBALL
cd $DIR
dh_make -f ../$TARBALL --single --yes
dpkg-buildpackage -uc -us
lintian --info --display-level '+>=pedantic' *.deb
