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
#
# Sprint 11: the private session bus below (dbus-run-session, plus the
# gjs and dconf-service children it may spawn) is torn down by an EXIT
# trap, registered before the session starts: however this script exits -
# success, a failed check, a hung session - the whole process group goes,
# leaving no session bus behind in the chroot.

# PID of the dbus-run-session started below; empty until then.  The trap
# tolerates that (it also fires on failures that precede the session).
SESSION_PID=

cleanup() {
    if [ -n "$SESSION_PID" ]; then
        # dbus-run-session runs under setsid, so it leads its own session
        # and process group; the bus, gjs, and any dconf-service it
        # spawned are all in that group.  Kill the group (TERM, then KILL
        # after a grace sleep), then the pid itself for good measure.
        # The pgid is looked up from our own pid, so it can only name our
        # group - never a host process group.
        local pgid=""
        pgid=$(ps -o pgid= -p $SESSION_PID 2>/dev/null | tr -d ' ' || true)
        if [ -n "$pgid" ]; then
            kill -TERM -$pgid 2>/dev/null || true
            sleep 1
            kill -KILL -$pgid 2>/dev/null || true
        fi
        kill $SESSION_PID 2>/dev/null || true
    fi
}
trap cleanup EXIT

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
# dbus-run-session runs in the background under setsid so the EXIT trap
# can take down its whole process group if anything below fails while
# the bus is still up; wait() propagates gjs's exit status.
export GI_TYPELIB_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14
export LD_LIBRARY_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14
setsid dbus-run-session -- gjs -c 'imports.gi.Meta; imports.gi.Clutter; imports.gi.St; imports.gi.Shell; print("gjs imports ok")' &
SESSION_PID=$!
wait $SESSION_PID
SESSION_PID=

# The shell binary identifies itself.
gnome-shell --version

# --- The session wiring survived this stage's setup run ------------------

test -x /usr/bin/pizlix-shell
grep -v '^#' /usr/share/gnome-session/sessions/pizlix-gnome.session \
    | grep '^RequiredComponents=org.gnome.Shell;$'
grep '^Exec=/usr/bin/pizlix-shell$' /etc/xdg/autostart/org.gnome.Shell.desktop
test ! -e /etc/xdg/autostart/pizlix-mutter.desktop

echo GNOME-SHELL-SMOKE-OK
