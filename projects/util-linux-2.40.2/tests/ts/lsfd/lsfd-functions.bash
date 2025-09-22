#!/bin/bash
#
# Copyright (C) 2021 Masatake YAMATO <yamato@redhat.com>
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

# The exit-status used in a test target.
readonly EPERM=18
readonly ENOPROTOOPT=19
readonly EPROTONOSUPPORT=20
readonly EACCES=21
readonly ENOENT=22

function lsfd_wait_for_pausing {
	ts_check_prog "sleep"

	local PID=$1
	until [[ $(ps --no-headers -ostat "${PID}") =~ S.* ]]; do
		sleep 1
	done
}

function lsfd_compare_dev {
    local LSFD=$1
    local FILE=$2
    local EXPR=$3

    ts_check_prog "grep"
    ts_check_prog "expr"
    ts_check_prog "stat"

    local DEV=$("${LSFD}" --raw -n -o DEV -Q "${EXPR}")
    echo 'DEV[RUN]:' $?
    local MAJ=${DEV%:*}
    local MIN=${DEV#*:}
    local DEVNUM=$(( ( MAJ << 8 ) + MIN ))
    local STAT_DEVNUM=$(stat -c "%d" "$FILE")
    echo 'STAT[RUN]:' $?
    if [ "${DEVNUM}" == "${STAT_DEVNUM}" ]; then
	echo 'DEVNUM[STR]:' 0
    else
	echo 'DEVNUM[STR]:' 1
	# Print more information for debugging
	echo 'DEV:' "${DEV}"
	echo 'MAJ:MIN' "${MAJ}:${MIN}"
	echo 'DEVNUM:' "${DEVNUM}"
	echo 'STAT_DEVNUM:' "${STAT_DEVNUM}"
    fi
}

function lsfd_strip_type_stream
{
    # lsfd changes the output of NAME column for a unix stream socket
    # whether the kernel reports it is a "UNIX-STREAM" socket or a
    # "UNIX" socket. For "UNIX", lsfd appends "type=stream" to the
    # NAME column. Let's delete the appended string before comparing.
    sed -e 's/ type=stream//'
}

function lsfd_make_state_connected
{
    # Newer kernels report the states of unix dgram sockets created by
    # sockerpair(2) are "connected" via /proc/net/unix though Older
    # kernels report "unconnected".
    #
    # Newer kernels report the states of unix dgram sockets already
    # connect(2)'ed are "connected", too.
    #
    # https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=83301b5367a98c17ec0d76c7bc0ccdc3c7e7ad6d
    #
    # This rewriting adjusts the output of lsfd running on older kernels
    # to that on newer kernels.
    sed -e 's/state=unconnected/state=connected/'
}

function lsfd_check_mkfds_factory
{
	local FACTORY=$1

	ts_check_test_command "$TS_HELPER_MKFDS"
	if ! "$TS_HELPER_MKFDS" --is-available "$FACTORY"; then
		ts_skip "test_mkfds has no factory for $FACTORY"
	fi
}

function lsfd_check_sockdiag
{
	local family=$1

	ts_check_test_command "$TS_HELPER_MKFDS"

	local msg
	local err

	msg=$("$TS_HELPER_MKFDS" -c sockdiag 9 family=$family 2>&1)
	err=$?

	case $err in
	    0)
		return;;
	    $EPROTONOSUPPORT)
		ts_skip "NETLINK_SOCK_DIAG protocol is not supported in socket(2)";;
	    $EACCES)
		ts_skip "sending a msg via a sockdiag netlink socket is not permitted";;
	    $ENOENT)
		ts_skip "sockdiag netlink socket is not available";;
	    *)
		ts_failed "failed to create a sockdiag netlink socket $family ($err): $msg";;
	esac
}
