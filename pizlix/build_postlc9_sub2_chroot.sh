#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources


./build_postlc9_chroot_project_rust.sh
hash -r
./build_postlc9_chroot_project_rust_dispatch.sh

./build_postlc9_chroot_project_librsvg.sh
hash -r