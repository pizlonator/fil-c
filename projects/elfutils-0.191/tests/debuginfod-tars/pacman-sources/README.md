# Creating sample pacman packages

You will need:
- an archlinux installation
- `pacman -S base-devel` for basic build requirements (like debian "build-essential")

Run `makepkg` inside this directory to produce *.pkg.tar.* archives. This may
be .xz, .zst, .gz etc. depending on the current defaults, see makepkg.conf(5)
"PKGEXT" for details. The archives will appear in the current directory, or the
"PKGDEST" defined by makepkg.conf(5).

# Byte-for-byte reproduction

You will need:
- an archlinux installation
- `pacman -S devtools` for the clean chroot builder/reproducer

Run `makerepropkg /path/to/hello-debug-1-1-x86_64.pkg.tar.xz` (or whichever the
archive filename is) inside this directory.
