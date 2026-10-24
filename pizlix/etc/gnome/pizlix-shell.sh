#!/bin/bash
# gnome-shell launcher for the GNOME session (DisplayServer phase).
#
# gnome-shell is the mutter-based Wayland compositor plus the shell UI on
# top; as the session's DisplayServer component it IS the compositor, so it
# runs with --wayland exactly like the standalone mutter did (do NOT pass
# --display-server: that is mutter's headless subprocess mode, not the
# session mode).  As a logind/elogind session client it becomes the DRM
# master of seat0 the same way mutter did.
#
# - GBM_ALWAYS_SOFTWARE=true: the only GPU here is QEMU's display-only
#   bochs-drm (no render node), so gbm/EGL must use the kms_swrast
#   (llvmpipe) path.  Without this, gbm binds the driver as if it were
#   hardware and EGL later dereferences a null
#   queryCompatibleRenderOnlyDeviceFd pointer.
#
# - NIR_DEBUG=novalidate disables NIR's shader validator.  The validator
#   bit-tags nir_src::_parent (packing a tag bit into what is now a real
#   parent pointer) and trips over the provenance-carrying _parent_p field;
#   a proper fix needs a validator-side refactor, so run with validation
#   off.  Known limitation, see the postlc7 notes.
#
# - GI_TYPELIB_PATH/LD_LIBRARY_PATH: the shell's introspection data lives
#   outside the default girepository dir (Shell-14/St-14/Gvc-1.0 in
#   /usr/lib/gnome-shell, Meta-14/Cogl-14/... in /usr/lib/mutter-14).  The
#   gnome-shell binary's RUNPATH covers the shared libraries, but gjs
#   resolves typelibs through GI_TYPELIB_PATH only - this is the
#   environment the postlc8 headless gate verified.
#
# The session leader (pizlix-gnome) exports the same environment so that
# everything gnome-session autostarts inherits it; these exports make the
# wrapper self-contained.
#
# - gnome-session considers a spawned autostart app "registered" only when
#   something calls org.gnome.SessionManager.RegisterClient with the app's
#   DESKTOP_AUTOSTART_ID (or an XSMP client presents that id as its
#   previous-id).  GNOME 46 dropped the shell's own registration code
#   (systemd user units do the phase accounting there) and we run without
#   systemd, so - exactly like pizlix-mutter.sh does - start the shell,
#   wait for the Wayland socket (so the next phase gets a WORKING
#   WAYLAND_DISPLAY), register, then stay in the foreground for as long as
#   the shell lives so gnome-session notices if it dies.  Without
#   registering, the DisplayServer phase would sit for 90 seconds and then
#   flag the required component as failed (popping the gnome-session
#   "something has gone wrong" dialog over the shell).
set -u

export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP=GNOME
export GI_TYPELIB_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14
export LD_LIBRARY_PATH=/usr/lib/gnome-shell:/usr/lib/mutter-14${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export GBM_ALWAYS_SOFTWARE=true
export NIR_DEBUG=novalidate
# The shell's Adw.StyleManager also needs this to pick up the gsettings
# color-scheme (Dark Style) when no xdg-desktop-portal is running.
export ADW_DISABLE_PORTAL=1

# Remove stale Wayland sockets from previous (crashed) compositor runs:
# otherwise the readiness check below sees a dead socket file and hands
# control to the next phase before the shell has bound a live one.
rm -f "$XDG_RUNTIME_DIR"/wayland-*

gnome-shell --wayland > /tmp/pizlix-shell-session.log 2>&1 &
SHELL_PID=$!

ready=0
for i in $(seq 1 150); do
    if ! kill -0 "$SHELL_PID" 2>/dev/null; then
        break
    fi
    if ls "$XDG_RUNTIME_DIR"/wayland-* >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.1
done

if [ "$ready" = 1 ]; then
    # Give the shell a moment to finish its Setenv round-trip (mutter
    # publishes WAYLAND_DISPLAY to the session right after socket
    # creation), then register.
    sleep 0.5
    if [ -n "${DESKTOP_AUTOSTART_ID:-}" ]; then
        dbus-send --session --print-reply \
            --dest=org.gnome.SessionManager /org/gnome/SessionManager \
            org.gnome.SessionManager.RegisterClient \
            string:org.gnome.Shell.desktop \
            "string:$DESKTOP_AUTOSTART_ID" >/dev/null 2>&1 || true
    fi
fi

wait "$SHELL_PID"
