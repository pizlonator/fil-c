#!/bin/bash
set -e
set -x

# LibreOffice build script for Pizlix (Fil-C)
# It assumes all dependencies (abseil, boost, libxslt, X11 libs, etc.) are already installed

rm -rf libreoffice-24.8.0.3
tar -xf libreoffice-24.8.0.3.tar.xz
cd libreoffice-24.8.0.3

# Apply Fil-C patch (which includes BLFS changes)
patch -Np1 -i ../libreoffice-filc.patch

# Fix zlib linking bug (from BLFS)
sed -i '/icuuc \\/a zlib\\' writerperfect/Library_wpftdraw.mk

# Fix desktop integration (from BLFS)
sed -e "/gzip -f/d" \
    -e "s|.1.gz|.1|g" \
    -i bin/distro-install-desktop-integration

sed -e "/distro-install-file-lists/d" -i Makefile.in

# Set up external tarballs
install -dm755 external/tarballs
# The dictionaries/help/translations "submodule" tarballs live in /sources/
# (copied there by build_postlc5.sh).  Use absolute paths because the symlinks
# are created in external/tarballs/ — a relative "../" would only reach
# external/, not /sources/.
ln -sv /sources/libreoffice-dictionaries-24.8.0.3.tar.xz external/tarballs/
ln -sv /sources/libreoffice-help-24.8.0.3.tar.xz external/tarballs/
ln -sv /sources/libreoffice-translations-24.8.0.3.tar.xz external/tarballs/

# Symlink pre-downloaded external dependency tarballs.
# The chroot has no network access, so `make fetch` cannot download them.
# They were downloaded outside the chroot and copied to /sources/.
for f in /sources/libreoffice-external-tarballs/*; do
    ln -sv "$f" external/tarballs/
done

# Create symlinks for submodule directories (dictionaries, help, translations)
# These will be broken until unpack-sources runs during make build, but that's OK
ln -sv src/libreoffice-dictionaries-24.8.0.3/dictionaries/ dictionaries
ln -sv src/libreoffice-help-24.8.0.3/helpcontent2/ helpcontent2
ln -sv src/libreoffice-translations-24.8.0.3/translations/ translations

# Fix sha256sum (Fil-C's shasum crashes)
sed -i 's/shasum -a 256/sha256sum/g' configure.ac || true

# Configure
./autogen.sh $(cat ../libreoffice-filc.autogen.flags | tr '\n' ' ')

# Skip the root check - the pizlix chroot runs everything as root, but
# LibreOffice refuses to build as root (check-if-root target). Remove the
# check-if-root prerequisite from the bootstrap target so the build proceeds.
# Note: the pattern must include a trailing space ("check-if-root ") so it
# matches the prerequisite on the "bootstrap: check-if-root compilerplugins"
# line (where the word is followed by a space), NOT the target definition
# "check-if-root:" (followed by a colon). This leaves the check-if-root target
# intact but simply unreferenced.
sed -i 's/check-if-root //' Makefile

# Build
make build -j$(nproc)

# Create empty nss.filelist before install (Fil-C: nss package list may be missing)
mkdir -p workdir/Package && touch workdir/Package/nss.filelist

# Install
make distro-pack-install

cd ..
rm -rf libreoffice-24.8.0.3
