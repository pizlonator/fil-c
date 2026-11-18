#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

# Sprint W2 of the Epiphany-on-Pizlix port: the WebKitGTK build.
#
# WebKitGTK 2.44.3 (webkitgtk-6.0 API, USE_GTK4=ON) is the last big
# dependency Epiphany 46 needs.  The Fil-C JIT gate: any PROCESS that
# loads swrast_dri.so and triggers llvmpipe shader compilation must link
# filc_jit_crt.o via LDFLAGS="-enable-filc-jit" (cmake reads LDFLAGS only
# at configure; the flag applies to executables, shared libs warn-and-
# ignore it).  Of the binaries this build installs: MiniBrowser and
# WebProcess (EGL/llvmpipe compositing) need the JIT CRT, NetworkProcess
# is safe, jsc is headless so the flag is harmless there.
./build_postlc10_chroot_project_webkitgtk.sh
hash -r

# Sprint W3 of the Epiphany-on-Pizlix port: the three Epiphany packages,
# in dependency order.  desktop-file-utils first (its desktop-file-validate
# is the optional test runner epiphany's data/meson.build finds and our
# epiphany gate runs on the installed .desktop file); libportal next
# (Epiphany's meson dependency libportal-gtk4 >= 0.6); epiphany last
# (needs webkitgtk-6.0 from above, libportal-gtk4, and desktop-file-
# validate from the two builds before it).
#
# The gnutls-trust rebuild sits between them: the W3 live pre-check found
# every https fetch failing with "System trust contains zero trusted
# certificates" (the postlc5 GnuTLS was pkcs11-only and the p11-kit-trust
# module serves zero objects), so GnuTLS is rebuilt with the PEM bundle
# from build_preseed_pizlix.sh as an additional default trust store before
# epiphany is built (see build_postlc10_chroot_project_gnutls_trust.sh).
./build_postlc10_chroot_project_desktop_file_utils.sh
hash -r
./build_postlc10_chroot_project_libportal.sh
hash -r
./build_postlc10_chroot_project_gnutls_trust.sh
hash -r
./build_postlc10_chroot_project_epiphany.sh
hash -r
