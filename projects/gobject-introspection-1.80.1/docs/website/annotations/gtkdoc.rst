=====================
GTK-Doc Format Primer
=====================

GObject-Introspection annotations are built on top of GTK-Doc comment
blocks. These are plain old C comment blocks, but formatted in a special
way. Each GTK-Doc comment block starts with a ``/**`` on its own line
end ends with ``*/``, again on its own line.

The basic format of a GTK-Doc comment block looks like this:

::

  /**
   * identifier_name: (annotations)
   * @parameter_name: (annotations): description
   *
   * symbol description
   *
   * tag_name: (annotations): description
   */

As we can see, a GTK-Doc comment block can be broken down into a couple of
parts. Each part is built out of one or more fields, separated by a ``:``
character. Each part has to start on its own line. Fields cannot span multiple
lines except the various ``description`` fields.

The order in which parts are written is important. For example, putting a
``tag`` part before the ``symbol description`` part is invalid as it would
result in the symbol description to be mistaken for the tag description.

In the above example we have:

* the start of a GTK-Doc comment block on line 1
* the identifier part on line 2
* a parameter part on line 3
* the symbol description on line 5
* a tag part on line 7
* the end of the comment block on line 8

identifier part
~~~~~~~~~~~~~~~

::

  /**
   * identifier_name: (annotations)
   * ...
   */

The identifier part is required as it identifies the symbol you want to
annotate. It is always written on the line immediately following the start of
your GTK-Doc comment block (``/**``).

The ``identifier`` part is constructed from:

* a required ``identifier_name`` field

  * different kinds of symbols that can be documented and annotated are
    described in the GTK-Doc manual.

* an optional ``annotations`` field

parameter part
~~~~~~~~~~~~~~

::

  /**
   * ...
   * @parameter_name: (annotations): description
   * ...
   */

The ``parameter`` part is optional. This means that there can be 0 or more
parameters, depending on the symbol you are annotating.

``parameter`` parts are constructed from:

* a required ``parameter_name`` which starts with a ``@`` character

  * this name should correspond with the parameter name of you function's
    signature.

* an optional ``annotations`` field
* a required description field (can be "empty")

  * can contain a single paragraph (multiple lines but no empty lines) of
    text.

Note that multiple ``parameter`` parts are never separated by an empty line.

symbol description part
~~~~~~~~~~~~~~~~~~~~~~~

::

  /**
   * ...
   *
   * symbol description
   * ...
   */

The ``symbol description`` part is optional. When present, it must always be
preceded with an empty line. It can contain multiple paragraphs (multiple
lines and empty lines) describing what the function, property, signal, enum or
constant does.

tag part
~~~~~~~~

::

  /**
   * ...
   * tag_name: (annotations)||value: description
   * ...
   */

The ``tag`` part is optional. There can be 0 or more tags, depending on the
symbol you are annotating.

``tag`` parts are constructed from:

* a required ``tag_name``

  * There are only four valid tags: ``Returns``, ``Since``, ``Deprecated``,
    and ``Stability``.

* an optional ``annotations`` field (``Returns``)
  **OR**
  an optional ``value`` field (``Since``, ``Deprecated``, and ``Stability``)
* a required description field (can be "empty")

  * can contain multiple paragraphs (multiple lines and empty lines) of text.

``tag`` parts can safely be preceded or followed by an empty line.

Tags taking an optional ``value`` field accept the following values:

.. list-table::
  :header-rows: 1
  :widths: 1 1 10

  * - Tag
    - Value field
    - Description
  * - ``Since``
    - ``VERSION``
    - This symbol was added in version ``VERSION``.
  * - ``Deprecated``
    - ``VERSION``
    - This symbol has been deprecated since version ``VERSION``.
  * - ``Stability``
    - ``Stable``, ``Unstable``, or ``Private``
    - An informal description of the stability level of this symbol.


GTK-Doc support
---------------

If GTK-Doc doesn't seem to understand your introspection annotations, you may
need to do two things:

#. make sure you are running GTK-Doc >= v1.12 (also try latest version from
   git)
#. add ``<xi:include href="xml/annotation-glossary.xml"><xi:fallback/></xi:include>``
   to your master GTK-Doc document; e.g. see the end of `tester-docs.xml
   <https://gitlab.gnome.org/GNOME/gtk-doc/blob/master/tests/gobject/docs/tester-docs.xml>`__
