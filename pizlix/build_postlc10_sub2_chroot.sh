#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

./build_postlc10_chroot_project_webkitgtk.sh
hash -r

./build_postlc10_chroot_project_desktop_file_utils.sh
hash -r
./build_postlc10_chroot_project_libportal.sh
hash -r
./build_postlc10_chroot_project_gnutls_trust.sh
hash -r
./build_postlc10_chroot_project_epiphany.sh
hash -r
