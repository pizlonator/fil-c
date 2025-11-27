GObject Introspection
=====================
The goal of the project is to describe the APIs and collect them in
a uniform, machine readable format.

GIR XML format
--------------
There's an XML format called GIR used by GObjectIntrospection.
The purpose of it is to provide a common structure to access the complete
available API that a library or other unit of code exports.
It is meant to be language agnostic using namespaces to separate
core, language or library specific functionality.
There are currently only C based tools that work on the format, but it's
meant to be usable to use in other situations, for instance to/from another
set of languages.

Typelib
-------
The typelib is an on-disk version of the GIR designed to be fast, memory
efficient and complete enough so that language bindings can be written
on top of it without other sources of information.
It is normally compiled from a GIR when a library/program is installed and
accessed from the language binding or another application who wishes to
use the information.

Tools
-----
The following tools are shipped with GObject Introspection:

- g-ir-scanner, a tool which generates GIR XML files by parsing headers,
  GTK-Doc comment blocks including annotations and introspecting GObject
  based libraries.
- g-ir-compiler, a typelib compiler. It converts one or more GIR files
  into one or more typelib blobs.
- g-ir-generate, an GIR generator, using the repository API. It generates
  GIR files from binary typelib which can be in a shared object, or a raw
  typelib blob.
- g-ir-annotation-tool, extracts annotations from source code files
- g-ir-doc-tool, generates API reference documentation from a GIR XML file.

API library
-----------
There's also a C based library called libgirepository which provides
an API to access to the typelib metadata. It also contains an API to
invoke functions, given the function info object. The implementation is
based on libffi (3.0 or higher of libffi required, can be found at
http://sourceware.org/libffi).

There are a number of GIR test files in tests/, and a script to do
roundtrip tests (GIR -> typelib -> GIR).

Helping out
-----------
The introspection framework needs a lot more work, see the ``TODO`` file.

Default branch renamed to ``main``
----------------------------------

The default development branch of GObject Introspection has been renamed
to ``main``. To update your local checkout, use::

    git checkout master
    git branch -m master main
    git fetch
    git branch --unset-upstream
    git branch -u origin/main
    git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main


Homepage
--------
https://gi.readthedocs.io/

Reporting bugs
--------------
Bugs should be reported in https://gitlab.gnome.org/GNOME/gobject-introspection/issues.

Contact
-------
:Discourse: https://discourse.gnome.org/tag/introspection
:IRC: #introspection@irc.gnome.org
