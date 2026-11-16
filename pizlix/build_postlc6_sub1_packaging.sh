#!/bin/bash

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/projects

test $EUID -eq `stat -c %u $FILCSRC`

cd $FILCSRC

rm -vf projects/*/pizlonated-*.tar.gz
./package-source.sh projects/abseil-cpp-20260107.1 pizlonated-abseil
./package-source.sh projects/cairo-1.18.0 pizlonated-cairo
./package-source.sh projects/libxslt-1.1.42 pizlonated-libxslt
./package-source.sh projects/zip-3.0 pizlonated-zip
./package-source.sh projects/unzip-6.0 pizlonated-unzip

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

download_tarball "https://archives.boost.org/release/1.86.0/source/boost_1_86_0.tar.bz2" \
    "boost_1_86_0.tar.bz2" \
    "1bed88e40401b2cb7a1f76d4bab499e352fa4d0c5f31c0dbae64e24d34d7513b"

download_tarball "https://download.documentfoundation.org/libreoffice/src/24.8.0/libreoffice-24.8.0.3.tar.xz" \
    "libreoffice-24.8.0.3.tar.xz" \
    "5b11468cd1b68c05c33b151fcd7d044eea0c7e1dbf4bda028b490e18df7d78c1"

download_tarball "https://download.documentfoundation.org/libreoffice/src/24.8.0/libreoffice-dictionaries-24.8.0.3.tar.xz" \
    "libreoffice-dictionaries-24.8.0.3.tar.xz" \
    "b4e9a20e94b96179e46648dc565bde1b80511c15ad8f95287574058bbd97bb9a"

download_tarball "https://download.documentfoundation.org/libreoffice/src/24.8.0/libreoffice-help-24.8.0.3.tar.xz" \
    "libreoffice-help-24.8.0.3.tar.xz" \
    "7653e34fa2139fa6818d644208a0fc9e4e43be18d669c020f38c8dab161671ed"

download_tarball "https://download.documentfoundation.org/libreoffice/src/24.8.0/libreoffice-translations-24.8.0.3.tar.xz" \
    "libreoffice-translations-24.8.0.3.tar.xz" \
    "5e2706a6b0339b3424a3fb75c83b73817603722baf8fa11a9b84dc9a65ece55c"

# Download LibreOffice external dependency tarballs.
# The chroot has no network access, so `make fetch` cannot download these
# ~136 tarballs inside the chroot.  Download them here (outside the chroot);
# they are copied into the chroot and symlinked into external/tarballs/ by
# the LO build script.
./pizlix/download_libreoffice_external_tarballs.sh
