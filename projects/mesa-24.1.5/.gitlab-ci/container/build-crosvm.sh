#!/usr/bin/env bash
# shellcheck disable=SC2086 # we want word splitting

set -ex

git config --global user.email "mesa@example.com"
git config --global user.name "Mesa CI"

CROSVM_VERSION=1641c55bcc922588e24de73e9cca7b5e4005bd6d
git clone --single-branch -b main --no-checkout https://chromium.googlesource.com/crosvm/crosvm /platform/crosvm
pushd /platform/crosvm
git checkout "$CROSVM_VERSION"
git submodule update --init

VIRGLRENDERER_VERSION=d9c002fac153b834a2c17731f2b85c36e333e102
rm -rf third_party/virglrenderer
git clone --single-branch -b main --no-checkout https://gitlab.freedesktop.org/virgl/virglrenderer.git third_party/virglrenderer
pushd third_party/virglrenderer
git checkout "$VIRGLRENDERER_VERSION"
meson setup build/ -D libdir=lib -D render-server-worker=process -D venus=true $EXTRA_MESON_ARGS
meson install -C build
popd

cargo update -p pkg-config@0.3.26 --precise 0.3.27

RUSTFLAGS='-L native=/usr/local/lib' cargo install \
  bindgen-cli \
  --locked \
  -j ${FDO_CI_CONCURRENT:-4} \
  --root /usr/local \
  --version 0.65.1 \
  $EXTRA_CARGO_ARGS

CROSVM_USE_SYSTEM_MINIGBM=1 CROSVM_USE_SYSTEM_VIRGLRENDERER=1 RUSTFLAGS='-L native=/usr/local/lib' cargo install \
  -j ${FDO_CI_CONCURRENT:-4} \
  --locked \
  --features 'default-no-sandbox gpu x virgl_renderer' \
  --path . \
  --root /usr/local \
  $EXTRA_CARGO_ARGS

popd

rm -rf /platform/crosvm
