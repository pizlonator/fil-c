#! /bin/sh

srcdir=${MESON_CURRENT_SOURCE_DIR:-./testsuite/headless}
builddir=${MESON_CURRENT_BUILD_DIR:-.}
outputdir=${builddir}/input

mkdir -p ${outputdir}


export GTK_A11Y=none

dbus-run-session sh 2>${outputdir}/dbus-stderr.log <<EOF

export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

pipewire >&${outputdir}/pipewire.log &
pipewire_pid=\$!
sleep 2

wireplumber >&${outputdir}/wireplumber.log &
wireplumber_pid=\$!
sleep 2

#echo DBUS_SESSION_BUS_ADDRESS=\$DBUS_SESSION_BUS_ADDRESS
#echo WAYLAND_DISPLAY=gtk-test

mutter --headless --virtual-monitor 1024x768 --no-x11 --wayland-display gtk-test2 >&${outputdir}/mutter.log &
mutter_pid=\$!

sleep 2

export WAYLAND_DISPLAY=gtk-test2
export GDK_BACKEND=wayland

python3 ${srcdir}/headless-input-tests.py
status=\$?

kill \$mutter_pid
kill \$wireplumber_pid
kill \$pipewire_pid

exit \$status

EOF
