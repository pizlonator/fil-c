#!/bin/bash

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

VERBOSE='false'
while getopts vVh OPTIONS; do
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

# Try to find missing manuals matching build targets with manual files.
declare -A ADOC_LIST

declare -i COUNT_REPEATS=0
declare -a REPEATS
declare -A KNOWN_REPEATS
KNOWN_REPEATS[mount.8.adoc]='foo l2 l c overlay'
KNOWN_REPEATS[hexdump.1.adoc]='l'
KNOWN_REPEATS[flock.1.adoc]='"$0"'
KNOWN_REPEATS[switch_root.8.adoc]='$DIR'
KNOWN_REPEATS[logger.1.adoc]='-'
KNOWN_REPEATS[unshare.1.adoc]='1000 0 1'

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
	find . -type f -name '*[[:alpha:]].[1-8].adoc' |grep -v "autom4te.cache\|\.libs/\|\.git"
); do
	ADOC_FILE=${I##*/}
	ADOC_LIST[${ADOC_FILE%%.[0-9]}]=1

	REPEATS=( $(cat ${I} |
		col -b |
		sed  -e 's/\/\/\/\///g; s/\.\.\.\.//g;' |
		awk 'BEGIN { p="" } { if (0 < length($0)) { if (p == $0) { print } } p = $0 }') )
	if [ 0 -lt "${#REPEATS[@]}" ]; then
		ITER=${#REPEATS[@]}+1
		while ((ITER--)); do
			remove_repeats ${ITER}
		done
		if [ 0 -lt "${#REPEATS[@]}" ]; then
			echo "warning: ${I} has repeating words: ${REPEATS[@]}"
			echo "=================================================="
			((++COUNT_REPEATS))
		fi
	fi
done

exit 0
