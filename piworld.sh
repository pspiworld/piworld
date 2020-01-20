#!/bin/bash
# This script is used to run piworld.
# It first checks that it's being run from an extracted archive. Then it checks
# which piworld build to run based on the existance of the vc4 module (if vc4
# exists then the Mesa OpenGL driver is in use).

PWDIR=$(dirname "$0")
RUN_FROM_ARCHIVE=1
if [ -x "$PWDIR/bin/piworld-brcm" ] ; then
    RUN_FROM_ARCHIVE=0
fi

runpiworld() {
    if [ $RUN_FROM_ARCHIVE = 0 ] ; then
        arch=$(uname -m)
        longbit=$(getconf LONG_BIT)
        if [ "$arch" = "aarch64" ] ; then
            PWBIN="$PWDIR/bin/piworld-aarch64"
        elif lsmod | grep --quiet "^vc4" ; then
            PWBIN="$PWDIR/bin/piworld-mesa"
        elif [ "${arch:0:3}" = "arm" ] ; then
            PWBIN="$PWDIR/bin/piworld-brcm"
        elif [ "$arch" = "x86_64" ] && [ "$longbit" = "64" ] ; then
            PWBIN="$PWDIR/bin/piworld-x86_64"
        else
            PWBIN="$PWDIR/bin/piworld-x86"
        fi
        "$PWBIN" "$@"
    else
        zenity --info --text="Please extract the archive first, then run
piworld from the extracted folder."
    fi
}

VER=
if [[ $VER == "" ]]; then
    # If version not specified then run dev build.
    "$PWDIR/piworld" "$@"
    exit
fi

if [ $RUN_FROM_ARCHIVE = 1 ] ; then
    # If running from the archive file use the command line xarchiver was called
    # with to locate the archive file itself.
    ARCHIVE_NAME=piworld-$VER
    ARC_CMD_LINE=`ps -eo args | grep xarchiver | grep $ARCHIVE_NAME`
    PKG_PATH=`echo $ARC_CMD_LINE | cut -d' ' -f2`

    if [[ -f $PKG_PATH ]]; then
        PKG_DIR=$(dirname $PKG_PATH)

        # This script is being run from within the tar.xz release archive.
        if [[ -d $PKG_DIR ]]; then
            RUN_FROM_ARCHIVE=0
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
fi

# Run piworld from its extracted location.
runpiworld "$@"

