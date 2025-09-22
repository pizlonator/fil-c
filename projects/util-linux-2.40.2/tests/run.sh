#!/bin/bash

#
# Copyright (C) 2007 Karel Zak <kzak@redhat.com>
#
# This file is part of util-linux.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

TS_TOPDIR=$(cd ${0%/*} && pwd)
SUBTESTS=
EXCLUDETESTS=
OPTS=
SYSCOMMANDS=

top_srcdir=
top_builddir=
paraller_jobs=1
has_asan_opt=
has_ubsan_opt=

function num_cpus()
{
	local num

	# coreutils
	if num=$(nproc 2>/dev/null); then
		:
	# BSD, OSX
	elif num=$(sysctl -n hw.ncpu 2>/dev/null); then
		:
	else
		num=$(grep -c "^processor" /proc/cpuinfo 2>/dev/null)
	fi

	# translate garbage output to "1"
	if test "$num" -gt "0" 2>/dev/null ;then
		echo "$num"
	else
		echo 1
	fi
}

function find_test_scripts()
{
	local searchdir="$1"
	find "$searchdir" -type f -regex ".*/[^\.~]*" \
		\( -perm -u=x -o -perm -g=x -o -perm -o=x \)
}

while [ -n "$1" ]; do
	case "$1" in
	--force |\
	--fake |\
	--memcheck-valgrind |\
	--nolocks |\
	--show-diff |\
	--verbose  |\
	--skip-loopdevs |\
	--noskip-commands |\
	--parsable)
		# these options are simply forwarded to the test scripts
		OPTS="$OPTS $1"
		;;
	--memcheck-asan)
		OPTS="$OPTS $1"
		has_asan_opt="yes"
		;;
	--memcheck-ubsan)
		OPTS="$OPTS $1"
		has_ubsan_opt="yes"
		;;
	--use-system-commands)
		OPTS="$OPTS $1"
		SYSCOMMANDS="yes"
		;;

	--nonroot)
		if [ $(id -ru) -eq 0 ]; then
			echo "Ignore util-linux test suite [non-root UID expected]."
			exit 0
		fi
		;;
	--srcdir=*)
		top_srcdir="${1##--srcdir=}"
		;;
	--builddir=*)
		top_builddir="${1##--builddir=}"
		;;
	--parallel=*)
		paraller_jobs="${1##--parallel=}"
		if ! [ "$paraller_jobs" -ge 0 ] 2>/dev/null; then
			echo "invalid argument '$paraller_jobs' for --parallel="
			exit 1
		fi
		;;
	--parallel)
		paraller_jobs=$(num_cpus)
		;;
	--parsable)
		OPTS="$OPTS $1"
		;;
	--exclude=*)
		EXCLUDETESTS="${1##--exclude=}"
		;;
	--*)
		echo "Unknown option $1"
		echo "Usage: "
		echo "  $(basename $0) [options] [<component> ...]"
		echo "Options:"
		echo "  --force               execute demanding tests"
		echo "  --fake                do not run, setup tests only"
		echo "  --memcheck-valgrind   run with valgrind"
		echo "  --memcheck-asan       enable ASAN (requires ./configure --enable-asan)"
		echo "  --nolocks             don't use flock to lock resources"
		echo "  --verbose             verbose mode"
		echo "  --show-diff           show diff from failed tests"
		echo "  --nonroot             ignore test suite if user is root"
		echo "  --use-system-commands use PATH rather than builddir"
		echo "  --noskip-commands     fail on missing commands"
		echo "  --srcdir=<path>       autotools top source directory"
		echo "  --builddir=<path>     autotools top build directory"
		echo "  --parallel=<num>      number of parallel test jobs, default: num cpus"
		echo "  --parsable            use parsable output (default on --parallel)"
		echo "  --exclude=<list>      exclude tests by list '<utilname>/<testname> ..'"
		echo
		exit 1
		;;

	*)
		SUBTESTS="$SUBTESTS $1"
		;;
	esac
	shift
done

# For compatibility with autotools is necessary to differentiate between source
# (with test scripts) and build (with temporary files) directories when
# executed by our build-system.
#
# The default is the source tree with this script.
#
if [ -z "$top_srcdir" ]; then
	top_srcdir="$TS_TOPDIR/.."
fi
if [ -z "$top_builddir" ]; then
	top_builddir="$TS_TOPDIR/.."
	if [ -e "$top_builddir/build/meson.conf" ]; then
		top_builddir="$TS_TOPDIR/../build"
	elif [ -e "$PWD/meson.conf" ]; then
		top_builddir="$PWD"
	fi
fi

OPTS="$OPTS --srcdir=$top_srcdir --builddir=$top_builddir"

# Auto-enable ASAN to avoid conflicts between tests and binaries
if [ -z "$has_asan_opt" ]; then
        if [ -e "$top_builddir/Makefile" ]; then
	    asan=$(awk '/^ASAN_LDFLAGS/ { print $3 }' $top_builddir/Makefile)
        elif [ -f "$top_builddir/meson.conf" ]; then
            . "$top_builddir/meson.conf"
        fi
	if [ -n "$asan" ]; then
		OPTS="$OPTS --memcheck-asan"
	fi
fi

if [ -z "$has_ubsan_opt" ]; then
	if [ -e "$top_builddir/Makefile" ]; then
		ubsan=$(awk '/^UBSAN_LDFLAGS/ { print $3 }' $top_builddir/Makefile)
	fi
	if [ -n "$ubsan" ]; then
		OPTS="$OPTS --memcheck-ubsan"
	fi
fi

declare -a comps
if [ -n "$SUBTESTS" ]; then
	# selected tests only
	for s in $SUBTESTS; do
		if [ -e "$top_srcdir/tests/ts/$s" ]; then
			comps+=( $(find_test_scripts "$top_srcdir/tests/ts/$s") ) || exit 1
		else
			echo "Unknown test component '$s'"
			exit 1
		fi
	done
else
	if [ -z "$SYSCOMMANDS" -a ! -f "$top_builddir/test_ttyutils" ]; then
		echo "Tests not compiled! Run 'make check-programs' to fix the problem."
		exit 1
	fi

	comps=( $(find_test_scripts "$top_srcdir/tests/ts") ) || exit 1
fi

if [ -n "$EXCLUDETESTS" ]; then
	declare -a xcomps		# temporary array
	for ts in ${comps[@]}; do
		tsname=${ts##*ts/}	# test name

		if [[ "$EXCLUDETESTS" == *${tsname}* ]]; then
			#echo "Ignore ${tsname}."
			true
		else
			xcomps+=($ts)
		fi
	done
	comps=("${xcomps[@]}")		# replace the array
fi

unset LIBMOUNT_DEBUG
unset LIBBLKID_DEBUG
unset LIBFDISK_DEBUG
unset LIBSMARTCOLS_DEBUG

echo
echo "-------------------- util-linux regression tests --------------------"
echo
echo "                    For development purpose only.                    "
echo "                 Don't execute on production system!                 "
echo

# TODO: add more information about system
printf "%13s: %-30s    " "kernel" "$(uname -r)"
echo
echo
echo "      options: $(echo $OPTS | sed 's/ / \\\n               /g')"
echo

if [ "$paraller_jobs" -ne 1 ]; then
	tmp=$paraller_jobs
	[ "$paraller_jobs" -eq 0 ] && tmp=infinite
	echo "              Executing the tests in parallel ($tmp jobs)    "
	echo
	OPTS="$OPTS --parallel"
fi

count=0
mkdir -p $top_builddir/tests/
>| $top_builddir/tests/failures
printf "%s\n" ${comps[*]} |
	sort |
	xargs -I '{}' -P $paraller_jobs -n 1 bash -c "'{}' \"$OPTS\" ||
		echo '{}' >> $top_builddir/tests/failures"
if [ $? != 0 ]; then
	echo "xargs error" >&2
	exit 1
fi

declare -a fail_file
fail_file=( $( < $top_builddir/tests/failures ) ) || exit 1
echo
echo "---------------------------------------------------------------------"
if [ ${#fail_file[@]} -eq 0 ]; then
	echo "  All ${#comps[@]} tests PASSED"
	res=0
else
	echo "  ${#fail_file[@]} tests of ${#comps[@]} FAILED"

	echo
	for ts in ${fail_file[@]}; do
		NAME=$(basename $ts)
		COMPONENT=$(basename $(dirname $ts))
		echo "      $COMPONENT/$NAME"
	done
	res=1
fi
echo "---------------------------------------------------------------------"

rm -f $top_builddir/tests/failures
exit $res
