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

TS_EXIT_NOTSUPP=2

function ts_abspath {
	cd $1
	pwd
}

function ts_canonicalize {
	P="$1"
	C=$(readlink -f $P)

	if [ -n "$C" ]; then
		echo "$C"
	else
		echo "$P"
	fi
}

function ts_cd {
	if [ $# -eq 0 ]; then
		ts_failed "ul_cd: not enough arguments"
	fi
	DEST=$(readlink -f "$1" 2>/dev/null)
	if [ "x$DEST" = "x" ] || [ ! -d "$DEST" ]; then
		ts_failed "ul_cd: $1: no such directory"
	fi
	cd "$DEST" 2>/dev/null || ts_failed "ul_cd: $1: cannot change directory"
	if [ "$PWD" != "$DEST" ]; then
		ts_failed "ul_cd: $PWD is not $DEST"
	fi
}

function ts_separator {
	local header="$1"
	echo >> $TS_OUTPUT
	if [ -z "$header" ]; then
		echo "============================================" >> $TS_OUTPUT
	else
		echo "=====$header================================" >> $TS_OUTPUT
	fi
}

function ts_report {
	local desc=

	if [ "$TS_PARSABLE" != "yes" ]; then
		if [ $TS_NSUBTESTS -ne 0 ] && [ -z "$TS_SUBNAME" ]; then
			desc=$(printf "%11s...")
		fi
		echo "$desc$1"
		return
	fi

	if [ -n "$TS_SUBNAME" ]; then
		desc=$(printf "%s: [%02d] %s" "$TS_DESC" "$TS_NSUBTESTS" "$TS_SUBNAME")
	else
		desc=$TS_DESC
	fi
	printf "%13s: %-45s ...%s\n" "$TS_COMPONENT" "$desc" "$1"
}

function ts_check_test_command {
	case "$1" in
	"")
		ts_failed "invalid test_command requested"
		;;
	*/*)
		# paths
		if [ ! -x "$1" ]; then
			ts_skip "${1##*/} not found"
		fi
		;;
	*)
		# just command names (e.g. --use-system-commands)
		local cmd=$1
		type "$cmd" >/dev/null 2>&1
	        if [ $? -ne 0 ]; then
			if [ "$TS_NOSKIP_COMMANDS" = "yes" ]; then
				ts_failed "missing in PATH: $cmd"
			fi
			ts_skip "missing in PATH: $cmd"
		fi
		;;
	esac
}

function ts_check_prog {
	local cmd=$1
	[ -z "$cmd" ] && ts_failed "invalid prog requested"
	type "$cmd" >/dev/null 2>&1 || ts_skip "missing in PATH: $cmd"
}

function ts_check_losetup {
	local tmp
	ts_check_test_command "$TS_CMD_LOSETUP"

	if [ "$TS_SKIP_LOOPDEVS" = "yes" ]; then
		ts_skip "loop-device tests disabled"
	fi

	# assuming that losetup -f works ... to be checked somewhere else
	tmp=$($TS_CMD_LOSETUP -f 2>/dev/null)
	if test -b "$tmp"; then
		return 0
	fi
	ts_skip "no loop-device support"
}

function ts_check_wcsspn {
	# https://gitlab.com/qemu-project/qemu/-/issues/1248
	if [ -e "$TS_HELPER_SYSINFO" ] &&
		[ "$("$TS_HELPER_SYSINFO" wcsspn-ok)" = "0" ]; then

		ts_skip "non-functional widestring functions"
	fi
}

function ts_check_native_byteorder {
	if [ "$QEMU_USER" == "1" ] && [ ! -e /sys/kernel/cpu_byteorder ]; then
		ts_skip "non-native byteorder"
	fi
}

function ts_check_enotty {
	# https://lore.kernel.org/qemu-devel/20230426070659.80649-1-thomas@t-8ch.de/
	if [ -e "$TS_HELPER_SYSINFO" ] &&
		[ "$("$TS_HELPER_SYSINFO" enotty-ok)" = "0" ]; then

		ts_skip "broken ENOTTY return"
	fi
}

function ts_report_skip {
	ts_report " SKIPPED ($1)"
}

function ts_skip {
	ts_report_skip "$1"

	ts_cleanup_on_exit
	exit 0
}

function ts_skip_nonroot {
	if [ $UID -ne 0 ]; then
		ts_skip "no root permissions"
	fi
}

# Specify the capability needed in your test case like:
#
#	ts_skip_capability cap_wake_alarm
#
function ts_skip_capability {
	ts_check_prog "$TS_HELPER_CAP"

	if ! "$TS_HELPER_CAP" "$1"; then
		ts_skip "no capability: $1"
	fi
}

function ts_skip_qemu_user {
	if [ "$QEMU_USER" == "1" ]; then
		ts_skip "running under qemu-user emulation"
	fi
}

function ts_failed_subtest {
	local msg="FAILED"
	local ret=1
	if [ "$TS_KNOWN_FAIL" = "yes" ]; then
		msg="KNOWN FAILED"
		ret=0
	fi

	if [ x"$1" == x"" ]; then
		ts_report " $msg ($TS_NS)"
	else
		ts_report " $msg ($1)"
	fi

	return $ret
}

function ts_failed {
	ts_failed_subtest "$1"
	exit $?
}

function ts_report_ok {
	if [ x"$1" == x"" ]; then
		ts_report " OK"
	else
		ts_report " OK ($1)"
	fi
}

function ts_ok {
	ts_report_ok "$1"
	exit 0
}

function ts_log {
	echo "$1" >> $TS_OUTPUT
	[ "$TS_VERBOSE" == "yes" ] && echo "$1"
}

function ts_logerr {
	echo "$1" >> $TS_ERRLOG
	[ "$TS_VERBOSE" == "yes" ] && echo "$1"
}

function ts_log_both {
	echo "$1" >> $TS_OUTPUT
	echo "$1" >> $TS_ERRLOG
	[ "$TS_VERBOSE" == "yes" ] && echo "$1"
}

function ts_has_option {
	NAME="$1"
	ALL="$2"

	# user may set options by env for a single test or whole component
	# e.g. TS_OPT_ipcs_limits2_fake="yes" or TS_OPT_ipcs_fake="yes"
	local v_test=${TS_TESTNAME//[-.]/_}
	local v_comp=${TS_COMPONENT//[-.]/_}
	local v_name=${NAME//[-.]/_}
	eval local env_opt_test=\$TS_OPT_${v_comp}_${v_test}_${v_name}
	eval local env_opt_comp=\$TS_OPT_${v_comp}_${v_name}
	if [ "$env_opt_test" = "yes" \
		-o "$env_opt_comp" = "yes" -a "$env_opt_test" != "no" ]; then
		echo "yes"
		return
	elif [ "$env_opt_test" = "no" \
		-o "$env_opt_comp" = "no" -a "$env_opt_test" != "yes" ]; then
		return
	fi

	# or just check the global command line options
	if [[ $ALL =~ ([$' \t\n']|^)--$NAME([$'= \t\n']|$) ]]; then
		echo yes
		return
	fi

	# or the _global_ env, e.g TS_OPT_parsable="yes"
	eval local env_opt=\$TS_OPT_${v_name}
	if [ "$env_opt" = "yes" ]; then echo "yes"; fi
}

function ts_option_argument {
	NAME="$1"
	ALL="$2"

	# last option wins!
	echo "$ALL" | sed -n "s/.*[ \t\n]--$NAME=\([^ \t\n]*\).*/\1/p" | tail -n 1
}

function ts_init_core_env {
	TS_SUBNAME=""
	TS_NS="$TS_COMPONENT/$TS_TESTNAME"
	TS_OUTPUT="$TS_OUTDIR/$TS_TESTNAME"
	TS_ERRLOG="$TS_OUTDIR/$TS_TESTNAME.err"
	TS_VGDUMP="$TS_OUTDIR/$TS_TESTNAME.vgdump"
	TS_EXIT_CODE="$TS_OUTDIR/$TS_TESTNAME.exit_code"
	TS_DIFF="$TS_DIFFDIR/$TS_TESTNAME"
	TS_EXPECTED="$TS_TOPDIR/expected/$TS_NS"
	TS_EXPECTED_ERR="$TS_TOPDIR/expected/$TS_NS.err"
	TS_MOUNTPOINT="$TS_OUTDIR/${TS_TESTNAME}-mnt"
}

function ts_init_core_subtest_env {
	TS_NS="$TS_COMPONENT/$TS_TESTNAME-$TS_SUBNAME"
	TS_OUTPUT="$TS_OUTDIR/$TS_TESTNAME-$TS_SUBNAME"
	TS_ERRLOG="$TS_OUTDIR/$TS_TESTNAME-$TS_SUBNAME.err"
	TS_VGDUMP="$TS_OUTDIR/$TS_TESTNAME-$TS_SUBNAME.vgdump"
	TS_EXIT_CODE="$TS_OUTDIR/$TS_TESTNAME-$TS_SUBNAME.exit_code"
	TS_DIFF="$TS_DIFFDIR/$TS_TESTNAME-$TS_SUBNAME"
	TS_EXPECTED="$TS_TOPDIR/expected/$TS_NS"
	TS_EXPECTED_ERR="$TS_TOPDIR/expected/$TS_NS.err"
	TS_MOUNTPOINT="$TS_OUTDIR/${TS_TESTNAME}-${TS_SUBNAME}-mnt"

	rm -f "$TS_OUTPUT" "$TS_ERRLOG" "$TS_VGDUMP" "$TS_EXIT_CODE"
	[ -d "$TS_OUTDIR" ]  || mkdir -p "$TS_OUTDIR"

	touch "$TS_OUTPUT" "$TS_ERRLOG" "$TS_EXIT_CODE"
	[ -n "$TS_VALGRIND_CMD" ] && touch $TS_VGDUMP
}

function ts_init_env {
	local mydir=$(ts_abspath ${0%/*})
	local tmp

	LANG="POSIX"
	LANGUAGE="POSIX"
	LC_ALL="POSIX"
	CHARSET="UTF-8"
	ASAN_OPTIONS="detect_leaks=0"
	UBSAN_OPTIONS="print_stacktrace=1:print_summary=1:halt_on_error=1"

	export LANG LANGUAGE LC_ALL CHARSET ASAN_OPTIONS UBSAN_OPTIONS

	mydir=$(ts_canonicalize "$mydir")

	# automake directories
	top_srcdir=$(ts_option_argument "srcdir" "$*")
	top_builddir=$(ts_option_argument "builddir" "$*")

	# where is this script
	TS_TOPDIR=$(ts_abspath $mydir/../../)

	# default
	if [ -z "$top_srcdir" ]; then
		top_srcdir="$TS_TOPDIR/.."
	fi
	if [ -z "$top_builddir" ]; then
		top_builddir="$TS_TOPDIR/.."
	fi

	top_srcdir=$(ts_abspath $top_srcdir)
	top_builddir=$(ts_abspath $top_builddir)

        if [ -e "$top_builddir/meson.conf" ]; then
            . "$top_builddir/meson.conf"
        fi

	# We use helpser always from build tree
	ts_helpersdir="${top_builddir}/"

	TS_USE_SYSTEM_COMMANDS=$(ts_has_option "use-system-commands" "$*")
	if [ "$TS_USE_SYSTEM_COMMANDS" == "yes" ]; then
		# Don't define anything, just follow current PATH
		ts_commandsdir=""
	else
		# The default is to use commands from build tree
		ts_commandsdir="${top_builddir}/"

		# some ul commands search other ul commands in $PATH
		export PATH="$ts_commandsdir:$PATH"
	fi

	TS_SCRIPT="$mydir/$(basename $0)"
	TS_SUBDIR=$(dirname $TS_SCRIPT)
	TS_TESTNAME=$(basename $TS_SCRIPT)
	TS_COMPONENT=$(basename $TS_SUBDIR)
	TS_DESC=${TS_DESC:-$TS_TESTNAME}

	TS_NSUBTESTS=0
	TS_NSUBFAILED=0

	TS_SELF="$TS_SUBDIR"

	TS_OUTDIR="$top_builddir/tests/output/$TS_COMPONENT"
	TS_DIFFDIR="$top_builddir/tests/diff/$TS_COMPONENT"

	TS_NOLOCKS=$(ts_has_option "nolocks" "$*")
	TS_LOCKDIR="$top_builddir/tests/output"

	# Don't lock if flock(1) is missing
	type "flock" >/dev/null 2>&1 || TS_NOLOCKS="yes"

	ts_init_core_env

	TS_NOSKIP_COMMANDS=$(ts_has_option "noskip-commands" "$*")
	TS_VERBOSE=$(ts_has_option "verbose" "$*")
	TS_SHOWDIFF=$(ts_has_option "show-diff" "$*")
	TS_PARALLEL=$(ts_has_option "parallel" "$*")
	TS_KNOWN_FAIL=$(ts_has_option "known-fail" "$*")
	TS_SKIP_LOOPDEVS=$(ts_has_option "skip-loopdevs" "$*")
	TS_PARSABLE=$(ts_has_option "parsable" "$*")
	[ "$TS_PARSABLE" = "yes" ] || TS_PARSABLE="$TS_PARALLEL"

	tmp=$( ts_has_option "memcheck-valgrind" "$*")
	if [ "$tmp" == "yes" -a -f /usr/bin/valgrind ]; then
		TS_VALGRIND_CMD="/usr/bin/valgrind"
	fi
	tmp=$( ts_has_option "memcheck-asan" "$*")
	if [ "$tmp" == "yes" ]; then
		TS_ENABLE_ASAN="yes"
	fi
	tmp=$( ts_has_option "memcheck-ubsan" "$*")
	if [ "$tmp" == "yes" ]; then
		TS_ENABLE_UBSAN="yes"
	fi

	TS_FSTAB="$TS_OUTDIR/${TS_TESTNAME}.fstab"
	BLKID_FILE="$TS_OUTDIR/${TS_TESTNAME}.blkidtab"

	declare -a TS_SUID_PROGS
	declare -a TS_SUID_USER
	declare -a TS_SUID_GROUP
	declare -a TS_LOOP_DEVS
	declare -a TS_LOCKFILE_FD

	if [ -f $TS_TOPDIR/commands.sh ]; then
		. $TS_TOPDIR/commands.sh
	fi

	export BLKID_FILE

	rm -f $TS_OUTPUT $TS_ERRLOG $TS_VGDUMP $TS_EXIT_CODE
	[ -d "$TS_OUTDIR" ]  || mkdir -p "$TS_OUTDIR"

	touch $TS_OUTPUT $TS_ERRLOG $TS_EXIT_CODE
	[ -n "$TS_VALGRIND_CMD" ] && touch $TS_VGDUMP

	if [ "$TS_VERBOSE" == "yes" ]; then
		echo
		echo "     script: $TS_SCRIPT"
		echo "   commands: $ts_commandsdir"
		echo "    helpers: $ts_helpersdir"
		echo "    sub dir: $TS_SUBDIR"
		echo "    top dir: $TS_TOPDIR"
		echo "       self: $TS_SELF"
		echo "  test name: $TS_TESTNAME"
		echo "  test desc: $TS_DESC"
		echo "  component: $TS_COMPONENT"
		echo "  namespace: $TS_NS"
		echo "    verbose: $TS_VERBOSE"
		echo "     output: $TS_OUTPUT"
		echo "  error log: $TS_ERRLOG"
		echo "  exit code: $TS_EXIT_CODE"
		echo "   valgrind: $TS_VGDUMP"
		echo "   expected: $TS_EXPECTED{.err}"
		echo " mountpoint: $TS_MOUNTPOINT"
		echo
	fi
}

function ts_init_subtest {

	TS_SUBNAME="$1"
	ts_init_core_subtest_env
	TS_NSUBTESTS=$(( $TS_NSUBTESTS + 1 ))

	if [ "$TS_PARSABLE" != "yes" ]; then
		[ $TS_NSUBTESTS -eq 1 ] && echo
		printf "%16s: %-27s ..." "" "$TS_SUBNAME"
	fi
}

function ts_init {
	ts_init_env "$*"

	local is_fake=$( ts_has_option "fake" "$*")
	local is_force=$( ts_has_option "force" "$*")

	if [ "$TS_PARSABLE" != "yes" ]; then
		printf "%13s: %-30s ..." "$TS_COMPONENT" "$TS_DESC"
	fi

	[ "$is_fake" == "yes" ] && ts_skip "fake mode"
	[ "$TS_OPTIONAL" == "yes" -a "$is_force" != "yes" ] && ts_skip "optional"
}

function ts_init_suid {
	PROG="$1"
	ct=${#TS_SUID_PROGS[*]}

	# Save info about original setting
	TS_SUID_PROGS[$ct]=$PROG
	TS_SUID_USER[$ct]=$(stat --printf="%U" $PROG)
	TS_SUID_GROUP[$ct]=$(stat --printf="%G" $PROG)

	chown root:root $PROG &> /dev/null
	chmod u+s $PROG &> /dev/null
}

function ts_init_py {
	LIBNAME="$1"

	if [ -f "$top_builddir/py${LIBNAME}.la" ]; then
            # autotoolz build
	    export LD_LIBRARY_PATH="$top_builddir/.libs:$LD_LIBRARY_PATH"
	    export PYTHONPATH="$top_builddir/$LIBNAME/python:$top_builddir/.libs:$PYTHONPATH"

	    PYTHON_VERSION=$(awk '/^PYTHON_VERSION/ { print $3 }' $top_builddir/Makefile)
	    PYTHON_MAJOR_VERSION=$(echo $PYTHON_VERSION | sed 's/\..*//')

	    export PYTHON="python${PYTHON_MAJOR_VERSION}"

        elif compgen -G "$top_builddir/$LIBNAME/python/py$LIBNAME*.so" >/dev/null; then
            # mezon!
            export PYTHONPATH="$top_builddir/$LIBNAME/python:$PYTHONPATH"

        else
            ts_skip "py${LIBNAME} not compiled"
        fi
}

function ts_run {
	declare -a args
	local asan_options="strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1"

	#
	# ASAN mode
	#
	if [ "$TS_ENABLE_ASAN" == "yes" -o "$TS_ENABLE_UBSAN" == "yes" ]; then
		args+=(env)
		if [ "$TS_ENABLE_ASAN" == "yes" ]; then
			# detect_leaks isn't supported on s390x: https://github.com/llvm/llvm-project/blob/master/compiler-rt/lib/lsan/lsan_common.h
			if [ "$(uname -m)" != "s390x" ]; then
				asan_options="$asan_options:detect_leaks=1"
			fi
			args+=(ASAN_OPTIONS=$asan_options)
		fi
		if [ "$TS_ENABLE_UBSAN" == "yes" ]; then
			args+=(UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1)
		fi
	fi

	#
	# valgrind mode
	#
	if [ -n "$TS_VALGRIND_CMD" ]; then
		args+=(libtool --mode=execute "$TS_VALGRIND_CMD" --tool=memcheck --leak-check=full)
		args+=(--leak-resolution=high --num-callers=20 --log-file="$TS_VGDUMP")
	fi

	"${args[@]}" "$@"
	echo $? >$TS_EXIT_CODE
}

function ts_gen_diff_from {
	local res=0
	local expected="$1"
	local output="$2"
	local difffile="$3"

	diff -u "$expected" "$output" > "$difffile"

	if [ $? -ne 0 ] || [ -s "$difffile" ]; then
		res=1
		if [ "$TS_SHOWDIFF" == "yes" -a "$TS_KNOWN_FAIL" != "yes" ]; then
			echo
			echo "diff-{{{"
			cat "$difffile"
			echo "}}}-diff"
			echo
		fi
	else
		rm -f "$difffile";
	fi

	return $res
}

function ts_gen_diff {
	local status_out=0
	local status_err=0
	local exit_code=0

	[ -f "$TS_OUTPUT" ] || return 1
	[ -f "$TS_EXPECTED" ] || TS_EXPECTED=/dev/null

	# remove libtool lt- prefixes
	sed --in-place 's/^lt\-\(.*\: \)/\1/g' "$TS_OUTPUT"
	sed --in-place 's/^lt\-\(.*\: \)/\1/g' "$TS_ERRLOG"

	[ -d "$TS_DIFFDIR" ] || mkdir -p "$TS_DIFFDIR"

	# error log is fully optional
	[ -f "$TS_EXPECTED_ERR" ] || TS_EXPECTED_ERR=/dev/null
	[ -f "$TS_ERRLOG" ] || TS_ERRLOG=/dev/null

	if [ "$TS_COMPONENT" != "fuzzers" ]; then
		ts_gen_diff_from "$TS_EXPECTED" "$TS_OUTPUT" "$TS_DIFF"
		status_out=$?

		ts_gen_diff_from "$TS_EXPECTED_ERR" "$TS_ERRLOG" "$TS_DIFF.err"
		status_err=$?
	else
		# TS_EXIT_CODE is empty when tests aren't run with ts_run: https://github.com/util-linux/util-linux/issues/1072
		# or when ts_finalize is called right after ts_finalize_subtest.
		exit_code="$(cat $TS_EXIT_CODE)"
		if [ -z "$exit_code" ]; then
			exit_code=0
		fi

		if [ $exit_code -ne 0 ]; then
			ts_gen_diff_from "$TS_EXPECTED" "$TS_OUTPUT" "$TS_DIFF"
			ts_gen_diff_from "$TS_EXPECTED_ERR" "$TS_ERRLOG" "$TS_DIFF.err"
		fi
	fi

	if [ $status_out -ne 0 -o $status_err -ne 0 -o $exit_code -ne 0 ]; then
		return 1
	fi
	return 0
}

function tt_gen_mem_report {
	if [ -n "$TS_VALGRIND_CMD" ]; then
		grep -q -E 'ERROR SUMMARY: [1-9]' $TS_VGDUMP &> /dev/null
		if [ $? -eq 0 ]; then
			echo "mem-error detected!"
		fi
	else
		echo "$1"
	fi
}

function ts_finalize_subtest {
	local res=0

	ts_gen_diff
	if [ $? -eq 1 ]; then
		ts_failed_subtest "$1"
		res=1
	else
		ts_report_ok "$(tt_gen_mem_report "$1")"
	fi

	[ $res -ne 0 ] && TS_NSUBFAILED=$(( $TS_NSUBFAILED + 1 ))

	# reset environment back to parental test
	ts_init_core_env

	return $res
}

function ts_skip_subtest {
	ts_report_skip "$1"
	# reset environment back to parental test
	ts_init_core_env

}

function ts_finalize {
	ts_cleanup_on_exit

	if [ $TS_NSUBTESTS -ne 0 ]; then
		if ! ts_gen_diff || [ $TS_NSUBFAILED -ne 0 ]; then
			ts_failed "$TS_NSUBFAILED from $TS_NSUBTESTS sub-tests"
		else
			ts_ok "all $TS_NSUBTESTS sub-tests PASSED"
		fi
	fi

	ts_gen_diff || ts_failed "$1"
	ts_ok "$1"
}

function ts_die {
	ts_log "$1"
	ts_finalize
}

function ts_cleanup_on_exit {

	for idx in $(seq 0 $((${#TS_SUID_PROGS[*]} - 1))); do
		PROG=${TS_SUID_PROGS[$idx]}
		chmod a-s $PROG &> /dev/null
		chown ${TS_SUID_USER[$idx]}:${TS_SUID_GROUP[$idx]} $PROG &> /dev/null
	done

	for dev in "${TS_LOOP_DEVS[@]}"; do
		ts_device_deinit "$dev"
	done
	unset TS_LOOP_DEVS

	ts_scsi_debug_rmmod
}

function ts_image_md5sum {
	local img=${1:-"$TS_OUTDIR/${TS_TESTNAME}.img"}
	echo $("$TS_HELPER_MD5" < "$img") $(basename "$img")
}

function ts_image_init {
	local mib=${1:-"5"}	# size in MiBs
	local img=${2:-"$TS_OUTDIR/${TS_TESTNAME}.img"}

	rm -f $img
	truncate -s "${mib}M" "$img"
	echo "$img"
	return 0
}

function ts_register_loop_device {
	local ct=${#TS_LOOP_DEVS[*]}
	TS_LOOP_DEVS[$ct]=$1
}

function ts_device_init {
	local img
	local dev

	img=$(ts_image_init $1 $2)
	dev=$($TS_CMD_LOSETUP --show --partscan -f "$img")
	if [ "$?" != "0" -o "$dev" = "" ]; then
		ts_die "Cannot init device"
	fi

	ts_register_loop_device "$dev"
	TS_LODEV=$dev
}

# call from ts_cleanup_on_exit() only because of TS_LOOP_DEVS maintenance
function ts_device_deinit {
	local DEV="$1"

	if [ -b "$DEV" ]; then
		$TS_CMD_UMOUNT "$DEV" &> /dev/null
		$TS_CMD_LOSETUP -d "$DEV" &> /dev/null
	fi
}

function ts_blkidtag_by_devname()
{
	local tag=$1
	local dev=$2
	local out
	local rval

	out=$($TS_CMD_BLKID -p -s "$tag" -o value "$dev")
	rval=$?
	printf "%s\n" "$out"

	test -n "$out" -a "$rval" = "0"
	return $?
}

function ts_uuid_by_devname {
	ts_blkidtag_by_devname "UUID" "$1"
	return $?
}

function ts_label_by_devname {
	ts_blkidtag_by_devname "LABEL" "$1"
	return $?
}

function ts_fstype_by_devname {
	ts_blkidtag_by_devname "TYPE" "$1"
	return $?
}

function ts_vfs_dump {
	if [ "$TS_SHOWDIFF" == "yes" -a "$TS_KNOWN_FAIL" != "yes" ]; then
		echo
		echo "{{{{ VFS dump:"
		findmnt
		echo "}}}}"
	fi
}

function ts_blk_dump {
	if [ "$TS_SHOWDIFF" == "yes" -a "$TS_KNOWN_FAIL" != "yes" ]; then
		echo
		echo "{{{{ blkdevs dump:"
		lsblk -o+FSTYPE
		echo "}}}}"
	fi
}

function ts_device_has {
	local TAG="$1"
	local VAL="$2"
	local DEV="$3"
	local vl=""
	local res=""

	vl=$(ts_blkidtag_by_devname "$TAG" "$DEV")
	test $? = 0 -a "$vl" = "$VAL"
	res=$?

	if [ "$res" != 0 ]; then
		ts_vfs_dump
		ts_blk_dump
	fi

	return $res
}

function ts_is_uuid()
{
	printf "%s\n" "$1" | grep -E -q '^[0-9a-z]{8}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{12}$'
	return $?
}

function ts_udevadm_settle()
{
	local dev=$1 # optional, might be empty
	shift        # all other args are tags, LABEL, UUID, ...
	udevadm settle
}

function ts_mount {
	local out
	local result
	local msg
	local fs
	local fs_exp=$1
	shift

	out=$($TS_CMD_MOUNT "$@" 2>&1)
	result=$?
	echo -n "$out" >> $TS_OUTPUT

	if [ $result != 0 ] \
		&& msg=$(echo "$out" | grep -m1 "unknown filesystem type")
	then
		# skip only if reported fs correctly and if it's not available
		fs=$(echo "$msg" | sed -n "s/.*type '\(.*\)'$/\1/p")
		[ "$fs" = "fs_exp" ] \
		 && grep -qe "[[:space:]]${fs}$" /proc/filesystems &>/dev/null \
		 || ts_skip "$msg"
	fi
	return $result
}

function ts_is_mounted {
	local DEV=$(ts_canonicalize "$1")

	grep -q "\(^\| \)$DEV " /proc/mounts && return 0

	if [ "${DEV#/dev/loop/}" != "$DEV" ]; then
		grep -q "^/dev/loop${DEV#/dev/loop/} " /proc/mounts && return 0
	fi
	return 1
}

function ts_fstab_open {
	echo "# <!-- util-linux test entry" >> "$TS_FSTAB"
}

function ts_fstab_close {
	echo "# -->" >> "$TS_FSTAB"
	sync "$TS_FSTAB" 2>/dev/null
}

function ts_fstab_addline {
	local SPEC="$1"
	local MNT=${2:-"$TS_MOUNTPOINT"}
	local FS=${3:-"auto"}
	local OPT=${4:-"defaults"}

	echo "$SPEC   $MNT   $FS   $OPT   0   0" >> "$TS_FSTAB"
}

function ts_fstab_lock {
	ts_lock "fstab"
}

function ts_fstab_add {
	ts_fstab_lock
	ts_fstab_open
	ts_fstab_addline $*
	ts_fstab_close
}

function ts_fstab_clean {
	ts_have_lock "fstab" || return 0
	sed --in-place "
/# <!-- util-linux/!b
:a
/# -->/!{
  N
  ba
}
s/# <!-- util-linux.*-->//;
/^$/d" "$TS_FSTAB"

	sync "$TS_FSTAB" 2>/dev/null
	ts_unlock "fstab"
}

function ts_fdisk_clean {
	local DEVNAME=$1

	# remove non comparable parts of fdisk output
	if [ -n "${DEVNAME}" ]; then
		# escape "@" with "\@" in $DEVNAME. This way sed correctly
		# replaces paths containing "@" characters
		sed -i -e "s@${DEVNAME//\@/\\\@}@<removed>@;" $TS_OUTPUT $TS_ERRLOG
	fi

	sed -i \
		-e 's/Disk identifier:.*/Disk identifier: <removed>/' \
		-e 's/Created a new partition.*/Created a new partition <removed>./' \
		-e 's/Created a new .* disklabel .*/Created a new disklabel./' \
		-e 's/^Device[[:blank:]]*Start/Device             Start/' \
		-e 's/^Device[[:blank:]]*Boot/Device     Boot/' \
		-e 's/Welcome to fdisk.*/Welcome to fdisk <removed>./' \
		-e 's/typescript file.*/typescript file <removed>./' \
		-e 's@^\(I/O size (minimum/op.* bytes /\) [1-9][0-9]* @\1 <removed> @' \
		$TS_OUTPUT $TS_ERRLOG
}


# https://stackoverflow.com/questions/41603787/how-to-find-next-available-file-descriptor-in-bash
function ts_find_free_fd()
{
	local rco
	local rci
	for fd in {3..200}; do
		rco="$(true 2>/dev/null >&${fd}; echo $?)"
		rci="$(true 2>/dev/null <&${fd}; echo $?)"
		if [[ "${rco}${rci}" = "11" ]]; then
			echo "$fd"
			return 0
		fi
	done
	return 1
}

function ts_get_lock_fd {
	local resource=$1
	local fd

	for fd in "${!TS_LOCKFILE_FD[@]}"; do
		if [ "${TS_LOCKFILE_FD["$fd"]}" = "$resource" ]; then
			echo "$fd"
			return 0
		fi
	done
	return 1
}

function ts_have_lock {
	local resource=$1

	test "$TS_NOLOCKS" = "yes" && return 0
	ts_get_lock_fd "$resource" >/dev/null && return 0
	return 1
}

function ts_lock {
	local resource="$1"
	local lockfile="${TS_LOCKDIR}/${resource}.lock"
	local fd

	if [ "$TS_NOLOCKS" == "yes" ]; then
		return 0
	fi

	# Don't lock again
	fd=$(ts_get_lock_fd "$resource")
	if [ -n "$fd" ]; then
		echo "[$$ $TS_TESTNAME] ${resource} already locked!"
		return 0
	fi

	fd=$(ts_find_free_fd) || ts_skip "failed to find lock fd"

	eval "exec $fd>$lockfile"
	flock --exclusive "$fd" || ts_skip "failed to lock $resource"

	TS_LOCKFILE_FD["$fd"]="$resource"
	###echo "[$$ $TS_TESTNAME] Locked   $resource"
}

function ts_unlock {
	local resource="$1"
	local lockfile="${TS_LOCKDIR}/${resource}.lock"
	local fd

	if [ "$TS_NOLOCKS" == "yes" ]; then
		return 0
	fi

	fd=$(ts_get_lock_fd "$resource")
	if [ -n "$fd" ]; then
		eval "exec $fd<&-"
		TS_LOCKFILE_FD["$fd"]=""
		###echo "[$$ $TS_TESTNAME] Unlocked $resource"
	else
		echo "[$$ $TS_TESTNAME] unlocking unlocked $resource!?"
	fi
}

function ts_scsi_debug_init {
	local devname
	local t
	TS_DEVICE="none"

	ts_lock "scsi_debug"

	# dry run is not really reliable, real modprobe may still fail
	modprobe --dry-run --quiet scsi_debug &>/dev/null \
		|| ts_skip "missing scsi_debug module (dry-run)"

	# skip if still in use or removal of modules not supported at all
	# We don't want a slow timeout here so we don't use ts_scsi_debug_rmmod!
	modprobe -r scsi_debug &>/dev/null
	if [ "$?" -eq 1 ]; then
		ts_unlock "scsi_debug"
		ts_skip "cannot remove scsi_debug module (rmmod)"
	fi

	modprobe -b scsi_debug "$@" &>/dev/null \
		|| ts_skip "cannot load scsi_debug module (modprobe)"

	# it might be still not loaded, modprobe.conf or whatever
	lsmod 2>/dev/null | grep -q "^scsi_debug " \
		|| ts_skip "scsi_debug module not loaded (lsmod)"

	udevadm settle

	# wait for device if udevadm settle does not work
	for t in 0 0.02 0.05 0.1 1; do
		sleep $t
		devname=$(grep --no-messages --with-filename scsi_debug /sys/block/*/device/model) && break
	done
	[ -n "${devname}" ] || ts_skip "timeout waiting for scsi_debug device"

	devname=$(echo $devname | awk -F '/' '{print $4}')
	TS_DEVICE="/dev/${devname}"

	# TODO validate that device is really up, for now just a warning on stderr
	test -b $TS_DEVICE || echo "warning: scsi_debug device is still down" >&2
}

# automatically called once in ts_cleanup_on_exit()
function ts_scsi_debug_rmmod {
	local err=1
	local t
	local lastmsg

	# We must not run if we don't have the lock
	ts_have_lock "scsi_debug" || return 0

	# Return early most importantly in case we are not root or the module does
	# not exist at all.
	[ $UID -eq 0 ] || return 0
	[ -n "$TS_DEVICE" ] || return 0
	lsmod 2>/dev/null | grep -q "^scsi_debug " || return 0

	udevadm settle

	# wait for successful rmmod if udevadm settle does not work
	for t in 0 0.02 0.05 0.1 1; do
		sleep $t
		lastmsg="$(modprobe -r scsi_debug 2>&1)" && err=0 && break
	done

	if [ "$err" = "1" ]; then
		ts_log "rmmod failed: '$lastmsg'"
		ts_log "timeout removing scsi_debug module (rmmod)"
		return 1
	fi
	if lsmod | grep -q "^scsi_debug "; then
		ts_log "BUG! scsi_debug still loaded"
		return 1
	fi

	# TODO Do we need to validate that all devices are gone?
	udevadm settle
	test -b "$TS_DEVICE" && echo "warning: scsi_debug device is still up" >&2

	# TODO unset TS_DEVICE, check that nobody uses it later, e.g. ts_fdisk_clean

	ts_unlock "scsi_debug"
	return 0
}

function ts_resolve_host {
	local host="$1"
	local tmp

	# currently we just resolve default records (might be "A", ipv4 only)
	if type "dig" >/dev/null 2>&1; then
		tmp=$(dig "$host" +short 2>/dev/null) || return 1
	elif type "nslookup" >/dev/null 2>&1; then
		tmp=$(nslookup "$host" 2>/dev/null) || return 1
		tmp=$(echo "$tmp"| grep -A1 "^Name:"| grep "^Address:"| cut -d" " -f2)
	elif type "host" >/dev/null 2>&1; then
		tmp=$(host "$host" 2>/dev/null) || return 1
		tmp=$(echo "$tmp" | grep " has address " | cut -d " " -f4)
	elif type "getent" >/dev/null 2>&1; then
		tmp=$(getent ahosts "$host" 2>/dev/null) || return 1
		tmp=$(echo "$tmp" | cut -d " " -f 1 | sort -u)
	fi

	# we return 1 if tmp is empty
	test -n "$tmp" || return 1
	echo "$tmp" | sort -R | head -n 1
}

# listen to unix socket (background socat)
function ts_init_socket_to_file {
	local socket=$1
	local outfile=$2
	local pid="0"

	ts_check_prog "socat"
	rm -f "$socket" "$outfile"

	# if socat is too old for these options we'll skip it below
	socat -u UNIX-LISTEN:$socket,fork,max-children=1,backlog=128 \
		STDOUT > "$outfile" 2>/dev/null &
	pid=$!

	# check for running background process
	if [ "$pid" -le "0" ] || ! kill -s 0 "$pid" &>/dev/null; then
		ts_skip "unable to run socat"
	fi
	# wait for the socket listener
	if ! socat -u /dev/null UNIX-CONNECT:$socket,retry=30,interval=0.1 &>/dev/null; then
		kill -9 "$pid" &>/dev/null
		ts_skip "timeout waiting for socat socket"
	fi
	# check socket again
	if ! socat -u /dev/null UNIX-CONNECT:$socket &>/dev/null; then
		kill -9 "$pid" &>/dev/null
		ts_skip "socat socket stopped listening"
	fi
}

function ts_has_ncurses_support {
	grep -q '#define HAVE_LIBNCURSES' ${top_builddir}/config.h
	if [ $? == 0 ]; then
		echo "yes"
	else
		echo "no"
	fi
}

# Get path to the ASan runtime DSO the given binary was compiled with
function ts_get_asan_rt_path {
	local binary="${1?}"
	local rt_path

	ts_check_prog "ldd"
	ts_check_prog "awk"

	rt_path="$(ldd "$binary" | awk '/lib.+asan.*.so/ {print $3; exit}')"
	if [ -n "$rt_path" -a -f "$rt_path" ]; then
		echo "$rt_path"
	fi
}

function ts_skip_exitcode_not_supported {
	if [ $? -eq $TS_EXIT_NOTSUPP ]; then
		ts_skip "functionality not implemented by system"
	fi
}

function ts_inhibit_custom_colorscheme {
	export XDG_CONFIG_HOME=/dev/null
}

function ts_is_virt {
	type "systemd-detect-virt" >/dev/null 2>&1
	if [ $? -ne 0 ]; then
		return 1
	fi

	virt="$(systemd-detect-virt)"
	for arg in "$@"; do
		if [ "$virt" = "$arg" ]; then
			return 0;
		fi
	done
	return 1
}

function ts_check_enosys_syscalls {
	ts_check_test_command "$TS_CMD_ENOSYS"
	"$TS_CMD_ENOSYS" ${@/#/-s } true 2> /dev/null
	[ $? -ne 0 ] && ts_skip "test_enosys does not work: $*"
}

function ts_skip_docker {
	test -e /.dockerenv && ts_skip "unsupported in docker environment"
}
