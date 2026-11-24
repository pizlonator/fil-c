#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Fix mount(8) aborting under Fil-C on
# any type-less mount (e.g. `mount /dev/sda2 /mnt/u`).  libmount's new-mount-API
# probe fsopen_is_supported() called fsopen(NULL, FSOPEN_CLOEXEC); the Fil-C
# fsopen wrapper read-checks the filesystem-name string and a NULL name is a
# null read, so every type-less mount(8) died with a filc panic before doing
# anything (see /var/filc/panics/1795130885-mount-4416 on the Pizlix image).
# The fix (now in the projeny patch) probes with an empty name, which the
# kernel answers with EINVAL instead of EFAULT when the syscall exists and
# with ENOSYS when it does not -- identical probe semantics, valid C.
#
# This script rebuilds util-linux inside the Pizlix chroot from the updated
# tarball (one pass; libudev is already installed, so the second-pass
# configure line from build_postlc_sub2_chroot_part2.sh applies), and gates on
# a type-less mount actually surviving the probe.

NPROC=$(nproc)

cd /sources
rm -rf pizlonated-util-linux
tar -xf pizlonated-util-linux.tar.gz
cd pizlonated-util-linux

./configure --bindir=/usr/bin \
    --libdir=/usr/lib \
    --runstatedir=/run \
    --sbindir=/usr/sbin \
    --disable-chfn-chsh \
    --disable-login \
    --disable-nologin \
    --disable-su \
    --disable-setpriv \
    --disable-runuser \
    --disable-pylibmount \
    --disable-liblastlog2 \
    --disable-static \
    --without-python \
    --without-systemd \
    --without-systemdsystemunitdir \
    ADJTIME_PATH=/var/lib/hwclock/adjtime \
    --docdir=/usr/share/doc/util-linux-2.42.4

make -j$NPROC
make install
cd ..
rm -rf pizlonated-util-linux
hash -r

echo "=== gate: mount + libmount installed ==="
test -x /bin/mount
test -e /usr/lib/libmount.so.1
ls -la /bin/mount /usr/lib/libmount.so.1*

echo "=== gate: type-less mount survives the fsopen probe (the old bug panicked here) ==="
mkdir -p /mnt/fixcheck
set +e
mount /dev/sda2 /mnt/fixcheck 2>/mnt/fixcheck.err
RC=$?
set -e
cat /mnt/fixcheck.err
if test $RC -eq 133 -o $RC -eq 132; then
    echo "FATAL: mount died on a trap (int3/SIGILL) - the fsopen probe still panics" >&2
    exit 1
fi
if grep -q "filc" /mnt/fixcheck.err 2>/dev/null; then
    echo "FATAL: filc panic in mount output" >&2
    exit 1
fi
if test $RC -eq 0; then
    echo "type-less mount SUCCEEDED (probe passed, mount completed)"
    umount /mnt/fixcheck
else
    echo "type-less mount returned $RC (a normal mount error, no trap) - probe fix OK"
fi
rm -rf /mnt/fixcheck /mnt/fixcheck.err

echo "postlc util-linux fsopen fix: ALL GATES GREEN"
