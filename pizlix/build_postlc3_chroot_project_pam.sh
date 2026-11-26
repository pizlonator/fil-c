#!/bin/bash

set -e
set -x

# Linux-PAM 1.7.3 (meson build, per projects/Linux-PAM.projeny).
# Flags mirror optfil/build_opt.sh's known-good Fil-C recipe, retargeted
# from /opt/fil to /usr.  BLFS creates /etc/pam.d/other below.

rm -rf pizlonated-pam
tar -xf pizlonated-pam.tar.gz
cd pizlonated-pam
mkdir -v build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D libdir=/usr/lib \
      -D sysconfdir=/etc \
      -D vendordir= \
      -D docs=disabled \
      -D nis=disabled \
      -D logind=disabled \
      -D elogind=disabled \
      -D selinux=disabled \
      -D audit=disabled \
      -D econf=disabled \
      -D pwaccess=disabled \
      -D openssl=enabled \
      -D read-both-confs=true \
      -D usergroups=true \
      ..
ninja
ninja install
chmod -v 4755 /usr/sbin/unix_chkpwd
cd ../..
rm -rf pizlonated-pam
hash -r

# BLFS: create the fallback PAM configuration.
install -vdm 755 /etc/pam.d
cat > /etc/pam.d/other << "EOF"
# Begin /etc/pam.d/other

auth            required        pam_unix.so     nullok
account         required        pam_unix.so
session         required        pam_unix.so
password        required        pam_unix.so     nullok

# End /etc/pam.d/other
EOF
