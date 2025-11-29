#!/usr/bin/env bash

set -ex

# When changing this file, you need to bump the following
# .gitlab-ci/image-tags.yml tags:
# DEBIAN_BASE_TAG
# DEBIAN_BUILD_TAG
# FEDORA_X86_64_BUILD_TAG
# KERNEL_ROOTFS_TAG

MOLD_VERSION="2.4.1"

git clone -b v"$MOLD_VERSION" --single-branch --depth 1 https://github.com/rui314/mold.git
pushd mold

cmake -DCMAKE_BUILD_TYPE=Release -D BUILD_TESTING=OFF -D MOLD_LTO=ON
cmake --build . --parallel
cmake --install .

popd
rm -rf mold
