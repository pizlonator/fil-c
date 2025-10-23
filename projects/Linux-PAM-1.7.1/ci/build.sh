#!/bin/sh -ex
#
# Copyright (c) 2018-2024 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

opts='-Doptimization=2 -Dwerror=true -Dpam_lastlog=enabled'

case "${VENDORDIR-}" in
	*/*)
		opts="$opts -Dvendordir=$VENDORDIR"
		;;
esac

case "${USE_LOGIND-}" in
	yes)
		opts="$opts -Dlogind=enabled"
		;;
esac

case "${USE_OPENSSL-}" in
	yes)
		opts="$opts -Dopenssl=enabled"
		;;
esac

case "${ENABLE_DEBUG-}" in
	yes)
		opts="$opts -Dpam-debug=true"
		;;
esac

echo 'BEGIN OF BUILD ENVIRONMENT INFORMATION'
uname -a |head -1
libc="$(ldd /bin/sh |sed -n 's|^[^/]*\(/[^ ]*/libc\.so[^ ]*\).*|\1|p' |head -1)"
$libc |head -1
$CC --version |head -1
meson --version |head -1
ninja --version |head -1
kver="$(printf '%s\n%s\n' '#include <linux/version.h>' 'LINUX_VERSION_CODE' | $CC -E -P -)"
printf 'kernel-headers %s.%s.%s\n' $((kver/65536)) $((kver/256%256)) $((kver%256))
echo 'END OF BUILD ENVIRONMENT INFORMATION'

mkdir build
meson setup $opts build

# If "meson dist" supported -v option, it could be used here
# instead of all subsequent individual meson commands.

meson compile -v -C build
mkdir build/destdir
DESTDIR=$(pwd)/build/destdir meson install -C build
meson test -v -C build

if git status --porcelain |grep '^?'; then
	echo >&2 'git status reported untracked files'
	exit 1
fi
