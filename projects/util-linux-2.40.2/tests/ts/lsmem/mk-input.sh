#!/bin/bash
#
# This script makes a copy of relevant files from /sys.
# The files are useful for lsmem(1) regression tests.
#
progname=$(basename $0)

if [ -z "$1" ]; then
	echo -e "\nusage: $progname <testname>\n"
	exit 1
fi

TS_NAME="$1"
TS_DUMP="$TS_NAME"
CP="cp -r --parent"

mkdir -p $TS_DUMP/sys/devices/system

$CP /sys/devices/system/memory $TS_DUMP

tar jcvf $TS_NAME.tar.bz2 $TS_DUMP
rm -rf $TS_DUMP
