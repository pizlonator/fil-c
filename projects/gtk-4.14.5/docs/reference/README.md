# How to contribute to GTK's documentation

The GTK documentation is divided in two major components:

 - the API reference, which is generated from special comments in the GTK
   source code
 - static pages that provide an overview of specific sections of the API

In both cases, the contents are parsed as markdown and cross-linked in order
to match types, functions, signals, and properties. Ultimatively, we generate
HTML, which can be used to read the documentation both offline and online.

Contributing to the GTK documentation requires modifying files tracked in the
source control repository, and follows the same steps as any other code
contribution as outlined in the GTK [contribution guide][contributing].
Please, refer to that document for any further question on the mechanics
of contributing to GTK.

GTK uses [gi-docgen][gidocgen] to generate its documentation. Please, visit
the gi-docgen website to read the project's documentation.

[contributing]: ../../CONTRIBUTING.md
[gi-docgen]: https://gitlab.gnome.org/ebassi/gi-docgen

## Contributing to the API reference

Whenever you need to add or modify the documentation of a type or a
function, you will need to edit a comment stanza, typically right
above the type or function declaration. For instance:

```c
/**
 * gtk_foo_set_bar:
 * @self: a foo widget
 * @bar: (nullable): the bar to set
 *
 * Sets the given `GtkBar` instance on a foo widget.
 *
 * Returns: `TRUE` if the bar was set
 */
gboolean
gtk_foo_set_bar (GtkFoo *self,
                 GtkBar *bar)
{
  ...
```

Or, for types:

```c
/**
 * GtkFoo:
 *
 * A foo widget instance.
 */
struct _GtkFoo
{
  /*< private >*/
  GtkWidget parent_instance;
};
```

The GTK documentation also contains a number of 'freestanding' chapters
for which the source is in .md files in docs/reference/gtk.

## Style guide

Like the [coding style][coding], these rules try to formalize the existing
documentation style; in general, you should only ever modify existing code
that does not match the rules if you're already changing that code for
unrelated reasons.

[coding]: ../CODING-STYLE.md

### Syntax

The input syntax for GTK documentation is Markdown, in a flavor that is
similar to what you see on GitLab or GitHub. The markdown support for
fragments that are extracted from sources is identical to the one for
freestanding chapters. As an exception, man pages for tools are currently
maintained in docbook, since the conversion from markdown to docbook is
losing too much of the expected formatting.

In addition to typical markdown formatting such as \*emphasis\* or \_italics\_,
the GTK documentation supports additional link formats, like:

`[class@Namespace.ClassName]`
 : Creates a link to the docs for a class

`[method@Namespace.Method.name]`
 : Creates a link to the docs for a method in a class

`[func@Namespace.function]`
 : Creates a link to the docs for a global function

For more information on the available link formats, see the gi-docgen
documentation.

Every doc comment should start with a single-sentence paragraph that
can serve as a summary of sorts (it will often be placed next to a
link pointing to the full documentation for the symbol/class/etc).
The summary should not include links.

### Introspection annotations

The purpose of the annotations for function arguments, properties, signals,
etc., is to describe the API in a machine readable way. The annotations
are consumed by language bindings and by the documentation tools.

For more information about the annotations used by GTK, you should refer to
the [GObject Introspection documentation][gi-annotations].

[gi-annotations]: https://gi.readthedocs.io/en/latest/annotations/giannotations.html

### Type description

Each type should be annotated with a description of what the type does.

For classes, the description should contain an overview of the type;
what it does; typical use cases; and idiomatic examples of its use.

For widget classes, the description should also contain:

  - special XML elements and attributes parsed by the class, in case of a
    custom GtkBuildable implementation
  - the CSS element name to be used by selectors
  - the CSS selector hierarchy for children, in case of a composite widget
  - the accessible role of the class

Each section in a type description can have a heading; it's preferred to use
second and third level headings only.

### Functions

 - The argument names must match in the declaration, definition, and
   documentation stanza.
 - The description should refer to the function as the subject, e.g.:

```
Adds a shortcut to the shortcuts controller.
```

   Or:

```
Checks whether the widget is set to be visible or not.
```

### Methods

 - Methods are special functions whose first argument is always the instance
   of a certain class. The instance argument for newly written code should be
   called `self`.
 - If a method is a setter or a getter for an object property, you should
   add an `(attributes org.gtk.Method.set_property=property-name)` or a
   an `(attributes org.gtk.Method.get_property=property-name)` annotation
   to the method's identifier
 - If a method changes one or more properties as side effect, link those
   properties in the method's description
 - If a method is a signal emitter, you should use the
   `(attributes org.gtk.Method.signal=signal-name)` annotation in
   the method's identifier

### Arguments and return values

 - Arguments should be descriptive, but short
 - There is no need to mention the type of the argument
 - Always annotate nullability, direction, and ownership transfer

### Signals

 - While GObject can introspect argument and return types for signals,
   you should *always* document them with an explicit documentation stanza.
 - The syntax for signal stanzas is similar to functions:

```c
/**
 * GtkFoo::signal-name:
 * @arg1: ...
 * @arg2: ...
 *
 * ...
```

### Properties

 - While GObject properties contain text that can be extracted
   programmatically in order to build their documentation, you should
   *always* document them with an explicit documentation stanza. The text
   associated to the property is short and meant to be used when
   programmatically building user interfaces, and not for documentation
   purposes.
 - Always note if setting a property has side effects, like causing another
   property to change state.
 - If the property has public accessors you should annotate it with
   the `(attributes org.gtk.Property.set=setter_function)` and
   `(attributes org.gtk.Property.get=getter_function)` attributes
 - The syntax for property documentation is:

```c
/**
 * GtkFoo:property-name:
 *
 * ...
```

### Actions

 - Actions are new in GTK 4, and describe an action associated to
   a widget class
 - The syntax for action documentation is:

```
/**c
 * GtkFoo|action-name:
 * @arg1: ...
 * @arg2: ...
 *
 * ...
```
