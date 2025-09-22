#!/bin/bash
# The following is a synthesis of info in:
#
#  http://vmsplice.net/~stefan/stefanha-kernel-recipes-2015.pdf
#  http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/README
#
KBASE=../../linux
#APPEND="console=ttyS0"

function die {
    echo "$*"
    exit 1
}

pushd ..
make LIBCSTATIC=yes all test || die "failed to make all test of libcap tree"
make LIBCSTATIC=yes -C progs tcapsh-static || die "failed to make progs/tcapsh-static"
make -C tests uns_test
popd

# Assumes desired make *config (eg. make defconfig) is already done.
pushd $KBASE
pwd
make V=1 all || die "failed to build kernel: $0"
popd

HERE=$(/bin/pwd)

cat > fs.conf <<EOF
file /init test-init.sh 0755 0 0
dir /etc 0755 0 0
file /etc/passwd test-passwd 0444 0 0
dir /lib 0755 0 0
dir /proc 0755 0 0
dir /dev 0755 0 0
dir /sys 0755 0 0
dir /sbin 0755 0 0
file /sbin/busybox /usr/sbin/busybox 0755 0 0
dir /bin 0755 0 0
file /bin/myprompt test-prompt.sh 0755 0 0
file /bin/bash test-bash.sh 0755 0 0
dir /usr 0755 0 0
dir /usr/bin 0755 0 0
dir /root 0755 0 0
file /root/quicktest.sh $HERE/../progs/quicktest.sh 0755 0 0
file /root/setcap $HERE/../progs/setcap 0755 0 0
file /root/getcap $HERE/../progs/getcap 0755 0 0
file /root/capsh $HERE/../progs/capsh 0755 0 0
file /root/getpcaps $HERE/../progs/getpcaps 0755 0 0
file /root/tcapsh-static $HERE/../progs/tcapsh-static 0755 0 0
file /root/exit $HERE/exit 0755 0 0
file /root/uns_test $HERE/../tests/uns_test 0755 0 0
EOF

# convenience for some local experiments
if [ -f "$HERE/extras.sh" ]; then
    echo "local, uncommitted enhancements to kernel test"
    . "$HERE/extras.sh"
fi

if [ -f "$HERE/interactive" ]; then
    echo "file /root/interactive $HERE/interactive 0755 0 0" >> fs.conf
fi

COMMANDS="awk cat chmod cp dmesg grep id less ln ls mkdir mount pwd rm rmdir sh sort umount uniq vi"
for f in $COMMANDS; do
    echo slink /bin/$f /sbin/busybox 0755 0 0 >> fs.conf
done

UCOMMANDS="id cut"
for f in $UCOMMANDS; do
    echo slink /usr/bin/$f /sbin/busybox 0755 0 0 >> fs.conf
done

$KBASE/usr/gen_init_cpio fs.conf | gzip -9 > initramfs.img

KERNEL=$KBASE/arch/$(uname -m)/boot/bzImage

qemu-system-$(uname -m) -m 1024 \
		   -kernel $KERNEL \
		   -initrd initramfs.img \
		   -append "$APPEND" \
		   -smp sockets=2,dies=1,cores=4 \
		   -device isa-debug-exit
