#!/bin/bash

set -e

# Disable slow diskspace check
sed -i 's/^CheckSpace/#CheckSpace/g' /etc/pacman.conf

# Update everything
pacman --noconfirm -Syyu

# Remove ADA and ObjC packages breaking the depenencies
pacman -R --noconfirm mingw-w64-i686-gcc-ada mingw-w64-i686-gcc-objc || true;
pacman -R --noconfirm mingw-w64-x86_64-gcc-ada mingw-w64-x86_64-gcc-objc || true;
