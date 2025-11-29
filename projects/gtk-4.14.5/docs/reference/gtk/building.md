Title: Building GTK
Slug: gtk-building

Before we get into the details of how to compile GTK, we should
mention that in many cases, binary packages of GTK prebuilt for
your operating system will be available, either from your
operating system vendor or from independent sources. If such a
set of packages is available, installing it will get you
programming with GTK much faster than building it yourself. In
fact, you may well already have GTK installed on your system already.

In order to build GTK, you will need *meson* installed on your
system. On Linux, and other UNIX-like operating systems, you will
also need *ninja*. This guide does not cover how to install these
two requirements, but you can refer to the
[Meson website](http://mesonbuild.com) for more information. The
[Ninja](https://ninja-build.org) build tool is also usable on
various operating systems, so we will refer to it in the examples.

If you are building GTK from a source distribution or from a Git
clone, you will need to use *meson* to configure the project. The
most commonly useful argument is the `--prefix` one, which determines
where the files will go once installed. To install GTK under a prefix
like `/opt/gtk` you would run Meson as:

```
meson setup --prefix /opt/gtk builddir
```

Meson will create the `builddir` directory and place all the build
artefacts there.

You can get a list of all available options for the build by
running `meson configure`.

After Meson successfully configured the build directory, you then
can run the build, using Ninja:

```
cd builddir
meson compile
meson install
```

If you don't have permission to write to the directory you are
installing in, you may have to change to root temporarily before
running `meson install`.

Several environment variables are useful to pass to set before
running *meson*. `CPPFLAGS` contains options to pass to the C
compiler, and is used to tell the compiler where to look for
include files. The `LDFLAGS` variable is used in a similar fashion
for the linker. Finally the `PKG_CONFIG_PATH` environment variable
contains a search path that `pkg-config` (see below) uses when
looking for files describing how to compile programs using different
libraries. If you were installing GTK and it's dependencies into
`/opt/gtk`, you might want to set these variables as:

```
CPPFLAGS="-I/opt/gtk/include"
LDFLAGS="-L/opt/gtk/lib"
PKG_CONFIG_PATH="/opt/gtk/lib/pkgconfig"
export CPPFLAGS LDFLAGS PKG_CONFIG_PATH
```

You may also need to set the `LD_LIBRARY_PATH` environment variable
so the systems dynamic linker can find the newly installed libraries,
and the `PATH` environment program so that utility binaries installed
by the various libraries will be found.

```
LD_LIBRARY_PATH="/opt/gtk/lib"
PATH="/opt/gtk/bin:$PATH"
export LD_LIBRARY_PATH PATH
```

## Build types

Meson has different build types, exposed by the `buildtype`
configuration option. GTK enables and disables functionality
depending on the build type used when calling *meson* to
configure the build.

### Debug builds

GTK will enable debugging code paths in both the `debug` and
`debugoptimized` build types. Builds with `buildtype` set to
`debug` will additionally enable consistency checks on the
internal state of the toolkit.

It is recommended to use the `debug` or `debugoptimized` build
types when developing GTK itself. Additionally, `debug` builds of
GTK are recommended for profiling and debugging GTK applications,
as they include additional validation of the internal state.

The `debugoptimized` build type is the default for GTK if no build
type is specified when calling *meson*.

### Release builds

The `release` build type will disable debugging code paths and
additional run time safeties, like checked casts for object
instances.

The `plain` build type provided by Meson should only be used when
packaging GTK, and it's expected that packagers will provide their
own compiler flags when building GTK. See the previous section for
the list of environment variables to be used to define compiler and
linker flags. Note that with the plain build type, you are also
responsible for controlling the debugging features of GTK with
`-DG_ENABLE_DEBUG` and `-DG_DISABLE_CAST_CHECKS`.

## Dependencies

Before you can compile GTK, you need to have various other tools and
libraries installed on your system. Dependencies of GTK have their own
build systems, so you will need to refer to their own installation
instructions.

A particular important tool used by GTK to find its dependencies
is `pkg-config`.

[pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
is a tool for tracking the compilation flags needed for libraries
that are used by the GTK libraries. (For each library, a small `.pc`
text file is installed in a standard location that contains the
compilation flags needed for that library along with version number
information.)

Some of the libraries that GTK depends on are maintained by the
GTK team: GLib, GdkPixbuf, Pango, and GObject Introspection.
Other libraries are maintained separately.

- The GLib library provides core non-graphical functionality
  such as high level data types, Unicode manipulation, and
  an object and type system to C programs. It is available
  from [here](https://download.gnome.org/sources/glib/).
- The [GdkPixbuf](https://git.gnome.org/browse/gdk-pixbuf/)
  library provides facilities for loading images in a variety of
  file formats. It is available [here](ttps://download.gnome.org/sources/gdk-pixbuf/).
- [Pango](http://www.pango.org) is a library for internationalized
  text handling. It is available [here](https://download.gnome.org/sources/pango/).
- [GObject Introspection](https://wiki.gnome.org/Projects/GObjectIntrospection)
  is a framework for making introspection data available to language
  bindings. It is available [here](https://download.gnome.org/sources/gobject-introspection/).
- The [GNU libiconv](https://www.gnu.org/software/libiconv/) library
  is needed to build GLib if your system doesn't have the iconv()
  function for doing conversion between character encodings. Most
  modern systems should have iconv().
- The libintl library from the [GNU gettext](https://www.gnu.org/software/gettext/)
  package is needed if your system doesn't have the gettext()
  functionality for handling message translation databases.
- The libraries from the X window system are needed to build
  Pango and GTK. You should already have these installed on
  your system, but it's possible that you'll need to install
  the development environment for these libraries that your
  operating system vendor provides.
- The [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/)
  library provides Pango with a standard way of locating fonts and matching
  them against font names.
- [Cairo](https://www.cairographics.org) is a graphics library that
  supports vector graphics and image compositing. Both Pango and GTK
  use Cairo for drawing. Note that we also need the auxiliary cairo-gobject
  library.
- [libepoxy](https://github.com/anholt/libepoxy) is a library that
  abstracts the differences between different OpenGL libraries. GTK
  uses it for cross-platform GL support and for its own drawing.
- [Graphene](http://ebassi.github.io/graphene/) is a library that
  provides vector and matrix types for 2D and 3D transformations.
  GTK uses it internally for drawing.
- The [Wayland](https://wayland.freedesktop.org) libraries are needed
  to build GTK with the Wayland backend.
- The [shared-mime-info](https://www.freedesktop.org/wiki/Software/shared-mime-info)
  package is not a hard dependency of GTK, but it contains definitions
  for mime types that are used by GIO and, indirectly, by GTK.
  gdk-pixbuf will use GIO for mime type detection if possible.
  For this to work, shared-mime-info needs to be installed and
  `XDG_DATA_DIRS` set accordingly at configure time. Otherwise,
  gdk-pixbuf falls back to its built-in mime type detection.

## Building and testing GTK

First make sure that you have the necessary external
dependencies installed: `pkg-config`, Meson, Ninja,
the JPEG, PNG, and TIFF libraries, FreeType, and, if necessary,
libiconv and libintl. To get detailed information about building
these packages, see the documentation provided with the
individual packages. On any average Linux system, it's quite likely
you'll have all of these installed already, or they will be easily
accessible through your operating system package repositories.

Then build and install the GTK libraries in the order:
GLib, Cairo, Pango, then GTK. For each library, follow the
instructions they provide, and make sure to share common settings
between them and the GTK build; if you are using a separate prefix
for GTK, for instance, you will need to use the same prefix for
all its dependencies you build. If you're lucky, this will all go
smoothly, and you'll be ready to [start compiling your own GTK
applications](#gtk-compiling). You can test your GTK installation
by running the `gtk4-demo` program that GTK installs.

If one of the projects you're configuring or building fails, look
closely at the error messages printed; these will often provide useful
information as to what went wrong. Every build system has its own
log that can help you understand the issue you're encountering. If
all else fails, you can ask for help on the
[GTK forums](#gtk-resources).

## Extra Configuration Options

In addition to the normal options provided by Meson, GTK defines various
arguments that modify what should be built. All of these options are passed
to `meson` as `-Doption=value`. Most of the time, the value can be `true` or
`false`, or `enabled`, `disabled` or `auto`.

To see a summary of all supported options and their allowed values, run
```
meson configure builddir
```

### `x11-backend`, `win32-backend`, `broadway-backend`, `wayland-backend` and `macos-backend`

Enable specific backends for GDK.  If none of these options are given, the
Wayland backend will be enabled by default, if the platform is Linux; the
X11 backend will also be enabled by default, unless the platform is Windows,
in which case the default is win32, or the platform is macOS, in which case
the default is macOS. If any backend is explicitly enabled or disabled, no
other platform will be enabled automatically.

### `vulkan`

By default, GTK will try to build with support for the Vulkan graphics
API in addition to cairo and OpenGL. This option can be used to explicitly
control whether Vulkan should be used.

### `media-gstreamer`

By default, GTK will try to build the gstreamer backend for
media playback support. These options can be used to explicitly
control which media backends should be built.

### `print-cups` and `print-cpdb`

By default, GTK will try to build the cups and file print backends
if their dependencies are found. These options can be used to
explicitly control which print backends should be built.

### `cloudproviders`

This option controls whether GTK should use libcloudproviders for
supporting various Cloud storage APIs in the file chooser.

### `sysprof`

This option controls whether GTK should include support for
tracing with sysprof.

### `tracker`

This option controls whether GTK should use Tracker for search
support in the file chooser.

### `colord`

This option controls whether GTK should use colord for color
calibration support in the cups print backend.

### `documentation`, `man-pages` and `screenshots`

The *gi-docgen* package is used to generate the reference documentation
included with GTK. By default support for *gi-docgen* is disabled
because it requires various extra dependencies to be installed.
Introspection needs to be enabled, since the documentation is generated
from introspection data.

### `introspection`

Allows to disable building introspection support. This is option
is mainly useful for shortening turnaround times on developer
systems. Installed builds of GTK should always have introspection
support.

### `build-testsuite`

If you want to run the testsuite to ensure that your GTK build
works, you should enable it with this option.

### `build-tests`, `build-examples`, `build-demos`

By default, GTK will build quite a few tests, examples and demos.
While these are useful on a developer system, they are not
needed when GTK is built e.g. for a flatpak runtime. These
options allow to disable building tests and demos.
