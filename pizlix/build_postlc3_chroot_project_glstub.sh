#!/bin/bash
set -e
set -x
# Build the GL stub library for Wayland-only systems
# LibreOffice needs libGL for its configure check, but we don't have real OpenGL
gcc -shared -o /usr/lib/libGL.so.1 -fPIC gl_stub.c
ln -sf libGL.so.1 /usr/lib/libGL.so
# Install pkg-config file
cat > /usr/lib/pkgconfig/gl.pc << 'EOF'
prefix=/usr
libdir=${prefix}/lib
includedir=${prefix}/include

Name: gl
Description: Stub libGL for Wayland-only systems
Version: 1.4
Libs: -L${libdir} -lGL
Cflags: -I${includedir}
EOF
