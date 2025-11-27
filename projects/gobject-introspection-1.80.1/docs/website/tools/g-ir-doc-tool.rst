=============
g-ir-doc-tool
=============

---------------------
Documentation builder
---------------------

:Manual section: 1


SYNOPSIS
========

**g-ir-doc-tool** [OPTION...] GIRFILE


DESCRIPTION
===========

g-ir-doc-tool builds library documentation directly from .gir files. The output
is adjusted according to which programming language you're generating docs for.


OPTIONS
=======

--help
    Show help options

--output=DIRECTORY
    Save the resulting output in DIRECTORY.

--format=FORMAT
    Output format. One of devdocs, mallard or sections.

--language=LANGUAGE
    Output language. One of c, python, or gjs.

--add-include-path=DIRECTORY
    Adds a directory which will be used to find includes inside the GIR format.

--version
    Show program's version number and exit

--write-sections-file
    Backwards-compatible equivalent to -f sections.


BUGS
====

Report bugs at https://gitlab.gnome.org/GNOME/gobject-introspection/issues


HOMEPAGE and CONTACT
====================

https://gi.readthedocs.io/


AUTHORS
=======

David King
