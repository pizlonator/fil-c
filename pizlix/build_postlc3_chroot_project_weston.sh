#!/bin/bash

set -e
set -x

rm -rf pizlonated-weston
tar -xf pizlonated-weston.tar.gz
cd pizlonated-weston
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D backend-pipewire=false \
      -D backend-rdp=false \
      -D backend-vnc=false \
      -D backend-x11=false \
      -D backend-default=drm \
      -D renderer-gl=false \
      -D xwayland=false \
      -D remoting=false \
      -D pipewire=false \
      -D image-jpeg=false \
      -D image-webp=false \
      -D color-management-lcms=false \
      -D backend-drm-screencast-vaapi=false \
      -D systemd=false \
      -D simple-clients=damage,im,shm \
      -D demo-clients=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-weston
