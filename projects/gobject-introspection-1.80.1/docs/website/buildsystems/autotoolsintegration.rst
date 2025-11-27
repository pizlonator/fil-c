=====================
Autotools Integration
=====================

The gobject-introspection package provides the following two macros for use in
your configure.ac file:

GOBJECT_INTROSPECTION_CHECK([version])
  This macro adds a ``--enable-introspection=yes|no|auto`` configure
  option which defaults to ``auto``.

  * ``auto`` - Will set ``HAVE_INTROSPECTION`` if gobject-introspection is available
    and the version requirement is satisfied.
  * ``yes`` - Will error out if gobject-introspection is missing or the version
    requirement is not satisfied. ``HAVE_INTROSPECTION`` will always be true.
  * ``no`` - Will never error out and ``HAVE_INTROSPECTION`` will always be false.

GOBJECT_INTROSPECTION_REQUIRE([version])
    This macro does not add a configure option and behaves as if
    ``--enable-introspection=yes``.


Example
-------

You'll need to adapt this for the library you're adding introspection support
to.

* configure.ac (or configure.in if no .ac file is used)

  .. code-block:: none

     GOBJECT_INTROSPECTION_CHECK([1.40.0])

* Makefile.am

  .. code-block:: none

    DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

  or just add to the existing DISTCHECK_CONFIGURE_FLAGS 

* foo/Makefile.am - must be near the end (after CLEANFILES has been set)

  .. code-block:: none

    -include $(INTROSPECTION_MAKEFILE)
    INTROSPECTION_GIRS =
    INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
    INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

    if HAVE_INTROSPECTION
    introspection_sources = $(libfoo_1_0_la_SOURCES)

    Foo-1.0.gir: libfoo-1.0.la
    Foo_1_0_gir_INCLUDES = GObject-2.0
    Foo_1_0_gir_CFLAGS = $(INCLUDES)
    Foo_1_0_gir_LIBS = libfoo-1.0.la
    Foo_1_0_gir_FILES = $(introspection_sources)
    INTROSPECTION_GIRS += Foo-1.0.gir

    girdir = $(datadir)/gir-1.0
    gir_DATA = $(INTROSPECTION_GIRS)

    typelibdir = $(libdir)/girepository-1.0
    typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

    CLEANFILES += $(gir_DATA) $(typelib_DATA)
    endif

  You can also check out a complete example at
  https://gitlab.gnome.org/GNOME/gtk/blob/c0ba041c73214f82d2c32b2ca1fa8f3c388c6170/gtk/Makefile.am#L1571


Makefile variable documentation
-------------------------------

``INTROSPECTION_GIRS`` is the entry point, you should list all the gir files
you want to build there in the XXX-Y.gir format where X is the name of the gir
(for example Gtk) and Y is the version (for example 2.0).

If output is Gtk-3.0.gir then you should name the variables like
``Gtk_3_0_gir_NAMESPACE``, ``Gtk_3_0_gir_VERSION`` etc.

* Required variables:

  * ``FILES`` - C sources and headers which should be scanned 

* One of these variables are required:

  * ``LIBS`` - Library where the symbol represented in the gir can be found
  * ``PROGRAM`` - Program where the symbol represented in the gir can be found 

* Optional variables, commonly used:

  * ``INCLUDES`` - Gir files to include without the .gir suffix, for instance
    GLib-2.0, Gtk-3.0. This is needed for all libraries which you depend on
    that provides introspection information.
  * ``SCANNERFLAGS`` - Flags to pass in to the scanner, see g-ir-scanner(1)
    for a list
  * ``PACKAGES`` - list of pkg-config names which cflags are required to parse
    the headers of this gir. Note that ``INCLUDES`` will already fetch the
    packages and thus the cflags for all dependencies.
  * ``EXPORT_PACKAGES`` - List of pkg-config names which are provided by this
    Gir.
  * ``CFLAGS`` - Flags to pass in to the parser when scanning headers.
    Normally ``INCLUDES`` and ``PACKAGES`` will fetch the cflags for all
    dependencies. This is normally used for project specific CFLAGS.
  * ``LDFLAGS`` - Linker flags used by the scanner. Normally ``INCLUDES`` and
    ``PACKAGES`` will fetch the ldflags for all dependencies. This is normally
    used for project-specific LDFLAGS (for instance if you're building several
    libraries and typelibs).

* Optional variables, seldomly used:

  * ``NAMESPACE`` - Namespace of the gir, first letter capital, rest should be
    lower case, for instance: 'Gtk', 'Clutter', 'ClutterGtk'. If not present
    the namespace will be fetched from the gir filename, the part before the
    first dash. For 'Gtk-3.0', namespace will be 'Gtk'.
  * ``VERSION`` - Version of the gir, if not present, will be fetched from gir
    filename, the part after the first dash. For 'Gtk-3.0', version will be
    '3.0'.
