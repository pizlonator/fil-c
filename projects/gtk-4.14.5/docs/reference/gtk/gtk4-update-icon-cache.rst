.. _gtk4-update-icon-cache(1):

======================
gtk4-update-icon-cache
======================

--------------------------
Icon theme caching utility
--------------------------

SYNOPSIS
--------

|   **gtk4-update-icon-cache** [OPTIONS...] <PATH>

DESCRIPTION
-----------

``gtk4-update-icon-cache`` creates ``mmap(2)``-able cache files for icon themes.

It expects to be given the ``PATH`` to an icon theme directory containing an
``index.theme``, e.g. ``/usr/share/icons/hicolor``, and writes a
``icon-theme.cache`` containing cached information about the icons in the
directory tree below the given directory.

GTK can use the cache files created by ``gtk4-update-icon-cache`` to avoid a lot
of system call and disk seek overhead when the application starts. Since the
format of the cache files allows them to be shared across multiple processes,
for instance using the POSIX ``mmap(2)`` system call, the overall memory
consumption is reduced as well.

OPTIONS
-------

``-f, --force``

  Overwrite an existing cache file even if it appears to be up-to-date.

``-t, --ignore-theme-index``

  Don't check for the existence of ``index.theme`` in the icon theme directory.
  Without this option, ``gtk4-update-icon-cache`` refuses to create an icon
  cache in a directory which does not appear to be the toplevel directory of an
  icon theme.

``-i, --index-only``

  Don't include image data in the cache.

``--include-image-data``

  Include image data in the cache.

``-c, --source <NAME>``

  Output a C header file declaring a constant ``NAME`` with the contents of the
  icon cache.

``-q, --quiet``

  Turn off verbose output.

``-v, --validate``

  Validate existing icon cache.
