#!/bin/bash

set -e
set -x

# X11 extension client libraries needed by mutter/GNOME (postlc3).
# Same configure style as the X11 libraries in build_postlc3_sub2_chroot.sh.
# All of them depend on libX11/libXext/libXrender from this stage's X11
# library block and on libXfixes (built first here).

# libXi must come before libXtst (xtst's pkg-config requires xi).
for pkg in libXfixes libXcomposite libXcursor libXdamage libXi libXtst libxkbfile; do
    rm -rf pizlonated-$pkg
    tar -xf pizlonated-$pkg.tar.gz
    cd pizlonated-$pkg
    ./configure --prefix=/usr --sysconfdir=/etc
    make -j$(nproc)
    make install
    cd ..
    rm -rf pizlonated-$pkg
    hash -r
done
