#!/bin/bash

set -e
set -x

# Sprint W3 of the Epiphany-on-Pizlix port: the live-check pre-seed, made
# reproducible.  The postlc9 stage snapshot (lfs-postlc9.tar.gz) predates
# the live-check seeding: it carries the Pizlix ssh HOST keys (verified
# present in the tarball) but NOT the CA trust bundle and NOT the t800 ssh
# authorized_keys -- those were copied into the live /mnt/lfs by hand in
# earlier sprints, and the W3 recovery-based chain test (recover postlc9 ->
# rerun the whole postlc10 stage) wipes them.  This script re-seeds them
# from the repo so the chain test reproduces a bootable, TLS-capable
# Pizlix:
#
# 1. /etc/ssl/ca-bundle.crt: the Mozilla CA bundle (make-ca format) that
#    earlier sprints pre-seeded.  Pizlix's GnuTLS (postlc5) is built with
#    --with-default-trust-store-pkcs11="pkcs11:" so the system trust comes
#    from the p11-kit trust module (postlc2, trust_paths=/etc/pki/anchors)
#    -- WITHOUT a populated anchors dir glib-networking fails every https
#    fetch with "System trust contains zero trusted certificates".
# 2. /etc/pki/anchors/ca-bundle.crt: the same bundle, copied into the
#    p11-kit trust path (p11-kit-trust.so parses PEM bundle files there).
# 3. /etc/ssl/certs/: the bundle split into individual PEM files plus the
#    openssl hash symlinks (openssl rehash, run inside the chroot), so
#    OpenSSL-based consumers find the same trust.
# 4. /root/.ssh/authorized_keys + /home/pizlo/.ssh/authorized_keys: the
#    t800 key (root and pizlo carry the same single key).
#
# Runs as root on the host with the repo as CWD.  Safe to re-run.

test $EUID -eq 0

export LFS=/mnt/lfs
test -d $LFS

# CWD-independent: the repo root is the parent of this script's pizlix/
# directory (this script is invoked both as ./build_preseed_pizlix.sh from
# pizlix/ by build_with_recovered_postlc9.sh and as
# pizlix/build_preseed_pizlix.sh from the repo root).
SRCDIR=`cd "\`dirname \"$0\"\`/.." && pwd`

# 1-2. the CA bundle + the p11-kit trust anchors
install -vDm644 $SRCDIR/pizlix/etc/ssl/ca-bundle.crt $LFS/etc/ssl/ca-bundle.crt
install -vDm644 $SRCDIR/pizlix/etc/ssl/ca-bundle.crt $LFS/etc/pki/anchors/ca-bundle.crt

# 3. split the bundle into individual certs for /etc/ssl/certs
rm -rf $LFS/etc/ssl/certs
install -vdm755 $LFS/etc/ssl/certs
awk '/BEGIN CERTIFICATE/,/END CERTIFICATE/' $SRCDIR/pizlix/etc/ssl/ca-bundle.crt \
    | csplit -sz -f $LFS/etc/ssl/certs/cert- -b '%04d.pem' - '/BEGIN CERTIFICATE/' '{*}'
chmod 644 $LFS/etc/ssl/certs/cert-*.pem
chroot $LFS /usr/bin/openssl rehash /etc/ssl/certs

# 4. the ssh key for root and pizlo
install -vDm600 $SRCDIR/pizlix/etc/authorized_keys $LFS/root/.ssh/authorized_keys
install -vDm600 -o 1000 -g 1000 $SRCDIR/pizlix/etc/authorized_keys $LFS/home/pizlo/.ssh/authorized_keys

# 5. DNS for the chroot-side gates (and as a sane default for the boot):
#    dhcpcd rewrites /etc/resolv.conf at every Pizlix boot (and empties it
#    again on shutdown), but the chroot-side builds run without dhcpcd, so
#    seed the QEMU user-net resolver address, which is stable for this VM.
printf 'nameserver 10.0.2.3\n' > $LFS/etc/resolv.conf

echo "== pre-seed summary:"
ls -la $LFS/etc/ssl/ca-bundle.crt $LFS/etc/pki/anchors/
ls $LFS/etc/ssl/certs | wc -l
ls -la $LFS/root/.ssh/authorized_keys $LFS/home/pizlo/.ssh/authorized_keys
echo "pre-seed OK"
