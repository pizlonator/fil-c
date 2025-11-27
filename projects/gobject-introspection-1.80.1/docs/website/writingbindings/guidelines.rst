==========
Guidelines
==========

This page is intended as a guide describing the process of writing a language
binding for the GObject Introspection framework.

* Decide if you want to make a language binding which is implementation
  agnostic or implementation specific. Some languages such as Python have
  libraries which are available across implementations. The Python module
  ``ctypes`` is a binding for the libffi language binding, which is
  implemented in a couple of different Python implementations. It's usually
  easier to target a specific interpreter or compiler implementation so if you
  unsure, write a specific one.

* For interpreted language implementations, construct the language binding on
  top of the :doc:`libgirepository` library instead of writing a code generator.
  It'll make it easier for developers to use your language binding as they
  will be able to read the typelibs in runtime without having an extra
  intermediate step to generate the language specific metadata. The
  libgirepository library can also be used for bindings based upon code
  generators as an optimization, its a lot faster to read metadata from the
  typelib than it is to extract the metadata from the GIR XML files.

* Use the Everything library in your unittests, aim at testing all functions
  there. Do testing as early as possible in the development of the binding, as
  the code is likely to be more complex than you anticipate.

* Use the same coding style as your language. If libraries for your language
  normally uses underscores do that as well. For instance, Java bindings
  should have a method on it's GtkButton wrapper called ``setLabel`` and not
  ``set_label``.

* If there are existing GObject bindings, reuse them for improved
  compatibility. It's a nice feature being able to use static bindings and
  introspected bindings together. The Perl & Python bindings does that.

* Try to stay close to the C language semantics, for instance
  GObject should be mapped to a class and gtk_button_set_label to a method of
  that class:

    * java: ``button.setLabel("foo")``
    * javascript/python/vala: ``button.set_label("foo")``
    * perl: ``$button->set_label("foo")``
    * scheme: ``(send button (set-label "foo"))``
