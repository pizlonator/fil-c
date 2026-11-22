#!/bin/bash

set -e
set -x

ulimit -c unlimited

# GnuTLS system trust wiring.
#
# The live pre-check of the epiphany build found that every https fetch
# failed with glib-networking's "System trust contains zero trusted
# certificates".  The cause: the chroot's GnuTLS (built in postlc5) got
# its default trust store ONLY through PKCS#11
# (--with-default-trust-store-pkcs11="pkcs11:"), i.e. through the
# p11-kit-trust module, and that module serves zero objects even with the
# anchor files in place (its configured trust path /etc/pki/anchors was
# empty before the pre-seed, and after the pre-seed populates it
# the token still initializes with no objects -- the p11-kit trust module
# 0.26.5 in this system does not currently load them).
#
# Fix: rebuild GnuTLS with --with-default-trust-store-file=/etc/ssl/ca-
# bundle.crt ADDED to the existing pkcs11 setting.  add_system_trust()
# (lib/system/certs.c) loads BOTH stores, so the PEM bundle (the same
# Mozilla CA bundle the pre-seed script installs) reaches every GnuTLS
# consumer -- glib-networking, gnutls-cli -- regardless of the p11-kit
# state.  Same source tarball, same configure line as postlc5 plus the
# one new flag; the soname does not change, so nothing needs relinking.
#
# The bundle is installed by pizlix/build_preseed_pizlix.sh (host side),
# which also splits it into /etc/ssl/certs + the p11-kit anchors dir and
# seeds the ssh keys.  That script runs before this stage in the
# recovery-based chain test, and its output is carried by the stage
# snapshot.

cd /sources
rm -rf pizlonated-gnutls
tar -xf pizlonated-gnutls.tar.gz
cd pizlonated-gnutls
./configure --prefix=/usr \
            --docdir=/usr/share/doc/gnutls-3.8.7.1 \
            --with-default-trust-store-pkcs11="pkcs11:" \
            --with-default-trust-store-file=/etc/ssl/ca-bundle.crt \
            --disable-hardware-acceleration
make
make install
cd ..
rm -rf pizlonated-gnutls
hash -r

