#!/bin/bash

set -e
set -x

# Sprint 10: postlc8-level smoke test for the GNOME Shell stage.
#
# Runs INSIDE the chroot as root, after build_postlc8_sub2_chroot.sh has
# built the shell stack and build_postlc7_chroot_setup_scripts_for_gnome.sh
# has re-installed the session wiring.  This is the counterpart of
# build_postlc7_chroot_gnome_smoke.sh: that one verifies the postlc7-level
# GNOME desktop (dbus/elogind/mutter/gnome-session + the session glue
# FILES) and deliberately checks NO shell artifacts, because gnome-shell
# and the introspection data only exist once THIS stage has run.
#
# (History: these shell-level checks used to live in the postlc7 smoke,
# where they aborted every fresh ./build.sh - the postlc7 smoke ran before
# the shell existed, while the shell-port sprints only saw a chroot that
# already had everything.  They were moved here.)

cd /sources

# --- File-level checks: the JS runtime stack (postlc8: mozjs, gjs) ------

test -x /usr/bin/gjs
test -f /usr/lib/pkgconfig/mozjs-115.pc
test -f /usr/lib/pkgconfig/gjs-1.0.pc

# --- File-level checks: the shell and its introspection data ------------

test -x /usr/bin/gnome-shell
test -x /usr/bin/gnome-extensions

# The shell's own introspection data (installed into /usr/lib/gnome-shell).
test -f /usr/lib/gnome-shell/Shell-14.typelib
test -f /usr/lib/gnome-shell/St-14.typelib
test -f /usr/lib/gnome-shell/Gvc-1.0.typelib

# The mutter introspection rebuild (the postlc7 mutter is built with
# -Dintrospection=false; this stage rebuilds it with introspection).
test -f /usr/lib/mutter-14/Meta-14.typelib
test -f /usr/lib/mutter-14/Clutter-14.typelib
test -f /usr/lib/mutter-14/Cally-14.typelib
test -f /usr/lib/mutter-14/Cogl-14.typelib
test -f /usr/lib/mutter-14/Mtk-14.typelib

# gnome-desktop introspection rebuild + the at-spi2-core full build.
test -f /usr/lib/girepository-1.0/GnomeDesktop-4.0.typelib
test -f /usr/lib/girepository-1.0/GnomeBG-4.0.typelib
test -f /usr/lib/girepository-1.0/GnomeRR-4.0.typelib
test -f /usr/lib/girepository-1.0/Atspi-2.0.typelib

# The services/libraries the shell's JS talks to (all postlc8 builds).
for t in Polkit-1.0 Gcr-4 UPowerGlib-1.0 AccountsService-1.0 \
         Geoclue-2.0 GWeather-4.0 GMenu-3.0 IBus-1.0; do
    test -f /usr/lib/girepository-1.0/$t.typelib
done

# hicolor-icon-theme (postlc8, data only) completes the icon-theme
# lookup chain (Adwaita, installed by postlc7, ships no hicolor
# index.theme).
test -f /usr/share/icons/hicolor/index.theme

# --- Runtime checks ------------------------------------------------------

# The engine runs plain JS.
gjs -c 'print("gjs ok")'

# ...and loads the shell's introspection data through gjs.  Same
# environment /usr/bin/pizlix-shell exports and the postlc8 headless gate
# verified: the shell/mutter typelibs live outside the default
# girepository dir.  A private session bus, like the headless gate.
export GI_TYPELIB_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14
export LD_LIBRARY_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14
dbus-run-session -- gjs -c 'imports.gi.Meta; imports.gi.Clutter; imports.gi.St; imports.gi.Shell; print("gjs imports ok")'

# The shell binary identifies itself.
gnome-shell --version

# --- The session wiring survived this stage's setup run ------------------

test -x /usr/bin/pizlix-shell
grep -v '^#' /usr/share/gnome-session/sessions/pizlix-gnome.session \
    | grep '^RequiredComponents=org.gnome.Shell;$'
grep '^Exec=/usr/bin/pizlix-shell$' /etc/xdg/autostart/org.gnome.Shell.desktop
test ! -e /etc/xdg/autostart/pizlix-mutter.desktop

echo GNOME-SHELL-SMOKE-OK
