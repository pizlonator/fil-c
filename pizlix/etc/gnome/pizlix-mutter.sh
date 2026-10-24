#!/bin/bash
# mutter launcher for the GNOME session (DisplayServer phase).
#
# - GBM_ALWAYS_SOFTWARE=true: the only GPU here is QEMU's display-only
#   bochs-drm (no render node), so gbm/EGL must use the kms_swrast
#   (llvmpipe) path.  Without this, gbm binds the driver as if it were
#   hardware and EGL later dereferences a null
#   queryCompatibleRenderOnlyDeviceFd pointer.
#
# - gnome-session considers a spawned autostart app "registered" only when
#   the app calls org.gnome.SessionManager.RegisterClient with its
#   DESKTOP_AUTOSTART_ID (or via XSMP / a D-Bus name, none of which mutter
#   does).  Without registering, the phase would sit for 90 seconds and
#   then flag mutter as an unrecoverable required component.  So: start
#   mutter, wait for the Wayland socket (so the next phase gets a WORKING
#   WAYLAND_DISPLAY), register, then stay in the foreground for as long as
#   mutter lives so gnome-session notices if it dies.
set -u

export GBM_ALWAYS_SOFTWARE=true

# Fil-C: NIR_DEBUG=novalidate disables NIR's shader validator.  The
# validator bit-tags nir_src::_parent (packing a tag bit into what is now a
# real parent pointer) and trips over the provenance-carrying _parent_p
# field; a proper fix needs a validator-side refactor, so run with
# validation off.  Known limitation, see the postlc7 notes.
export NIR_DEBUG=novalidate

# Force the software (stage-drawn) cursor: bochs-drm exposes only the
# primary KMS plane (no hardware cursor plane), so mutter must not try the
# KMS cursor manager; with this env it logs "Disabling hardware cursors
# because MUTTER_DEBUG_DISABLE_HW_CURSORS is set" and the cursor is drawn by
# the software cursor renderer as part of the stage paint - the only path
# that can work on this hardware.  (The GNOME session wrappers export it
# too; this copy keeps mutter-only testing consistent.)
export MUTTER_DEBUG_DISABLE_HW_CURSORS=1

# Remove stale Wayland sockets from previous (crashed) compositor runs:
# otherwise the readiness check below sees a dead socket file and hands
# control to the next phase before mutter has bound a live one.
rm -f "$XDG_RUNTIME_DIR"/wayland-*

mutter --wayland > /tmp/pizlix-mutter-session.log 2>&1 &
MUTTER_PID=$!

ready=0
for i in $(seq 1 150); do
    if ! kill -0 "$MUTTER_PID" 2>/dev/null; then
        break
    fi
    if ls "$XDG_RUNTIME_DIR"/wayland-* >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.1
done

if [ "$ready" = 1 ]; then
    # Give mutter a moment to finish its Setenv round-trip (it happens
    # right after socket creation), then register.
    sleep 0.5
    if [ -n "${DESKTOP_AUTOSTART_ID:-}" ]; then
        dbus-send --session --print-reply \
            --dest=org.gnome.SessionManager /org/gnome/SessionManager \
            org.gnome.SessionManager.RegisterClient \
            string:pizlix-mutter.desktop \
            "string:$DESKTOP_AUTOSTART_ID" >/dev/null 2>&1 || true
    fi
fi

wait "$MUTTER_PID"
