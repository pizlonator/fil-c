#!/bin/sh -ex
#
# Copyright (c) 2018-2019 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

j=-j`nproc` || j=
type sudo >/dev/null 2>&1 && sudo=sudo || sudo=
packages="
bison
docbook5-xml
docbook-xsl-ns
flex
gettext
libaudit-dev
libdb-dev
libfl-dev
libselinux1-dev
libssl-dev
libxml2-utils
meson
pkg-config
sed
w3m
xsltproc
xz-utils
$CC"

retry_if_failed()
{
	for i in `seq 0 99`; do
		"$@" && i= && break || sleep 1
	done
	[ -z "$i" ]
}

updated=
apt_get_install()
{
	[ -n "$updated" ] || {
		retry_if_failed $sudo apt-get -qq update
		updated=1
	}
	retry_if_failed $sudo \
		apt-get -qq --no-install-suggests --no-install-recommends \
		install -y "$@"
}

case "$CC" in
	gcc-*)
		retry_if_failed \
			$sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
		;;
esac

case "$TARGET" in
	x32|x86)
		packages="$packages gcc-multilib"
		case "$CC" in
			gcc-*) packages="$packages $CC-multilib" ;;
		esac
		;;
esac

case "${USE_LOGIND-}" in
	yes)
		packages="$packages libsystemd-dev"
		;;
esac

apt_get_install $packages

case "${CHECK-}" in
	coverage)
		apt_get_install lcov python-pip python-setuptools
		retry_if_failed \
			pip install --user codecov
		;;
	valgrind)
		apt_get_install valgrind
		;;
esac
