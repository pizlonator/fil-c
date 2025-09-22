#!/bin/bash

#
# This script checks if we have bash-completion scripts for the all compiled
# binaries.
#
# Copyright (C) 2016 Karel Zak <kzak@redhat.com>
#


die() {
	echo "error: $1"
	exit 1
}

usage() {
	echo "Usage:"
	echo " $0 [<top_srcdir>]"
}

# unwanted scripts -- use grep -E, e.g. (aaa|bbb|ccc)
completion_exclude="(nologin)"

top_srcdir=${1-"."}
completion_dir="${top_srcdir}/bash-completion"

[ -d "${completion_dir}" ] || die "not found ${completion_dir}"

bin_files=$(cd ${top_srcdir} && find * -maxdepth 0 -perm /u+x \
		\! -type d \
		\! -name \*.sh \! -name \*.cache \! -name \*.status \
		\! -name configure \! -name libtool | sort)

completion_files=$(cd ${completion_dir}/ && find * ! -name '*.am' | sort -u)
completion_missing=$(comm -3 <(echo "$completion_files") <(echo "$bin_files"))

if [ -n "$completion_missing" -a -n "$completion_exclude" ]; then
	completion_missing=$(echo "$completion_missing" | grep -v -E "$completion_exclude")
fi

if [ -n "$completion_missing" ]; then
	echo "Missing completion scripts:"
	echo "$completion_missing"
fi

