GTK Coding Style
-------------------------------------------------------------------------------

This document is intended to be a short description of the preferred
coding style to be used for the GTK source code. It was strongly
inspired by Clutter's `CODING_STYLE`.

Coding style is a matter of consistency, readability and maintenance;
coding style is also completely arbitrary and a matter of taste. This
document will use examples at the very least to provide authoritative
and consistent answers to common questions regarding the coding style,
and will also try to identify the allowed exceptions.

The examples will show the preferred coding style; the negative examples
will be clearly identified. Please, don't submit code to GTK that
looks like any of these.

Part of the rationales for these coding style rules are available either
in the kernel CodingStyle document or in Cairo's `CODING_STYLE` one.

When in doubt, check the surrounding code and try to imitate it.

### Line width

The maximum line width for source files is 80 characters, whenever possible.
Longer lines are usually an indication that you either need a function
or a pre-processor macro.

### Indentation

Each new level is indented 2 or more spaces than the previous level:

```c
  if (condition)
    single_statement ();
```

This can only be achieved using space characters. It may not be achieved
using tab characters alone, or using a combination of spaces and tabs.

Do not change the editor's configuration to change the meaning of a
tab character (see below); code using tabs to indent will not be accepted
into GTK.

Even if two spaces for each indentation level allows deeper nesting than
8 spaces, GTK favours self-documenting function names that can take
quite some space. For this reason you should avoid deeply nested code.

### Tab characters

The tab character must always be expanded to spaces. If a literal
tab must be used inside the source, the tab must always be interpreted
according to its traditional meaning:

```
	Advance to the next column which is a multiple of 8.
        [ these two lines should be aligned ]
```

### Braces

Curly braces should not be used for single statement blocks:

```c
  if (condition)
    single_statement ();
  else
    another_single_statement (arg1);
```

In case of multiple statements, curly braces should be put on another
indentation level:

```c
  if (condition)
    {
      statement_1 ();
      statement_2 ();
      statement_3 ();
    }
```

The "no block for single statements" rule has only four exceptions:

  1. if the single statement covers multiple lines, e.g. for functions with
     many arguments, and it is followed by else or else if:

```c
  /* valid */
  if (condition)
    {
      a_single_statement_with_many_arguments (some_lengthy_argument,
                                              another_lengthy_argument,
                                              and_another_one,
                                              plus_one);
    }
  else
    another_single_statement (arg1, arg2);
```

  2. if the condition is composed of many lines:

```c
  /* valid */
  if (condition1 ||
      (condition2 && condition3) ||
      condition4 ||
      (condition5 && (condition6 || condition7)))
    {
      a_single_statement ();
    }
```

  3. Nested if's, in which case the block should be placed on the
     outermost if:

```c
  /* valid */
  if (condition)
    {
      if (another_condition)
        single_statement ();
      else
        another_single_statement ();
    }

  /* invalid */
  if (condition)
    if (another_condition)
      single_statement ();
    else if (yet_another_condition)
      another_single_statement ();
```

  4.  If either side of an if-else statement has braces, both sides
     should, to match up indentation:

```c
  /* valid */
  if (condition)
    {
      foo ();
      bar ();
    }
  else
    {
      baz ();
    }

  /* invalid */
  if (condition)
    {
      foo ();
      bar ();
    }
  else
    baz ();
```

In general, new blocks should be placed on a new indentation level,
like:

```c
  int retval = 0;

  statement_1 ();
  statement_2 ();

  {
    int var1 = 42;
    gboolean res = FALSE;

    res = statement_3 (var1);

    retval = res ? -1 : 1;
  }
```

While curly braces for function definitions should rest on a new line
they should not add an indentation level:

```c
  /* valid */
  static void
  my_function (int argument)
  {
    do_my_things ();
  }

  /* invalid */
  static void
  my_function (int argument) {
    do_my_things ();
  }

  /* invalid */
  static void
  my_function (int argument)
    {
      do_my_things ();
    }
```

Curly braces must not be placed on the same line as a condition:

```c
  /* invalid */
  if (condition) {
    statement_1 ();
    statement_2 ();
  }
```

### Conditions

Do not check boolean values for equality:

```c
  /* invalid */
  if (condition == TRUE)
    do_foo ();

  /* valid */
  if (another_condition)
    do_bar ();
```

Even if C handles NULL equality like a boolean, be explicit:

```c
  /* valid */
  if (some_pointer == NULL)
    do_blah ();

  /* invalid */
  if (some_other_pointer)
    do_blurp ();
```

In case of conditions split over multiple lines, the logical operators should
always go at the end of the line:

```c
  /* invalid */
  if (condition1
      || condition2
      || condition3)
    {
      do_foo ();
    }

  /* valid */
  if (condition1 &&
      condition2 &&
      (condition3 || (condition4 && condition5)))
    {
      do_blah ();
    }
```

### Functions

Functions should be declared by placing the returned value on a separate
line from the function name:

```c
  void
  my_function (void)
  {
  }
```

The arguments list must be broken into a new line for each argument,
with the argument names right aligned, taking into account pointers:

```c
  void
  my_function (some_type_t     type,
               another_type_t *a_pointer,
               final_type_t    another_type)
  {
  }
```

The alignment also holds when invoking a function without breaking the
80 characters limit:

```c
  align_function_arguments (first_argument,
                            second_argument,
                            third_argument);
```

To respect the 80 characters limit do not break the function name from
the arguments:

```c
  /* invalid */
  a_very_long_function_name_with_long_parameters
    (argument_the_first, argument_the_second);

  /* valid */
  first_a = argument_the_first;
  second_a = argument_the_second;
  a_very_long_function_name_with_long_parameters (first_a, second_a);
```

### Whitespace

Always put a space before a parenthesis but never after:

```c
  /* valid */
  if (condition)
    do_my_things ();

  /* valid */
  switch (condition)
    {
    }

  /* invalid */
  if(condition)
    do_my_things();

  /* invalid */
  if ( condition )
    do_my_things ( );
```

A `switch()` should open a block on a new indentation level, and each case
should start on the same indentation level as the curly braces, with the
case block on a new indentation level:

```c
  /* valid */
  switch (condition)
    {
    case FOO:
      do_foo ();
      break;

    case BAR:
      do_bar ();
      break;
    }

  /* invalid */
  switch (condition) {
    case FOO: do_foo (); break;
    case BAR: do_bar (); break;
  }

  /* invalid */
  switch (condition)
    {
    case FOO: do_foo ();
      break;
    case BAR: do_bar ();
      break;
    }

  /* invalid */
  switch (condition)
    {
      case FOO:
      do_foo ();
      break;
      case BAR:
      do_bar ();
      break;
    }
```

It is preferable, though not mandatory, to separate the various cases with
a newline:

```c
  switch (condition)
    {
    case FOO:
      do_foo ();
      break;

    case BAR:
      do_bar ();
      break;

    default:
      do_default ();
    }
```

The `break` statement for the `default:` case is not mandatory.

If a case block needs to declare new variables, the same rules as the
inner blocks (see above) apply; the break statement should be placed
outside of the inner block:

```c
  switch (condition)
    {
    case FOO:
      {
        int foo;

        foo = do_foo ();
      }
      break;

    ...
    }
```

When declaring a structure type use newlines to separate logical sections
of the structure:

```c
  struct _GtkWrapBoxPrivate
  {
    GtkOrientation        orientation;
    GtkWrapAllocationMode mode;

    GtkWrapBoxSpreading   horizontal_spreading;
    GtkWrapBoxSpreading   vertical_spreading;

    guint16               vertical_spacing;
    guint16               horizontal_spacing;

    guint16               minimum_line_children;
    guint16               natural_line_children;

    GList                *children;
  };
```

Do not eliminate whitespace and newlines just because something would
fit on 80 characters:

```c
  /* invalid */
  if (condition) foo (); else bar ();
```

Do eliminate trailing whitespace on any line, preferably as a separate
patch or commit. Never use empty lines at the beginning or at the end of
a file.

Do enable the default git pre-commit hook that detect trailing
whitespace for you and help you to avoid corrupting GTK's tree with
it. Do that as follows:

```
  chmod a+x .git/hooks/pre-commit
```

You might also find the git-stripspace utility helpful which acts as a
filter to remove trailing whitespace as well as initial, final, and
duplicate blank lines.

### Headers

Headers are special, for GTK, in that they don't have to obey the
80 characters limit. The only major rule for headers is that the function
definitions should be vertically aligned in three columns:

```c
  return value          function_name           (type   argument,
                                                 type   argument,
                                                 type   argument);
```

The maximum width of each column is given by the longest element in the
column:

```c
  void        gtk_type_set_property (GtkType      *type,
                                     const char   *value,
                                     GError      **error);
  const char *gtk_type_get_property (GtkType      *type);
```

It is also possible to align the columns to the next tab:

```c
  void          gtk_type_set_prop           (GtkType *type,
                                             float    value);
  float         gtk_type_get_prop           (GtkType *type);
  int           gtk_type_update_foobar      (GtkType *type);
```

Public headers should never be included directly:

```c
  #if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
  #error "Only <gtk/gtk.h> can be included directly."
  #endif
```

Private headers should include the public header first, if one exists:

```c
  #pragma once

  /* gtkfooprivate.h */

  #include "gtkfoo.h"

  ...

  #endif /* __GTK_FOO_PRIVATE_H__ */
```

All headers should use the `once` pragma to prevent multiple inclusion,
instead of the classic pre-processor guards:

```c
  #pragma once
```

Additionally, public headers should use C++ guards around their declarations:

```c
  G_BEGIN_DECLS

  GDK_AVAILABLE_IN_ALL
  GType         gtk_foo_get_type        (void) G_GNUC_CONST;

  GDK_AVAILABLE_IN_ALL
  GtkWidget *   gtk_foo_new             (void);

  ...

  G_END_DECLS
```

### Includes

GTK source files must never include the global gtk.h header; instead, it
should include the individual headers that are needed.

Every source file must include config.h first, followed by the header matching
the source file, either the public installed header, or the private header, if
it exists.

```c
  /* gtkfoo.c */

  #include "config.h"

  #include "gtkfoo.h"
```

Source files should then include project headers, in alphabetical order,
starting from headers in the current directory; then headers in
sub-directories; and, finally, in paths relative to the top-level
directory:

```c
  #include "config.h"

  #include "gtkfooprivate.h"

  #include "gtkbutton.h"
  #include "gtkwidget.h"

  #include "a11y/gtkwidgetaccessible.h"

  #include "gdk/gdkwindowprivate.h"
```

Finally, source files should include the system headers last:

```c
  #include "config.h"

  #include "gtkbarprivate.h"

  #include "gtkcontainerprivate.h"

  #include "a11y/gtkcontaineraccessible.h"
  #include "a11y/gtkwidgetaccessible.h"

  #include "gdk/gdkwindowprivate.h"

  #include <graphene.h>
  #include <string.h>
```

Cyclic dependencies should be avoided if at all possible;
for instance, you could use additional headers to break cycles.

### GObject

GObject classes definition and implementation require some additional
coding style notices.

Typedef declarations should be placed at the beginning of the file:

```c
  typedef struct _GtkFoo          GtkFoo;
  typedef struct _GtkFooClass     GtkFooClass;
```

This includes enumeration types:

```c
  typedef enum
  {
    GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT,
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH
  } GtkSizeRequestMode;
```

And callback types:

```c
  typedef void (* GtkCallback) (GtkWidget *widget,
                                gpointer   user_data);
```

Instance structures should only contain the parent type:

```c
  struct _GtkFoo
  {
    GtkWidget parent_instance;
  };
```

You should use the `G_DECLARE_DERIVABLE_TYPE()` and `G_DECLARE_FINAL_TYPE()`
macros in newly written headers. There is also a `GDK_DECLARE_INTERNAL_TYPE()`
for declaring types that can be derived inside GTK, but not in 3rd party code.

Inside your source file, always use the `G_DEFINE_TYPE()`,
`G_DEFINE_TYPE_WITH_PRIVATE()`, and `G_DEFINE_TYPE_WITH_CODE()` macros, or their
`abstract variants G_DEFINE_ABSTRACT_TYPE()`,
`G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE()`, and `G_DEFINE_ABSTRACT_TYPE_WITH_CODE()`;
also, use the similar macros for defining interfaces, quarks, and boxed types.

All the properties should be stored inside the private data structure, which
is defined inside the source file - or, if needed, inside a private header
file; the private header filename must end with "private.h" and must not be
installed.

The private data structure should only be accessed internally either using the
pointer inside the instance structure, for legacy code, or the generated
instance private data getter function for your type. You should never use the
`G_TYPE_INSTANCE_GET_PRIVATE()` macro or the `g_type_instance_get_private()`
function.

Interface types should always have the dummy typedef for cast purposes:

```c
        typedef struct _GtkFoo                  GtkFoo;
```

The interface structure should have "Interface" postfixed to the dummy typedef:

```c
        typedef struct _GtkFooInterface         GtkFooInterface;
```

Interfaces must have the following macros:

| Macro                        | Expands to                      |
|------------------------------|---------------------------------|
| `GTK_TYPE_<iface_name>`      | `<iface_name>_get_type`         |
| `GTK_<iface_name>`           | `G_TYPE_CHECK_INSTANCE_CAST`    |
| `GTK_IS_<iface_name>`        | `G_TYPE_CHECK_INSTANCE_TYPE`    |
| `GTK_<iface_name>_GET_IFACE` | `G_TYPE_INSTANCE_GET_INTERFACE` |

The `G_DECLARE` macros define these as static inline functions instead.

### Memory allocation

When dynamically allocating data on the heap use `g_new()`.

Public structure types should always be returned after being zero-ed,
either explicitly for each member, or by using `g_new0()`. Do not use
`g_slice` in new code.

Memory that is only needed within the scope of a function can be
stack-allocated using `g_newa()` or `g_alloca()`. But limit the amount
of stack memory that you consume this way, in particular in recursive
functions.

On Windows, the default stack size we have to work with is 1M.

### Macros

Try to avoid private macros unless strictly necessary. Remember to #undef
them at the end of a block or a series of functions needing them.

Inline functions are usually preferable to private macros.

Public macros should not be used unless they evaluate to a constant.

### Symbol visibility

Any symbol that is not explicitly annotated using a `GDK_AVAILABLE_IN_*`
or `GDK_DEPRECATED_IN_*` macro is considered internal, and not exported
in the shared library.

Never export variables as public API, since this is cumbersome on some
platforms. It is always preferable to add getters and setters instead.

Non-exported functions that are needed in more than one source file
should be declared in a private header file with a name that ends in
`private.h`.

Non-exported functions that are only needed in one source file
should be declared static.

### Documentation

All public APIs must have doc comments. For functions, these should
be placed in the source file, directly above the function.

```c
  /* valid */
  /**
   * gtk_get_flow:
   * @widget: a `GtkWidget`
   *
   * Gets the flow of a widget.
   *
   * Note that flows may be laminar or turbulent...
   *
   * Returns: (transfer none): the flow of @widget
   */
  GtkFlow *
  gtk_get_flow (GtkWidget *widget)
  {

    ...

  }
```

Doc comments for macros, function types, class structs, etc should be
placed next to the definitions, typically in headers.

The main content of the doc comments uses markdown, which can include
inline formatting, sections, tables, images, links. For links to
symbols/classes/etc, we use a markdown extension syntax like this:

[class@Gtk.Widget], [method@Gtk.ListView.get_factory],...

Every doc comment should start with a single-sentence paragraph that
can serve as a summary of sorts (it will often be placed next to a
link pointing to the full documentation for the symbol/class/etc).
The summary should not include links.

Long-form documentation for classes should be included in the doc
comment for the struct, typically at the top of the source file,
after the license header and includes:

```c
  /**
   * GtkSizeRequest:
   *
   * The GtkSizeRequest interface is GTK's height-for-width geometry
   * geometry management system.
   *
   * # Geometry management
   *
   * ...
   */
```

For more information on the documentation style and contribution guidelines,
please [follow the corresponding contribution guide](./reference/README.md).

### Old code

New code that is being added to GTK should adhere to the style
explained above. Existing GTK code does largely follow these
conventions, but there are some differences, e.g. occurrences
of tabs, etc.

It is ok to update the style of a code block or function when you
are touching it anyway, but sweeping whitespace changes obscure the
source revision history, and should be avoided.
