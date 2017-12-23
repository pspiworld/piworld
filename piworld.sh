#!/bin/bash
# This script is used to run the release build of piworld.
# It first checks that it's being run from an extracted archive. Then it checks
# which piworld build to run based on the existance of the vc4 module (if vc4
# exists then the experimental OpenGL driver is in use).

cd `dirname $0`
if [ -x "./bin/piworld-brcm" ] ; then
    if lsmod | grep --quiet "^vc4" ; then
        ./bin/piworld-mesa "$@"
    else
        ./bin/piworld-brcm "$@"
    fi;
else
    zenity --info --text="Please extract the archive first, then run
piworld from the extracted folder."
fi;
cd -

