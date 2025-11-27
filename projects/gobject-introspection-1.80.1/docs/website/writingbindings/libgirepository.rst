===============
libgirepository
===============

libgirepository is a C library which provides a C API for accessing the
typelib data and for interacting with the corresponding GObject based
libraries.

For more information about libgirepository see the `API documentation
<https://gnome.pages.gitlab.gnome.org/gobject-introspection/girepository/>`__.

The following example shows how to call the ``g_assertion_message()`` function
from libglib-2.0:

.. literalinclude:: ../../../examples/girepository/glib-print.c
    :language: c
