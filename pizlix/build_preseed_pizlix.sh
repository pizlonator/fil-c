#!/bin/bash

set -e
set -x


test $EUID -eq 0

export LFS=/mnt/lfs
test -d $LFS

# CWD-independent: the repo root is the parent of this script's pizlix/
# directory (build_postlc2.sh invokes this script as
# ./build_preseed_pizlix.sh from pizlix/ to pre-seed the CA trust before
# the chroot builds gnutls, and it can also be run from the repo root as
# pizlix/build_preseed_pizlix.sh).
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

echo "== pre-seed summary:"
ls -la $LFS/etc/ssl/ca-bundle.crt $LFS/etc/pki/anchors/
ls $LFS/etc/ssl/certs | wc -l
echo "pre-seed OK"
