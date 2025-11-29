#!/usr/bin/env bash

# When changing this file, you need to bump the following
# .gitlab-ci/image-tags.yml tags:
# DEBIAN_X86_64_TEST_GL_TAG

set -ex -o pipefail

### Careful editing anything below this line

git config --global user.email "mesa@example.com"
git config --global user.name "Mesa CI"
git clone https://github.com/axeldavy/Xnine.git /Xnine
mkdir /Xnine/build
pushd /Xnine/build
git checkout c64753d224c08006bcdcfa7880ada826f27164b1

cmake .. -DBUILD_TESTS=1 -DWITH_DRI3=1 -DD3DADAPTER9_LOCATION=/install/lib/d3d/d3dadapter9.so
make

mkdir -p /NineTests/
mv NineTests/NineTests /NineTests/

popd
rm -rf /Xnine
