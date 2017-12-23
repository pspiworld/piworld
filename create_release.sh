#!/bin/bash -e

# Assume the latest git tag is the release version.
VER=`git tag --contains | head -n1`
TMPDIR=/tmp/piworld-$VER

mkdir -p ./bin

# Build to use the brcm driver on a new Raspbian install.
cmake -DRELEASE=1 -DMESA=0 -DRASPI=1 .
make -j4
cp ./piworld ./bin/piworld-brcm

# Build to use the experimental OpenGL driver on the Pi 2 or 3.
cmake -DRELEASE=1 -DMESA=1 -DRASPI=2 .
make -j4
cp ./piworld ./bin/piworld-mesa

mkdir $TMPDIR
mkdir -p $TMPDIR/bin
cp ./bin/piworld-* $TMPDIR/bin/
cp piworld.sh $TMPDIR/piworld

mkdir -p $TMPDIR/shaders
cp ./shaders/*.glsl $TMPDIR/shaders/

mkdir -p $TMPDIR/textures
cp ./textures/*.png $TMPDIR/textures/

# Copy the Controls section out of README.md to create the release readme file.
sed -n '/^### Controls/,/^#/p' README.md | head -n -2 > $TMPDIR/readme

# Create the tar.xz file.
cd $TMPDIR/..
tar cvf piworld-$VER.tar piworld-$VER
xz -f piworld-$VER.tar
rm $TMPDIR -rf
cd -

echo "Release tarball: /tmp/piworld-$VER.tar.xz"

