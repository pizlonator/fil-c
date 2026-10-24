#!/bin/bash
# Start a GNOME (Wayland) session: gnome-shell (the mutter-based Wayland
# compositor with the shell UI on top) run by gnome-session.
# gnome-shell itself is started by gnome-session as the DisplayServer-phase
# required component (see /etc/xdg/autostart/org.gnome.Shell.desktop).
set -e
export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP=GNOME
# Software rendering for the session: the shell's compositor is mutter's
# cogl, so mutter's two workarounds have to be visible to everything
# gnome-session autostarts (pizlix-shell.sh exports them too; these session
# copies cover anything that initializes GL outside the shell wrapper).
export GBM_ALWAYS_SOFTWARE=true
export NIR_DEBUG=novalidate
# No xdg-desktop-portal is built, and libadwaita only falls back to reading
# the gsettings color-scheme (Dark Style) when the portal is disabled, so
# force the gsettings path for everything in the session.
export ADW_DISABLE_PORTAL=1
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
mkdir -m 700 -p "$XDG_RUNTIME_DIR" 2>/dev/null || true
exec dbus-run-session -- gnome-session --session=pizlix-gnome
