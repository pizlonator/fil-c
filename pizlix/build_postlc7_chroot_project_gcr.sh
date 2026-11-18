#!/bin/bash

set -e
set -x

# Built in postlc7 (moved from postlc8): gnome-control-center (later in this
# stage) hard-requires gcr-4 >= 4.1.0 at meson setup; gnome-shell (postlc8)
# needs it (and the Gcr-4 typelib) too.  Its own deps are all
# earlier-or-in-stage: p11-kit (postlc2), libgcrypt (postlc5), gtk4 (the
# GL/EGL rebuild earlier in this stage), glib (postlc3).

# gcr-4 4.3.0 (GCR certificate/crypto widgets + Gcr-4 gir for gnome-shell's
# certificate chooser / key prompts).
#
# Flags (checked against meson_options.txt):
# -D introspection=true (default): Gcr-4 typelib.
# -D vapi=false: no valac in the chroot.
# -D gtk4=true (default): gtk4 is installed; the Gcr-4 gir needs the ui bits.
# -D gtk_doc=false: gi-docgen not vendored.
# -D gpg_path=/usr/bin/gpg2: upstream runs find_program('gpg2','gpg') (a hard
#   error when missing) only when gpg_path is empty; passing a path skips the
#   lookup.  gnupg is not ported — the path is only used by the GnuPG
#   importer at runtime, which gnome-shell does not exercise.
# -D crypto=libgcrypt (default): libgcrypt 1.11 installed.
# -D ssh_agent=false: needs libsecret-1 (not ported); gnome-shell does not
#   use gcr-4's ssh-agent component.
# -D systemd=auto → disabled (no libsystemd).
#
# Fil-C fixes (projects/gcr.projeny):
# - gck/libgck.map + gcr/libgcr.map: + pizlonated*;
cd /sources
rm -rf pizlonated-gcr
tar -xf pizlonated-gcr.tar.gz
cd pizlonated-gcr
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized \
      -Dvapi=false              \
      -Dgtk_doc=false           \
      -Dgpg_path=/usr/bin/gpg2  \
      -Dssh_agent=false
ninja
ninja install
cd ../..
rm -rf pizlonated-gcr
hash -r
