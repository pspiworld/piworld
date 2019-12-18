#!/bin/bash -e

# Raspbian Stretch is the currently prefered OS version to build a release
# with.
[[ -e /etc/debian_version ]] \
    || echo Warning: building on unsupported OS, only Raspbian supported.
[[ -e /etc/debian_version && `cut -c-2 /etc/debian_version` == "9." ]] \
    || echo Warning: building on unsupported OS version - only Raspbian \
            Stretch supported.

# If the latest git commit has a tag use that as the release version, otherwise
# use a shortened form of the commit ID.
TAG=`git tag --contains | head -n1`
COMMIT_ID=`git show | head -n1 | cut -d' ' -f2`
SHORT_COMMIT_ID=${COMMIT_ID:0:7}
VER=${TAG:-$SHORT_COMMIT_ID}

TMPDIR=/tmp/piworld-$VER

if [[ $SKIP_BUILD == 1 ]]; then
    echo "Skipping build."
else
    # Build to use the brcm driver on Pi models 0 to 3 on Raspbian.
    cmake -DRELEASE=1 -DMESA=0 -DRASPI=1 -DDEBUG=0 .
    make -j4
    cp ./piworld ./bin/piworld-brcm

    # Build to use Mesa on the Pi 4 or the experimental OpenGL driver on the Pi
    # 2 or 3.
    cmake -DRELEASE=1 -DMESA=1 -DRASPI=2 -DDEBUG=0 .
    make -j4
    cp ./piworld ./bin/piworld-mesa
fi

mkdir $TMPDIR

cp piworld.sh $TMPDIR/piworld
# Embed the release version into the piworld run script.
sed -i -e "s/VER=/VER=$VER/" $TMPDIR/piworld

mkdir -p $TMPDIR/bin
cp ./bin/piworld-* $TMPDIR/bin/
cp ./bin/sign.dds $TMPDIR/bin/

mkdir -p $TMPDIR/shaders
cp ./shaders/*.glsl $TMPDIR/shaders/

mkdir -p $TMPDIR/textures
cp ./textures/*.png $TMPDIR/textures/

# Generate the release readme file including sections cut out of README.md.
for SECTION in "Controls" \
               "In Game Command Line" \
               "Startup Options" \
               "Sign Text Markup"; do
    sed -n "/^### $SECTION/,/^#/p" README.md | head -n -2 >> $TMPDIR/readme
    echo >> $TMPDIR/readme
done

cp LICENSE.md $TMPDIR/license

# Create the tar.xz file.
cd $TMPDIR/..
tar cvf piworld-$VER.tar piworld-$VER
xz -f piworld-$VER.tar
rm $TMPDIR -rf
cd -

echo "Release tarball: /tmp/piworld-$VER.tar.xz"

