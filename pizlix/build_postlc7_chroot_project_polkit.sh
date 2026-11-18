#!/bin/bash

set -e
set -x

# Built in postlc7 (moved from postlc8): gnome-control-center (later in this
# stage) hard-requires polkit-gobject-1 at meson setup, and accountsservice
# (also moved to this stage, built after polkit) hard-requires it too;
# gnome-shell (postlc8) additionally needs the Polkit-1.0/PolkitAgent-1.0
# typelibs produced here.  Its own deps are all earlier-or-in-stage: glib
# (postlc3), expat (postlc), duktape (built just before this script),
# Linux-PAM (built at the top of this stage), libelogind (elogind build
# earlier in this stage), gobject-introspection (postlc3).

# polkit 126 (PolicyKit authorization manager).
#
# Flags (checked against meson_options.txt):
# -D session_tracking=elogind: the chroot has libelogind (no libsystemd);
#   logind would need libsystemd.
# -D os_type=lfs: installs PAM config that includes system-auth (pizlix's
#   Linux-PAM layout); also makes the privileged group "wheel".
# -D authfw=pam (default): Linux-PAM is installed.
# -D introspection=true (default): we need Polkit-1.0 and PolkitAgent-1.0
#   typelibs for gnome-shell.
# -D tests=false, man=false, gtk_doc=false, gettext=false (all defaults):
#   keep it minimal.
# -D libs-only=false (default): we also build polkitd/pkexec.  The build does
#   NOT need the polkitd user (meson_post_install.py only chowns pkexec and
#   rules.d when the user exists and prints a notice otherwise), but we create
#   the user anyway so the daemon is usable at runtime.
#
# Fil-C fixes (folded into projects/polkit.projeny):
# - src/symbol.map: pizlonated* export (consumers link the pizlonated_ thunks).
# - GType-typed g_once_init statics (enum templates, PolkitIdentity,
#   PolkitSubject, PolkitBackendActionLookup): gpointer + _pointer variants,
#   so the GType keeps its pointer representation under the Fil-C glib.
getent passwd polkitd >/dev/null || useradd -r -M -d / -s /bin/false polkitd

cd /sources
rm -rf pizlonated-polkit
tar -xf pizlonated-polkit.tar.gz
cd pizlonated-polkit
mkdir -v filc-build
cd filc-build
meson setup ..                    \
      --prefix=/usr               \
      --buildtype=debugoptimized  \
      -Dsession_tracking=elogind  \
      -Dos_type=lfs               \
      -Dtests=false               \
      -Dman=false                 \
      -Dgtk_doc=false             \
      -Dgettext=false
ninja
ninja install
cd ../..
rm -rf pizlonated-polkit
hash -r
