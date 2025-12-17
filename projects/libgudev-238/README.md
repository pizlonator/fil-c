libgudev
========

This is libgudev, a library providing GObject bindings for libudev. It
used to be part of udev, then merged into systemd. It's now a project
on its own.

The official download locations are:
  http://download.gnome.org/sources/libgudev

The official web site is:
  https://gitlab.gnome.org/GNOME/libgudev/

Installation
------------

libgudev uses [meson](https://mesonbuild.com/) as its build system, so
[generic installation instructions](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project)
apply to installing libgudev.

libgudev requires `libudev` (part of systemd), glib, and pkg-config. Refer to
the `meson.build` file itself for up-to-date version requirements.

libgudev also optionally uses [umockdev](https://github.com/martinpitt/umockdev)
for its tests.

Bugs and patches
----------------

Bugs and patches can be contributed on [the project's GitLab page](https://gitlab.gnome.org/GNOME/libgudev/).

License
-------

libgudev, as a combined works, is released under the GNU Lesser General
Public License version 2.1 or later. See `COPYING` file for details.
