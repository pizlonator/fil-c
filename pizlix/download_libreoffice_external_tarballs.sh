#!/bin/bash
#
# Downloads all external dependency tarballs listed in LibreOffice's download.lst.
#
# The pizlix build chroot has no network access, so `make fetch` cannot download
# these ~136 tarballs inside the chroot.  Instead, this script is run OUTSIDE the
# chroot (called from build_postlc5_sub1_packaging.sh) and the resulting files
# are copied into the chroot and symlinked into external/tarballs/ by the LO
# build script.
#
# The tarball list (names + SHA256 hashes) is extracted directly from the
# LibreOffice source tarball's download.lst, and variable references such as
# $(CAIRO_VERSION_MICRO) are resolved with `make`.  This means the script stays
# correct even if download.lst is updated in a future LO version.
#
# Most tarballs are fetched from https://dev-www.libreoffice.org/src/
# A few (fonts/jars with _TTF or _JAR suffix) come from
# https://dev-www.libreoffice.org/extern/
#
# Already-downloaded files with a matching SHA256 are skipped.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARBALL_DIR="$SCRIPT_DIR/libreoffice-external-tarballs"
LO_TARBALL="$SCRIPT_DIR/libreoffice-24.8.0.3.tar.xz"

test -f "$LO_TARBALL"
mkdir -p "$TARBALL_DIR"

# ---------------------------------------------------------------------------
# Extract download.lst from the LibreOffice source tarball.
# ---------------------------------------------------------------------------
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT
tar -xf "$LO_TARBALL" -O libreoffice-24.8.0.3/download.lst > "$WORKDIR/download.lst"

# ---------------------------------------------------------------------------
# Generate a Makefile that prints "url-base  filename  sha256" for every
# file variable (ending in _TARBALL, _TTF, or _JAR) defined in download.lst.
# Using `make` resolves $(VAR) references automatically.
# ---------------------------------------------------------------------------
{
    echo "include download.lst"
    echo "print:"
    grep -oE '^[A-Z0-9_]+_(TARBALL|TTF|JAR) ' "$WORKDIR/download.lst" \
        | awk '{print $1}' | sort -u | while read -r var; do
        base="${var%_*}"          # e.g. CAIRO from CAIRO_TARBALL
        suffix="${var##*_}"        # e.g. TARBALL
        sha_var="${base}_SHA256SUM"
        if [ "$suffix" = "TARBALL" ]; then
            url_base="src"
        else
            url_base="extern"
        fi
        printf '\t@echo "%s" "$(%s)" "$(%s)"\n' "$url_base" "$var" "$sha_var"
    done
} > "$WORKDIR/Makefile"

make -C "$WORKDIR" -s print > "$WORKDIR/tarball_list.txt"

# ---------------------------------------------------------------------------
# Download helper: download with SHA256 verification, skip if already present.
# ---------------------------------------------------------------------------
download_tarball() {
    local url="$1"
    local filename="$2"
    local expected_sha256="$3"
    local filepath="$TARBALL_DIR/$filename"

    if [ -f "$filepath" ]; then
        local actual_sha256
        actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
        if [ "$actual_sha256" = "$expected_sha256" ]; then
            echo "Already have $filename (sha256 verified)"
            return 0
        fi
        echo "sha256 mismatch for $filename, re-downloading"
        rm -f "$filepath"
    fi

    echo "Downloading $filename from $url"
    # Same hardening as download_tarball in build_postlc5_sub1_packaging.sh:
    # plain --retry does not retry errors like curl 92 ("HTTP/2 stream not
    # closed cleanly"), and without --continue-at - a retry would restart
    # from byte 0.  This is ~136 tarballs (~500MB) -- don't let one flake
    # kill the build.
    curl -L -f --retry 5 --retry-all-errors --continue-at - -o "$filepath" "$url"

    local actual_sha256
    actual_sha256=$(sha256sum "$filepath" | cut -d' ' -f1)
    if [ "$actual_sha256" != "$expected_sha256" ]; then
        echo "ERROR: sha256 mismatch for $filename after download"
        echo "Expected: $expected_sha256"
        echo "Actual:   $actual_sha256"
        return 1
    fi
    echo "Downloaded $filename (sha256 verified)"
}

# ---------------------------------------------------------------------------
# Download every tarball in the list.
# ---------------------------------------------------------------------------
total=0
ok=0
fail=0
while read -r url_base filename expected_sha256; do
    [ -z "$filename" ] && continue
    [ -z "$expected_sha256" ] && continue
    total=$((total + 1))
    if download_tarball \
        "https://dev-www.libreoffice.org/$url_base/$filename" \
        "$filename" "$expected_sha256"; then
        ok=$((ok + 1))
    else
        fail=$((fail + 1))
    fi
done < "$WORKDIR/tarball_list.txt"

echo "============================================"
echo "External tarballs: $total total, $ok ok, $fail failed"
echo "Stored in: $TARBALL_DIR"
echo "============================================"

if [ "$fail" -gt 0 ]; then
    echo "ERROR: $fail tarballs failed to download"
    exit 1
fi
