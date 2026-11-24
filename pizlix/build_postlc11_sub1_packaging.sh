#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

# The postlc11 stage packaging.
#
# There are no new projeny packages here (the WiFi
# userspace stack packages live in postlc7) -- what this stage
# needs is the 379MB FIRMWARE tarball (too big to commit to git; see #1)
# plus the wireless-regdb
# projeny package (#2; the git-tracked tarball that used to cover it is
# gone):
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
# 2. wireless-regdb-master-2024-10-07: the regulatory database from the
#    wens repo (same date as the firmware snapshot).  The kernel has
#    CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y + CONFIG_CFG80211_USE_KERNEL_
#    REGDB_KEYS=y, and 6.10.5 embeds BOTH sforshee.hex and wens.hex as
#    trusted regdb signing keys (net/wireless/certs/) -- the wens repo's
#    prebuilt regulatory.db is wens-signed, so it validates against the
#    kernel's key.  linux-firmware does NOT ship regulatory.db.
#    Since the 2026-11-22 projeny conversion this one is NOT downloaded
#    and cached here anymore: it is the projeny package
#    projects/wireless-regdb.projeny (URL-mode: the git.kernel.org
#    snapshot URL + blake3, no tarball in git), packaged below so
#    build_postlc11.sh can stage pizlonated-wireless-regdb.tar.gz into
#    /sources for the chroot firmware script.  projeny verifies the
#    blake3 of whatever it fetches (and it sends a User-Agent since the
#    2026-11-22 projeny fix -- git.kernel.org 403s UA-less clients).
#
# The download_tarball helper is the postlc6 one (sha256-verified, with
# --retry/--retry-all-errors/--continue-at so a flaky CDN does not kill a
# multi-hour one-shot build; it caches the tarball in pizlix/ so repeated
# stage runs do not re-download).  build_postlc11.sh copies the linux-
# firmware tarball from pizlix/ into $LFS/sources after this script
# succeeds, and copies the wireless-regdb projeny package out of
# projects/ alongside it.

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

# The wireless-regdb projeny package (URL-mode; the git.kernel.org snapshot
# URL and its blake3 live in the .projeny file).  projeny auto-setups the
# checkout on a fresh clone (download + blake3 verify + unpack), so the
# pizlonated tarball builds even where pizlix/ has no cached copy.

filc/projeny package projects/wireless-regdb.projeny projects/wireless-regdb/pizlonated-wireless-regdb.tar.gz

# The WiFi USERSPACE stack packages
# (libnl / libndp / wpa_supplicant / NetworkManager) moved to the postlc7
# stage (packaging AND chroot build) -- gnome-control-center (postlc7)
# links libnm + libnma-gtk4 and gnome-shell (postlc8) loads the NMA4
# typelib, so the stack must be installed before those stages run; see
# build_postlc7_sub1_packaging.sh / build_postlc7_sub2_chroot.sh.  libnma
# 1.10.6 (the NMA-1.0/NMA4-1.0 UI libs the two consumers use) is packaged
# there too.  What REMAINS in the postlc11 stage: the kernel rebuild with
# CONFIG_MAC80211_HWSIM=m, the iwlwifi firmware subset + the wens-signed
# regulatory db (runtime data -- any stage works for those, they stay
# here for historical reasons), and the networkmanager boot wiring setup
# script (S33networkmanager, run at the end of sub2 -- the full chain
# reaches postlc11 after postlc7/postlc8).

ls -la pizlix/linux-firmware-20241017.tar.xz \
       projects/wireless-regdb/pizlonated-wireless-regdb.tar.gz