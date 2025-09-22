#!/bin/bash
#
# Copyright (C) 2021 Karel Zak <kzak@redhat.com>
# based on checkman.sh from Sami Kerola <kerolasa@iki.fi>
#
set -e		# exit on errors
set -o pipefail	# exit if pipe writer fails
set -u		# disallow usage of unset variables
set -C		# disallow redirection file overwriting
SCRIPT_INVOCATION_SHORT_NAME=$(basename ${0})
trap 'echo "${SCRIPT_INVOCATION_SHORT_NAME}: exit on error"; exit 1' ERR

usage() {
	echo "Usage: ${0} [-vVh]"
	echo " -h  print this help and exit"
}

while getopts h OPTIONS; do
	case ${OPTIONS} in
		h)
			usage
			exit 0
			;;
		*)
			usage
			exit 1
	esac
done

declare -A ADOCS_LIST BIN_LIST

remove_repeats()
{
	set +u
	for KN in ${KNOWN_REPEATS[${I##*/}]}; do
		if [ "${KN}" = "${REPEATS[$1]}" ]; then
			if $VERBOSE; then
				echo "info: ${I} removing repeat: ${REPEATS[$1]}"
			fi
			unset REPEATS[$1]
		fi
	done
	set -u
}

cd $(git rev-parse --show-toplevel)

for I in $(
	find . -type f -name '*[[:alnum:]].[1-8].adoc' |grep -v "autom4te.cache\|\.libs/\|\.git"
); do
	ADOCS_FILE=${I##*/}
	ADOCS_LIST[${ADOCS_FILE%%.[0-9].adoc}]=1
done

# Create a list of build targets.
for I in $(find $(git rev-parse --show-toplevel) -name 'Make*.am' | xargs awk '
$1 ~ /_SOURCES/ {
	if ($1 ~ /^test/ ||
	    $1 ~ /^no(inst|dist)/ ||
	    $1 ~ /^sample/ ||
	    $1 ~ /^BUILT/) {
		next
	}
	sub(/_SOURCES/, "")
	if ($1 ~ /^lib.*_la/) {
		next
	}
	if ($1 ~ /^pylib.*_la/) {
		next
	}
	sub(/_static/, "")
	gsub(/_/, ".")
	sub(/switch.root/, "switch_root")
	sub(/pivot.root/, "pivot_root")
	print $1
}'); do
	BIN_LIST[$I]=1
done

# Find if build target does not have manual.
set +u
for I in ${!BIN_LIST[@]}; do
	if [ -v ${ADOCS_LIST[$I]} ]; then
		echo "warning: ${SCRIPT_INVOCATION_SHORT_NAME}: ${I} does not have man page"
	fi
done
set -u

exit 0
