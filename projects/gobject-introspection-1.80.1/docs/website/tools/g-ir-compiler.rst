=============
g-ir-compiler
=============

----------------
Typelib compiler
----------------

:Manual section: 1


SYNOPSIS
========

**g-ir-compiler** [OPTION...] GIRFILE


DESCRIPTION
===========

g-ir-compiler converts one or more GIR files into one or more typelib. The
output will be written to standard output unless the ``--output`` is
specified.


OPTIONS
=======

--help
    Show help options

--output=FILENAME
    Save the resulting output in FILENAME.

--verbose
    Show verbose messages

--debug
    Show debug messages

--includedir=DIRECTORY
    Adds a directory which will be used to find includes inside the GIR format.

--module=MODULE
    FIXME

--shared-library=FILENAME
    Specifies the shared library where the symbols in the typelib can be
    found. The name of the library should not contain the ending shared
    library suffix.

--version
    Show program's version number and exit


BUGS
====

Report bugs at https://gitlab.gnome.org/GNOME/gobject-introspection/issues


HOMEPAGE and CONTACT
====================

https://gi.readthedocs.io/


AUTHORS
=======

Mattias Clasen
