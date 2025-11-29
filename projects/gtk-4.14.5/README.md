GTK — The GTK toolkit
=====================

[![Build status](https://gitlab.gnome.org/GNOME/gtk/badges/main/pipeline.svg)](https://gitlab.gnome.org/GNOME/gtk/-/commits/main)

General information
-------------------

GTK is a multi-platform toolkit for creating graphical user interfaces.
Offering a complete set of widgets, GTK is suitable for projects ranging
from small one-off projects to complete application suites.

GTK is a free and open-source software project. The licensing terms
for GTK, the GNU LGPL, allow it to be used by all developers, including those
developing proprietary software, without any license fees or royalties.

GTK is hosted by the GNOME project (thanks!) and used by a wide variety
of applications and projects.

The official download location

  - https://download.gnome.org/sources/gtk/

The official web site

  - https://www.gtk.org

The official developers blog

  - https://blog.gtk.org

Discussion forum

  - https://discourse.gnome.org/c/platform/core/

Nightly documentation can be found at
  - Gtk: https://gnome.pages.gitlab.gnome.org/gtk/gtk4/
  - Gdk: https://gnome.pages.gitlab.gnome.org/gtk/gdk4/
  - Gsk: https://gnome.pages.gitlab.gnome.org/gtk/gsk4/

Nightly flatpaks of our demos can be installed from the
[GNOME Nightly](https://wiki.gnome.org/Apps/Nightly) repository:
  - `flatpak remote-add --if-not-exists gnome-nightly https://nightly.gnome.org/gnome-nightly.flatpakrepo`
  - `flatpak install gnome-nightly org.gtk.Demo4`
  - `flatpak install gnome-nightly org.gtk.WidgetFactory4`
  - `flatpak install gnome-nightly org.gtk.IconBrowser4`

Building and installing
-----------------------

In order to build GTK you will need:

  - [a C99 compatible compiler](https://wiki.gnome.org/Projects/GLib/CompilerRequirements)
  - [Python 3](https://www.python.org/)
  - [Meson](http://mesonbuild.com)
  - [Ninja](https://ninja-build.org)

You will also need various dependencies, based on the platform you are
building for:

  - [GLib](https://download.gnome.org/sources/glib/)
  - [GdkPixbuf](https://download.gnome.org/sources/gdk-pixbuf/)
  - [GObject-Introspection](https://download.gnome.org/sources/gobject-introspection/)
  - [Cairo](https://www.cairographics.org/)
  - [Pango](https://download.gnome.org/sources/pango/)
  - [Epoxy](https://github.com/anholt/libepoxy)
  - [Graphene](https://github.com/ebassi/graphene)
  - [Xkb-common](https://github.com/xkbcommon/libxkbcommon)

If you are building the Wayland backend, you will also need:

  - Wayland-client
  - Wayland-protocols
  - Wayland-cursor
  - Wayland-EGL

If you are building the X11 backend, you will also need:

  - Xlib, and the following X extensions:
    - xrandr
    - xrender
    - xi
    - xext
    - xfixes
    - xcursor
    - xdamage
    - xcomposite

Once you have all the necessary dependencies, you can build GTK by using
Meson:

```sh
$ meson setup _build
$ meson compile -C_build
```

You can run the test suite using:

```sh
$ meson test -C_build
```

And, finally, you can install GTK using:

```
$ sudo meson install -C_build
```

Complete information about installing GTK and related libraries
can be found in the file:

```
docs/reference/gtk/html/gtk-building.html
```

Or [online](https://docs.gtk.org/gtk4/building.html)

Building from git
-----------------

The GTK sources are hosted on [gitlab.gnome.org](http://gitlab.gnome.org). The main
development branch is called `main`, and stable branches are named after their minor
version, for example `gtk-4-10`.

How to report bugs
------------------

Bugs should be reported on the [issues page](https://gitlab.gnome.org/GNOME/gtk/issues/).

In the bug report please include:

* Information about your system. For instance:

   - which version of GTK you are using
   - what operating system and version
   - for Linux, which distribution
   - if you built GTK, the list of options used to configure the build

  And anything else you think is relevant.

* How to reproduce the bug.

  If you can reproduce it with one of the demo applications that are
  built in the demos/ subdirectory, on one of the test programs that
  are built in the tests/ subdirectory, that will be most convenient.
  Otherwise, please include a short test program that exhibits the
  behavior. As a last resort, you can also provide a pointer to a
  larger piece of software that can be downloaded.

* If the bug was a crash, the exact text that was printed out
  when the crash occurred.

* Further information such as stack traces may be useful, but
  is not necessary.

Contributing to GTK
-------------------

Please, follow the [contribution guide](./CONTRIBUTING.md) to know how to
start contributing to GTK.

If you want to support GTK financially, please consider donating to
the GNOME project, which runs the infrastructure hosting GTK.

Release notes
-------------

The release notes for GTK are part of the migration guide in the API
reference. See:

 - [3.x release notes](https://developer.gnome.org/gtk3/stable/gtk-migrating-2-to-3.html)
 - [4.x release notes](https://docs.gtk.org/gtk4/migrating-3to4.html)

Licensing terms
---------------

GTK is released under the terms of the GNU Lesser General Public License,
version 2.1 or, at your option, any later version, as published by the Free
Software Foundation.

Please, see the [`COPYING`](./COPYING) file for further information.

GTK includes a small number of source files under the Apache license:
- A fork of the roaring bitmaps implementation in [gtk/roaring](./gtk/roaring)
- An adaptation of timsort from python in [gtk/timsort](./gtk/timsort)
