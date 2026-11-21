#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Sprint 11.1 of the WiFi-on-Pizlix port: the kernel rebuild with
# CONFIG_MAC80211_HWSIM=m (the mac80211 hardware simulator -- virtual WiFi
# radios for QEMU testing of the cfg80211/mac80211 stack before any real
# hardware exists).  The iwlwifi stack itself (CONFIG_IWLWIFI=y,
# CONFIG_IWLDVM=y, CONFIG_IWLMVM=y, CONFIG_CFG80211=y, CONFIG_MAC80211=y,
# CONFIG_RFKILL=y, CONFIG_FW_LOADER=y) was already built into the kernel by
# build_postlc_sub2_chroot_part2.sh -- the ONLY config change is the one
# HWSIM line in pizlix/linux-config-6.10.5 (tracked; the full chain picks it
# up via build_copy_stuff.sh copying it to /sources and via part2's kernel
# block; the recovery chain picks it up because build_postlc11.sh copies the
# tracked config into $LFS/sources before this script runs).
#
# This is a drop-in re-run of ONLY the kernel block of part2 (lines 625-642
# there), in the mutter_gir drop-in style: same commands, same order, run in
# the chroot with CWD=/sources.  perf (part2 lines 653-693) is NOT rebuilt:
# it is already installed and a kernel rebuild does not invalidate it.
#
# =m (not =y): hwsim=y would auto-create virtual radios on every real boot;
# =m means the radios exist only after an explicit `modprobe
# mac80211_hwsim`, which is what the QEMU test gate wants.
#
# NB: cd before rm: the chroot drops us in the caller's CWD, so the rm must
# happen with an absolute CWD for the relative path to hit the stale tree
# (same as the mutter_gir/gjs/gnome-shell scripts).
cd /sources
rm -rf pizlonated-linux

# --- the part2:625-642 kernel block, verbatim -------------------------------

tar -xf pizlonated-linux.tar.gz
cd pizlonated-linux
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option" mrproper
cp -v ../linux-config-6.10.5 .config
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option"
make CC=/yolo/bin/gcc HOSTCC="/usr/bin/gcc -Wno-unknown-warning-option" modules_install
cp -v arch/x86/boot/bzImage /boot/vmlinuz-6.10.5-lfs-12.2
cp -v System.map /boot/System.map-6.10.5
cp -v .config /boot/config-6.10.5
cp -r Documentation -T /usr/share/doc/linux-6.10.5
install -v -m755 -d /etc/modprobe.d
cat > /etc/modprobe.d/usb.conf << "EOF"
# Begin /etc/modprobe.d/usb.conf
install ohci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe -i ohci_hcd ; true
install uhci_hcd /sbin/modprobe ehci_hcd ; /sbin/modprobe -i uhci_hcd ; true
# End /etc/modprobe.d/usb.conf
EOF

# --- gates --------------------------------------------------------------------

echo "=== gate: the config that was installed ==="
grep -E "^CONFIG_MAC80211=|^CONFIG_MAC80211_HWSIM=|^CONFIG_IWLWIFI=|^CONFIG_IWLDVM=|^CONFIG_IWLMVM=|^CONFIG_CFG80211=|^CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=|^CONFIG_CFG80211_USE_KERNEL_REGDB_KEYS=|^CONFIG_RFKILL=|^CONFIG_FW_LOADER=" /boot/config-6.10.5

echo "=== gate: the hwsim module was built and installed ==="
test -e /lib/modules/6.10.5/kernel/drivers/net/wireless/virtual/mac80211_hwsim.ko
ls -la /lib/modules/6.10.5/kernel/drivers/net/wireless/virtual/

echo "=== gate: modules.dep knows mac80211_hwsim (depmod ran in modules_install) ==="
grep -q "mac80211_hwsim" /lib/modules/6.10.5/modules.dep
grep "mac80211_hwsim" /lib/modules/6.10.5/modules.dep

echo "=== gate: the iwlwifi firmware manifest (what the firmware stage installs) ==="
UCODES=`tr '\0' '\n' < /lib/modules/6.10.5/modules.builtin.modinfo | grep -o 'iwlwifi-[A-Za-z0-9-]*\.ucode' | sort -u`
echo "$UCODES" | wc -l
echo "$UCODES" | head -5
test `echo "$UCODES" | wc -l` -ge 50

echo "=== gate: iwlwifi is still builtin ==="
grep -q "iwlwifi" /lib/modules/6.10.5/modules.builtin
grep "wireless" /lib/modules/6.10.5/modules.builtin

echo "postlc11 kernel rebuild: ALL GATES GREEN"

# --- cleanup: part2 removes the extracted tree after its perf block; this
# drop-in re-runs only the kernel block, so the cleanup rides here instead
# (keep the /sources snapshot small; the tarball stays).
cd /sources
rm -rf pizlonated-linux
hash -r