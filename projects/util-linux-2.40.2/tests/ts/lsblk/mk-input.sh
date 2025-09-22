#!/bin/bash
#
# Copyright (C) 2018 Karel Zak <kzak@redhat.com>
#
# This script makes a copy of relevant files from /sys and /proc.
# The files are useful for lsblk(1) regression tests.
#
progname=$(basename $0)

if [ -z "$1" ]; then
	echo -e "\nusage: $progname <testname>\n"
	exit 1
fi

TS_DUMP="$1"
TS_NAME=$(basename ${TS_DUMP})
TS_TARBALL="$TS_DUMP.tar.xz"
TS_CMD_LSBLK=${TS_CMD_LSBLK:-"lsblk"}

#
# procfs
#
mkdir -p $TS_DUMP/proc
mkdir -p $TS_DUMP/proc/self
cp /proc/self/mountinfo ${TS_DUMP}/proc/self
cp /proc/swaps ${TS_DUMP}/proc/swaps
cp /proc/version ${TS_DUMP}/proc/version


#
# sysfs
#
mkdir -p $TS_DUMP/sys/{block,dev/block}
cp --no-dereference /sys/dev/block/* ${TS_DUMP}/sys/dev/block
cp --no-dereference /sys/block/* ${TS_DUMP}/sys/block

DEVS=$(find /sys/dev/block/ -type l)
for x in ${DEVS}; do
	DEV="/sys/dev/block/$(readlink $x)"

	mkdir -p ${TS_DUMP}/${DEV}

	# attributes
	for f in $(find ${DEV} -type f -not -path '*/trace/*' -not -path '*/uevent'); do
		if [ ! -f ${TS_DUMP}/${f} ]; then
			SUB=$(dirname $f)
			mkdir -p ${TS_DUMP}/${SUB}
			cp $f ${TS_DUMP}/$f 2> /dev/null
		fi
	done

	# symlinks (slave, holders, etc.)
	for f in $(find ${DEV} -type l -not -path '*/subsystem' -not -path '*/bdi'); do
		if [ ! -f ${TS_DUMP}/${f} ]; then
			SUB=$(dirname $f)
			mkdir -p ${TS_DUMP}/${SUB}
			cp --no-dereference $f ${TS_DUMP}/$f
		fi
	done

	# device/ files
	if [ -d ${DEV}/device/ ]; then
		for f in $(find ${DEV}/device/ -maxdepth 1 -type f -not -path '*/uevent'); do
			if [ ! -f ${TS_DUMP}/${f} ]; then
				SUB=$(dirname $f)
				cp $f ${TS_DUMP}/$f 2> /dev/null
			fi
		done
	fi

done

#
# udev a lsblk specific
#
mkdir -p $TS_DUMP/dev
DEVS=$(lsblk --noheadings --output PATH)
for d in $DEVS; do

	# udev
	udevadm info --query=property $d > $TS_DUMP/$d

	# lsblk
	echo "OWNER=$($TS_CMD_LSBLK --noheadings --nodeps --output OWNER $d)" >> $TS_DUMP/$d
	echo "GROUP=$($TS_CMD_LSBLK --noheadings --nodeps --output GROUP $d)" >> $TS_DUMP/$d
	echo "MODE=$($TS_CMD_LSBLK  --noheadings --nodeps --output MODE  $d)" >> $TS_DUMP/$d
done

function mk_output {
	local cols="NAME,${2}"
	local subname="$1"

	echo "$cols" > ${TS_DUMP}/${subname}.cols
	$TS_CMD_LSBLK -o ${cols} > ${TS_DUMP}/lsblk-${TS_NAME}-${subname}
}


LANG="POSIX"
LANGUAGE="POSIX"
LC_ALL="POSIX"
CHARSET="UTF-8"

export LANG LANGUAGE LC_ALL CHARSET

#
# lsblk info
#
$TS_CMD_LSBLK -V &> ${TS_DUMP}/version

mk_output basic KNAME,MAJ:MIN,RM,SIZE,TYPE,MOUNTPOINT
mk_output vendor MODEL,VENDOR,REV
mk_output state RO,RM,HOTPLUG,RAND,STATE,ROTA,TYPE,PKNAME,SCHED
mk_output rw RA,WSAME
mk_output topo SIZE,ALIGNMENT,MIN-IO,OPT-IO,PHY-SEC,LOG-SEC,RQ-SIZE
mk_output discard DISC-ALN,DISC-GRAN,DISC-MAX,DISC-ZERO
mk_output zone ZONED


tar --xz -cvf ${TS_TARBALL} $TS_DUMP
rm -rf $TS_DUMP

echo -e "\nPlease, send ${TS_TARBALL} to util-linux upstream. Thanks!\n"


