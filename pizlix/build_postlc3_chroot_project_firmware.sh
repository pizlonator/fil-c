#!/bin/bash

set -e
set -x

ulimit -c unlimited

# The firmware.
#
# /lib/firmware is what the kernel's request_firmware() serves from (the
# iwlwifi driver walks fw_name_pre-api.ucode FLAT in /lib/firmware -- see
# pizlonated-linux/drivers/net/wireless/intel/iwlwifi/iwl-drv.c:246-263, no
# subdirectory, walking DOWN from ucode_api_max to ucode_api_min per
# family).  Two sources:
#
# 1. linux-firmware-20241017 (the LFS-12.2-era snapshot): the ucode files
#    are FLAT in the tarball root.  We do NOT install all 379MB of it --
#    this script is MANIFEST-DRIVEN: it reads the kernel's own
#    /lib/modules/6.10.5/modules.builtin.modinfo (installed by the kernel
#    build; 57 iwlwifi-*.ucode names = the MODULE_FIRMWARE() declarations of
#    the built-in drivers), strips the trailing -api.ucode to get the family
#    prefixes (iwlwifi-cc-a0-, iwlwifi-so-a0-gf-a0-, ...), and installs every
#    ucode/pnvm file in the tarball matching any of those prefixes.  This is
#    future-proof: a kernel config change that adds/removes a built-in WiFi
#    driver automatically widens/narrows the firmware set, and every API
#    version of a family comes along (the driver walks api_max..api_min).
#    Plus LICENCE.iwlwifi_firmware (the redistribution requirement).
# 2. wireless-regdb-master-2024-10-07: regulatory.db + regulatory.db.p7s,
#    FLAT in /lib/firmware.  REQUIRED: CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y
#    + CONFIG_CFG80211_USE_KERNEL_REGDB_KEYS=y mean cfg80211 wants this db at
#    runtime, signature checked against the keys embedded in the kernel
#    (net/wireless/certs/sforshee.hex + wens.hex in 6.10.5); the wens repo
#    snapshot is wens-signed, so it validates.  Since the 2026-11-22 projeny
#    conversion the regdb comes from pizlonated-wireless-regdb.tar.gz (the
#    pizlonated-wireless-regdb/ checkout of projects/wireless-regdb.projeny;
#    the git-tracked pizlix/wireless-regdb-master-2024-10-07.tar.gz is
#    gone) -- same bytes, see the projeny file's blake3.
#
# The inputs are staged in /sources by build_postlc3.sh: the linux-firmware
# tarball (downloaded by build_postlc3_sub1_packaging.sh with sha256
# verification) and pizlonated-wireless-regdb.tar.gz (packaged by
# build_postlc3_sub1_packaging.sh from projects/wireless-regdb.projeny,
# whose blake3 pins the exact snapshot bytes).  The extracted trees are
# deleted after the install so the stage snapshot stays small (the tarballs
# stay in /sources; the installed subset is ~195MB).

cd /sources
rm -rf linux-firmware-20241017 pizlonated-wireless-regdb

tar -xf linux-firmware-20241017.tar.xz
tar -xf pizlonated-wireless-regdb.tar.gz

test -d linux-firmware-20241017
test -d pizlonated-wireless-regdb
test -e pizlonated-wireless-regdb/regulatory.db
test -e pizlonated-wireless-regdb/regulatory.db.p7s
test -e /lib/modules/6.10.5/modules.builtin.modinfo

# --- the manifest-driven subset ---------------------------------------------

# 1. the family prefixes from the kernel's own manifest
tr '\0' '\n' < /lib/modules/6.10.5/modules.builtin.modinfo \
    | grep -o 'iwlwifi-[A-Za-z0-9-]*\.ucode' \
    | sort -u \
    | sed 's/-[0-9][0-9]*\.ucode$/-/' \
    | sort -u > /sources/iwlwifi-prefixes.txt

echo "=== the family prefixes (one per built-in iwlwifi/dvm/mvm driver flavor) ==="
wc -l /sources/iwlwifi-prefixes.txt
cat /sources/iwlwifi-prefixes.txt

mkdir -v -p /lib/firmware
cp -v -a linux-firmware-20241017/LICENCE.iwlwifi_firmware /lib/firmware/

# 2. every ucode + pnvm file of those families (all API versions: the
#    driver walks fw_name_pre-api.ucode from api_max down to api_min).
#    pnvm files have NO api suffix in their names (iwlwifi-so-a0-gf-a0.pnvm,
#    not iwlwifi-so-a0-gf-a0-84.pnvm), so the pnvm glob uses the family stem
#    without the trailing dash.
UCODES=0
PNVM=0
UNCOVERED=""
while read -r prefix
do
    stem=${prefix%-}
    if cp -a linux-firmware-20241017/${prefix}*.ucode /lib/firmware/ 2>/dev/null
    then
        UCODES=$((UCODES+1))
    else
        # Published linux-firmware snapshots have no files at all for the
        # never-released a0-stepping Wi-Fi 7 families (bz-/sc-/ma-a0, the
        # -90 API steps) and for the steppings 6.10.5's driver tables do
        # not declare (so-a0-gf4-a0 shares the gf-a0 ucode; Qu-c0-jf-b0,
        # bz-b0); zero files for such a prefix is expected, not fatal.
        UNCOVERED="$UNCOVERED $prefix"
    fi
    if cp -a linux-firmware-20241017/${stem}*.pnvm /lib/firmware/ 2>/dev/null
    then
        PNVM=$((PNVM+1))
    fi
done < /sources/iwlwifi-prefixes.txt

# 3. the regulatory database (wens-signed; the kernel's embedded wens.hex
#    key validates it at reg-set time)
cp -v -a pizlonated-wireless-regdb/regulatory.db /lib/firmware/
cp -v -a pizlonated-wireless-regdb/regulatory.db.p7s /lib/firmware/

# --- gates --------------------------------------------------------------------

echo "=== gate: counts ==="
echo "families with ucode files installed: $UCODES of $(wc -l < /sources/iwlwifi-prefixes.txt)"
echo "families with pnvm files installed:  $PNVM"
if test "x$UNCOVERED" != x
then
    echo "families with NO published firmware (expected for a0-stepping Wi-Fi 7):$UNCOVERED"
fi
echo "ucode files:    `ls /lib/firmware/iwlwifi-*.ucode | wc -l`"
echo "pnvm files:     `ls /lib/firmware/iwlwifi-*.pnvm | wc -l`"
# linux-firmware-20241017 carries 143 ucode + 7 pnvm iwlwifi files; the
# manifest-driven selection installs exactly the families the built-in
# drivers declare: 121 ucode files across 35 families (the tarball's other
# 22 ucode files belong to families the 6.10.5 manifest does not declare --
# iwlegacy 3945/4965, Qu-c0-jf-b0, bz-b0-fm-c0, and so-a0-gf4-a0, which
# shares the gf-a0 ucode) plus 5 pnvm files (bz-b0-fm-c0.pnvm and
# so-a0-gf4-a0.pnvm are for undeclared families).  The gates have slack so
# a kernel config change that adds a driver widens the install instead of
# failing the stage.
test `ls /lib/firmware/iwlwifi-*.ucode | wc -l` -ge 115
test `ls /lib/firmware/iwlwifi-*.pnvm | wc -l` -ge 5
test "$UCODES" -ge 30
test "$PNVM" -ge 4

echo "=== gate: the regulatory db ==="
ls -la /lib/firmware/regulatory.db /lib/firmware/regulatory.db.p7s
test -e /lib/firmware/regulatory.db
test -e /lib/firmware/regulatory.db.p7s
head -c 2 /lib/firmware/regulatory.db; echo " <- regdb magic (op)"; true

echo "=== gate: the AX200/AX210-era families, spot check ==="
test -e /lib/firmware/iwlwifi-cc-a0-50.ucode
test -e /lib/firmware/iwlwifi-so-a0-jf-b0-77.ucode
test -e /lib/firmware/iwlwifi-so-a0-hr-b0-89.ucode
test -e /lib/firmware/iwlwifi-so-a0-gf-a0-89.ucode
test -e /lib/firmware/iwlwifi-gl-c0-fm-c0-90.ucode
test -e /lib/firmware/iwlwifi-QuZ-a0-hr-b0-77.ucode

echo "=== gate: total size ==="
du -sm /lib/firmware
# ~166MB: 121 ucode + 5 pnvm; the ~195MB estimate covered the full
# 150-file iwlwifi set, 22+2 files of which belong to families the kernel
# manifest does not declare (see the counts gate above).
test `du -sm /lib/firmware | cut -f1` -ge 150

echo "=== gate: every file is world-readable (request_firmware runs as the loading task) ==="
find /lib/firmware -name 'iwlwifi-*' ! -perm -004 | head
test `find /lib/firmware -name 'iwlwifi-*' ! -perm -004 | wc -l` -eq 0

# --- cleanup (keep the snapshot small) ----------------------------------------

cd /sources
rm -rf linux-firmware-20241017 pizlonated-wireless-regdb /sources/iwlwifi-prefixes.txt
hash -r

echo "postlc3 firmware: ALL GATES GREEN"