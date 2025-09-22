#!/bin/sh

FILTER="$1"

MESON_CONFIG_H="build/config.h"
AUTOCONF_CONFIG_H="./config.h"

if [ ! -f $MESON_CONFIG_H ]; then
	echo 'Meson is not ready in the build/ directory (try "meson build")'
	exit 1
fi

if [ ! -f $AUTOCONF_CONFIG_H ]; then
	echo 'Autotools are not ready (try "./autogen.sh; ./configure")'
	exit 1
fi

TMPFILE_MESON="/tmp/util-linux-meson"
TMPFILE_AUTOCONF="/tmp/util-linux-autoconf"

GREP_PATTERN="#define "

if [ "$FILTER" = "headers" ]; then
	GREP_PATTERN="#define .*_H[[:blank:]]"
fi

echo "===MESON===" > $TMPFILE_MESON
grep "$GREP_PATTERN" $MESON_CONFIG_H | sort >> $TMPFILE_MESON

echo "===AUTOCONF===" > $TMPFILE_AUTOCONF
grep "$GREP_PATTERN" $AUTOCONF_CONFIG_H | sort >> $TMPFILE_AUTOCONF

diff --side-by-side $TMPFILE_AUTOCONF $TMPFILE_MESON

rm -rf $TMPFILE_MESON $TMPFILE_AUTOCONF
