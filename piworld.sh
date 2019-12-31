#!/bin/bash
# This script is used to run the release build of piworld.
# It first checks that it's being run from an extracted archive. Then it checks
# which piworld build to run based on the existance of the vc4 module (if vc4
# exists then the Mesa OpenGL driver is in use).

PWDIR=`dirname $0`

runpiworld() {
    if [ -x "$PWDIR/bin/piworld-brcm" ] ; then
        if lsmod | grep --quiet "^vc4" ; then
            $PWDIR/bin/piworld-mesa "$@"
        else
            $PWDIR/bin/piworld-brcm "$@"
        fi
    else
        zenity --info --text="Please extract the archive first, then run
piworld from the extracted folder."
    fi
}

VER=
if [[ $VER == "" ]]; then
    # If version not specified then run dev build.
    $PWDIR/piworld "$@"
    exit
fi

# If running from the archive file use the command line xarchiver was called
# with to locate the archive file itself.
ARCHIVE_NAME=piworld-$VER
ARC_CMD_LINE=`ps -eo args | grep xarchiver | grep $ARCHIVE_NAME`
PKG_PATH=`echo $ARC_CMD_LINE | cut -d' ' -f2`

if [[ -f $PKG_PATH ]]; then
    PKG_DIR=`dirname $PKG_PATH`

    # This script is being run from within the tar.xz release archive.
    if [[ -d $PKG_DIR ]]; then
        EXTRACTED_DIR=$PKG_DIR/$ARCHIVE_NAME
        if [[ -d $EXTRACTED_DIR ]]; then
            # Run piworld from previously extracted dir.
            PWDIR=$EXTRACTED_DIR
            runpiworld "$@"
            exit
        fi
        # Extract and then run piworld.
        cd $PKG_DIR
        tar xvf $PKG_PATH
        PWDIR=$PKG_DIR/$ARCHIVE_NAME
        runpiworld "$@"
        exit
    fi
fi

# Run piworld from its extracted location.
runpiworld "$@"

