#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

# Sprint A of the WiFi-on-Pizlix port: the postlc11 stage packaging.
#
# There are no new projeny packages in the WiFi stage's Sprint A (libnl /
# libndp / wpa_supplicant / NetworkManager come later) -- what this stage
# needs is the two FIRMWARE tarballs, which are too big to commit to git:
#
# 1. linux-firmware-20241017.tar.xz (379MB compressed): the LFS-12.2-era
#    linux-firmware snapshot.  Only a manifest-driven subset (~195MB) is
#    installed onto the target by build_postlc11_chroot_project_firmware.sh
#    (the iwlwifi ucode/pnvm families the built-in drivers actually
#    declare), but the FULL tarball must be present in /sources for that
#    selection to pick from.  The ucode files sit FLAT in the tarball root,
#    which matches how the iwlwifi driver requests firmware (flat
#    /lib/firmware, no subdirectory).
#
# 2. wireless-regdb-master-2024-10-07.tar.gz: the regulatory database from
#    the wens repo (same date as the firmware snapshot).  The kernel has
#    CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y + CONFIG_CFG80211_USE_KERNEL_
#    REGDB_KEYS=y, and 6.10.5 embeds BOTH sforshee.hex and wens.hex as
#    trusted regdb signing keys (net/wireless/certs/) -- the wens repo's
#    prebuilt regulatory.db is wens-signed, so it validates against the
#    kernel's key.  linux-firmware does NOT ship regulatory.db.
#
# The download_tarball helper is the postlc6 one (sha256-verified, with
# --retry/--retry-all-errors/--continue-at so a flaky CDN does not kill a
# multi-hour one-shot build; it caches the tarball in pizlix/ so repeated
# stage runs do not re-download).  build_postlc11.sh copies both tarballs
# from pizlix/ into $LFS/sources after this script succeeds.

# Download large tarballs that are too big to commit to git.
# Each tarball is verified with SHA256 (using sha256sum from coreutils).
download_tarball() {
    local url="$1"
    local filename="$2"
    local expected_sha256="$3"
    local filepath="pizlix/$filename"

    if [ -f "$filepath" ]; then
        local actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
        if [ "$actual_sha256" = "$expected_sha256" ]; then
            echo "Already have $filename (sha256 verified)"
            return 0
        fi
        echo "sha256 mismatch for $filename, re-downloading"
        rm -f "$filepath"
    fi

    echo "Downloading $filename from $url"
    # Retry + resume: transient CDN flakiness killed a 4-hour one-shot build
    # here (curl error 92, "HTTP/2 stream not closed cleanly", from
    # archives.boost.org mid-transfer).  Plain --retry does not cover such
    # errors, so --retry-all-errors retries on any error, and --continue-at -
    # resumes the partial file from where the failed attempt left off
    # instead of restarting a 120MB download from byte 0.
    curl -L --retry 5 --retry-all-errors --continue-at - -o "$filepath" "$url"

    local actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
    if [ "$actual_sha256" != "$expected_sha256" ]; then
        echo "ERROR: sha256 mismatch for $filename after download"
        echo "Expected: $expected_sha256"
        echo "Actual:   $actual_sha256"
        return 1
    fi
    echo "Downloaded $filename (sha256 verified)"
}

download_tarball "https://cdn.kernel.org/pub/linux/kernel/firmware/linux-firmware-20241017.tar.xz" \
    "linux-firmware-20241017.tar.xz" \
    "a26c38ef5a83272f2b98ce8bf8ca1865a852a3deea49ce5a8dd804b914351273"

download_tarball "https://git.kernel.org/pub/scm/linux/kernel/git/wens/wireless-regdb.git/snapshot/wireless-regdb-master-2024-10-07.tar.gz" \
    "wireless-regdb-master-2024-10-07.tar.gz" \
    "e0b60c2947125ac81ef7818c5667ef93ef3a275c0e417cf36db6793a4c2ae415"

# Sprint B of the WiFi-on-Pizlix port: the WiFi USERSPACE stack, as regular
# projeny packages (packaged with `filc/projeny package`, each setting its
# checkout up first if needed; the URL+blake3 headers live in the .projeny
# files):
#
# 1. libnl 3.11.0 -- the netlink library wpa_supplicant's nl80211 driver
#    builds against (NetworkManager does NOT use libnl; it has its own
#    netlink stack in src/libnm-platform).
# 2. libndp 1.9 -- NetworkManager's unconditional IPv6 neighbor discovery
#    dependency (meson.build:257).
# 3. wpa_supplicant 2.11 -- the WiFi auth daemon NM D-Bus-activates
#    (fi.w1.wpa_supplicant1); the tarball is the whole hostap archive,
#    the chroot script builds the wpa_supplicant/ subdirectory.
# 4. NetworkManager 1.48.4 -- the network daemon behind the GNOME network
#    indicator (libnm + NM-1.0 typelib for gjs).
#
# The pizlonated-*.tar.gz outputs are copied into $LFS/sources by
# build_postlc11.sh and built by build_postlc11_sub2_chroot.sh through
# the per-package chroot scripts.
filc/projeny package projects/libnl.projeny projects/libnl/pizlonated-libnl.tar.gz
filc/projeny package projects/libndp.projeny projects/libndp/pizlonated-libndp.tar.gz
filc/projeny package projects/wpa_supplicant.projeny projects/wpa_supplicant/pizlonated-wpa_supplicant.tar.gz
filc/projeny package projects/networkmanager.projeny projects/networkmanager/pizlonated-networkmanager.tar.gz

ls -la pizlix/linux-firmware-20241017.tar.xz pizlix/wireless-regdb-master-2024-10-07.tar.gz \
       projects/libnl/pizlonated-libnl.tar.gz projects/libndp/pizlonated-libndp.tar.gz \
       projects/wpa_supplicant/pizlonated-wpa_supplicant.tar.gz \
       projects/networkmanager/pizlonated-networkmanager.tar.gz