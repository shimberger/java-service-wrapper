#!/bin/sh

BUILDFILE="`pwd`/`dirname $0`/build.xml"

echo "--------------------"
echo "Wrapper Build System"
echo "using $BUILDFILE"
echo "--------------------"

"$ANT_HOME/bin/ant" -f "$BUILDFILE" -Dbits=64 $@ 
