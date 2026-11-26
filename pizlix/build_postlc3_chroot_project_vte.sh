#!/bin/bash

set -e
set -x

# vte 0.78.7 (the terminal widget library gnome-terminal needs; GTK3 widget
# only - gnome-terminal is a GTK3 app).
#
# Flags:
# -D _b_symbolic_functions=false: the house rule is no -Wl,-Bsymbolic-functions
#   (the pizlonator renames symbols with the pizlonated_ prefix; symbolic
#   binding would break the cross-library renames).
# -D gtk4=disabled: only the GTK3 widget is needed (and the GTK4 widget needs
#   gir anyway if anything downstream uses it from javascript).
# -D gir=false -D vapi=false: gnome-terminal is a C program; no introspection
#   or vala bindings are needed (g-ir-scanner would also slow the build down).
# -D glade=false -D docs=false: no glade catalogue, no gtk-doc.
# -D _systemd=false: no systemd user units.
# -D gnutls/icu/fribidi/a11y left enabled: gnutls (pty encryption), icu 78.3
#   (legacy charset conversion), fribidi (bidi) and atk (a11y) are all in the
#   chroot.
#
# Dependencies: gtk3, pango, cairo, glib 2.80.4, pcre2, icu, fribidi,
# gnutls - all pre-existing.
#
# Fil-C notes (see the projeny patch):
# - GLIB_VERSION_MAX_ALLOWED pinned to GLIB_VERSION_2_80 (vte pins 2.72, and
#   the pre-2.80 G_DEFINE_TYPE path treats the once-init static as gsize,
#   which is a type confusion now that GType is a pointer in the pizlonated
#   glib).
# - the vtetypebuiltins mkenums template gets a pointer-typed static and the
#   g_once_init_enter_pointer/leave_pointer variants (same reasoning).
# - src/missing.c: raw syscall(SYS_close_range)/syscall(SYS_getdents64)
#   replaced with the zsys_close_range/zsys_getdents wrappers (fil-C requires
#   syscalls to go through the zsys_* wrappers; there is no zsys_getdents64,
#   so the fdwalk /proc/self/fd scan uses the 32-bit getdents format).

# NB: cd before rm (same as the gjs/gnome-shell scripts): make the relative
# paths below independent of the chroot's inherited CWD.
cd /sources
rm -rf pizlonated-vte
tar -xf pizlonated-vte.tar.gz
cd pizlonated-vte
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized \
      -D _b_symbolic_functions=false \
      -D gtk3=true               \
      -D gtk4=false              \
      -D gir=false               \
      -D vapi=false              \
      -D glade=false             \
      -D docs=false              \
      -D _systemd=false
ninja
ninja install
cd ../..
rm -rf pizlonated-vte
hash -r
