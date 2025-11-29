#!/usr/bin/sh
#
builddir=$1
suite=$2
unit=$3

echo "** builddir: ${builddir}"
echo "**    suite: ${suite}"
echo "**     unit: ${unit}"

export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

weston --backend=headless-backend.so --socket=wayland-5 --idle-time=0 &
compositor=$!

export WAYLAND_DISPLAY=wayland-5

meson test -C ${builddir} \
            --print-errorlogs \
            --setup=wayland \
            --suite=${suite} \
            --no-suite=failing \
            --no-suite=flaky \
            --no-suite=wayland_failing \
            --no-suite=gsk-compare-broadway \
            --verbose \
            "${unit}"

exit_code=$?
kill ${compositor}

exit ${exit_code}
