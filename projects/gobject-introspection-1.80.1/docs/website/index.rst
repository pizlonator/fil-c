=====================
GObject Introspection
=====================

.. title:: Overview

.. toctree::
    :hidden:
    :titlesonly:
    :maxdepth: 1

    changelog
    goals
    architecture
    users
    build_test
    writingbindableapis
    buildsystems/index
    annotations/index
    writingbindings/index
    tools/index

GObject introspection is a middleware layer between C libraries (using
GObject) and language bindings. The C library can be scanned at compile time
and generate metadata files, in addition to the actual native C library. Then
language bindings can read this metadata and automatically provide bindings to
call into the C library.

.. figure:: images/overview.svg
    :width: 85%
    :align: center

The GI project consists of:

* an XML format called GIR containing introspection information in a machine parseable format
* a Python package to create and parse the GIR format
* a scanner to generate GIR format from C source and headers
* a typelib similar to xpcom/msole which stores the information on disk in a binary format
* a compiler to compile the typelib from a xml format (and vice versa)
* C library to read the typelib, :doc:`writingbindings/libgirepository`.


Getting the code
----------------

The latest stable release is available from
https://download.gnome.org/sources/gobject-introspection

GObject Introspection is stored in git and can be fetched:

.. code-block:: text

    git clone https://gitlab.gnome.org/GNOME/gobject-introspection.git

You can browse the repository online `here <https://gitlab.gnome.org/GNOME/gobject-introspection/>`__.


Reporting bugs
--------------

For a list of existing bugs and feature requests, see the `issues page
<https://gitlab.gnome.org/GNOME/gobject-introspection/issues>`__. You can also
`open an issue
<https://gitlab.gnome.org/GNOME/gobject-introspection/issues/new>`__.


Contact
-------

For questions or additional information, please use:

* Discourse: https://discourse.gnome.org/tag/introspection
* IRC: #introspection on irc.gnome.org
