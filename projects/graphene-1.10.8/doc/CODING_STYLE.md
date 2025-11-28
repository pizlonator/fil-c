# Graphene Coding Style

This document is intended to be a short description of the preferred
coding style to be used for the Graphene source code.

Coding style is a matter of consistency, readability and maintainance;
coding style is also completely arbitrary and a matter of taste. This
document will use examples at the very least to provide authoritative
and consistent answers to common questions regarding the coding style,
and will also try to identify the allowed exceptions.

The examples will show the preferred coding style; the negative examples
will be clearly identified. Please, don't submit code to Graphene that
looks like any of these.

Part of the rationales for these coding style rules are available either
in the Linux kernel `CodingStyle` document or in Cairo's `CODING_STYLE`
one.

When in doubt, check the surrounding code and try to imitate it.

You're strongly encouraged to use the [uncrustify](http://uncrustify.sourceforge.net/)
source code beautifier tool to check and fix the coding style of your
contributions. Graphene provides a list of options that implement the
coding style below, as well as a list of files where the coding style
applies.

### Line width

The maximum line width for source files is 100 characters, whenever possible.
Longer lines are usually an indication that you either need a function
or a pre-processor macro.

### Indentation

Each new level is indented 2 or more spaces than the previous level:

    if (condition)
      single_statement ();

This can only be achieved using space characters. It may not be achieved
using tab characters alone, or using a combination of spaces and tabs.

Do not change the editor's configuration to change the meaning of a
tab character (see below); code using tabs to indent will not be accepted
into Graphene.

Even if two spaces for each indentation level allows deeper nesting than
8 spaces, Graphene favours self-documenting function names that can take
quite some space. For this reason alone you should avoid deeply nested
code.

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

    if (condition)
      single_statement ();
    else
      another_single_statement (arg1);

In case of multiple statements, curly braces should be put on another
indentation level:

    if (condition)
      {
        statement_1 ();
        statement_2 ();
        statement_3 ();
      }

The "no block for single statements" rule has only three exceptions:

 1.  if the single statement covers multiple lines, e.g. for functions with
     many arguments, and it is followed by else or else if:

```
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

 2.  if the condition is composed of many lines:

```
    /* valid */
    if (condition1 ||
        (condition2 && condition3) ||
        condition4 ||
        (condition5 && (condition6 || condition7)))
      {
        a_single_statement ();
      }
```

 3.  Nested if's, in which case the block should be placed on the
     outermost if:
```
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

In general, new blocks should be placed on a new indentation level,
like:

    int retval = 0;

    statement_1 ();
    statement_2 ();

    {
      int var1 = 42;
      gboolean res = FALSE;

      res = statement_3 (var1);

      retval = res ? -1 : 1;
    }

While curly braces for function definitions should rest on a new line
they should not add an indentation level:

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

Curly braces must not be placed on the same line as a condition:

    /* invalid */
    if (condition) {
      statement_1 ();
      statement_2 ();
    }

### Conditions

Do not check boolean values for equality:

    /* invalid */
    if (condition == TRUE)
      do_foo ();

    /* valid */
    if (another_condition)
      do_bar ();

Even if C handles NULL equality like a boolean, be explicit:

    /* valid */
    if (some_pointer == NULL)
      do_blah ();

    /* invalid */
    if (some_other_pointer)
      do_blurp ();

In case of conditions split over multiple lines, the logical operators should
always go at the end of the line:

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

### Types

All public types in Graphene should conform to the `<namespace>_<type>_t`
pattern, e.g.:

    /* valid */
    typedef struct _graphene_quaternion_t	graphene_quaternion_t;

    /* valid */
    typedef enum {
      ...
    } graphene_euler_order_t;

    /* invalid */
    typedef struct _foo_bar_t	foo_bar_t;

    /* invalid */
    typedef enum {
      ...
    } GrapheneBlah;

CamelCase is not allowed in the public API, and it's strongly discouraged
for the internals as well.

All public types should have public structure definitions, but if fields are
to be considered private then you should add them at the end of the
structure definition, and use the `GRAPHENE_PRIVATE_FIELD` macro to declare
them, e.g.:

    /* valid */
    struct _graphene_euler_t
    {
      GRAPHENE_PRIVATE_FIELD (graphene_vec3_t, angles);
      GRAPHENE_PRIVATE_FIELD (graphene_euler_order_t, order);
    };

    /* invalid */
    struct _graphene_foo_t
    {
      graphene_vec4_t __some_private_field;
      graphene_vec4_t public_field;
    };

All public types should have the following functions:

 * a `graphene_<type>_alloc()` allocator; the contents of the structure
   returned by this function are undefined
 * a `graphene_<type>_free()` deallocator; this function should be NULL-safe
 * a `graphene_<type>_init()` initializer

All initializer functions should take a pointer of the type they initialize
as their first argument, and return the initialized structure, e.g.:

    /* valid */
    graphene_rect_t *   graphene_rect_init      (graphene_rect_t *rect,
                                                 float            x,
                                                 float            y,
                                                 float            width,
                                                 float            height);

All types should be usable on the stack, after being initialized; the
behaviour of a type before initialization is undefined.

### Functions

Functions should be declared by placing the returned value on a separate
line from the function name:

    void
    my_function (void)
    {
    }

The arguments list must be broken into a new line for each argument,
with the argument names right aligned, taking into account pointers:

    void
    my_function (some_type_t     type,
                 another_type_t *a_pointer,
                 final_type_t    another_type)
    {
    }

The alignment also holds when invoking a function without breaking the
80 characters limit:

    align_function_arguments (first_argument,
                              second_argument,
                              third_argument);

To respect the 80 characters limit do not break the function name from
the arguments:

    /* invalid */
    a_very_long_function_name_with_long_parameters
      (argument_the_first, argument_the_second);

    /* invalid */
    a_very_long_function_name_with_long_parameters (
      argument_the_first, argument_the_second);

    /* valid */
    first_a = argument_the_first;
    second_a = argument_the_second;
    a_very_long_function_name_with_long_parameters (first_a, second_a);

All public functions must be annotated with the `GRAPHENE_AVAILABLE_IN_x_y`
macro for the `x.y` stable release in which they appear. Functions private
to a specific source file should always be marked as `static`; functions
private to the whole project should be declared in a private header.

### Whitespace

Always put a space before a parenthesis but never after:

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

A `switch()` should open a block on a new indentation level, and each case
should start on the same indentation level as the curly braces, with the
case block on a new indentation level:

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

It is preferable, though not mandatory, to separate the various cases with
a newline:

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

The `break` statement for the `default:` case is not mandatory, but it is
appreciated.

If a case block needs to declare new variables, the same rules as the
inner blocks (see above) apply; the break statement should be placed
outside of the inner block:

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

Do not eliminate whitespace and newlines just because something would
fit in the line character limit:

    /* invalid */
    if (condition) foo (); else bar ();

Please, eliminate all trailing or leading whitespace before submitting a
patch for review.

### Headers

Headers are special, for Graphene, in that they don't have to obey the
80 characters limit. The only major rule for headers is that the functions
definition should be vertically aligned in three columns:

    return value          function_name           (type   argument,
                                                   type   argument,
                                                   type   argument);

The maximum width of each column is given by the longest element in the
column:

    bool        graphene_type_set_property (graphene_type_t  *type,
                                            const char       *value,
                                            char            **out_value);
    const char *graphene_type_get_property (graphene_type_t  *type);

It is also possible to align the columns to the next tab:

    void          graphene_type_set_property        (graphene_type_t *type,
                                                     float            value);
    float         graphene_type_get_property        (graphene_type_t *type);
    int           graphene_type_update_foobar       (graphene_type_t *type);

Public headers should have inclusion guards and C++ guards:

    #ifndef __GRAPHENE_HEADER_H__
    #define __GRAPHENE_HEADER_H__

    #if !defined(__GRAPHENE_H_INSIDE__) && !defined(GRAPHENE_COMPILATION)
    #error "Only <graphene.h> can be included directly."
    #endif

    #include "graphene-types.h"

    GRAPHENE_BEGIN_DECLS

    ...

    GRAPHENE_END_DECLS

    #endif /* __GRAPHENE_HEADER_H__ */

Private headers do not need the single header inclusion guard, but they need
every other guard. Private headers should also be clearly named with a
`-private` suffix. Private headers should include the public header for the
type they relate to, first:

    #ifndef __GRAPHENE_FOO_PRIVATE_H__
    #define __GRAPHENE_FOO_PRIVATE_H__

    #include "graphene-foo.h"

    GRAPHENE_BEGIN_DECLS

    ...

    GRAPHENE_END_DECLS

    #endif

### Includes

Graphene source files should never include the global `graphene.h` header, but
instead include the individual headers that are needed. Every file must
include `graphene-private.h` first, then its own header, then other Graphene
header in an alphabetical order, then system and third-party headers.

    /* valid */
    #include "graphene-private.h"

    #include "graphene-foo.h"

    #include "graphene-box.h"
    #include "graphene-point3d.h"
    #include "graphene-vec3.h"

    ...

    #include <math.h>

### Macros

Try to avoid private macros unless strictly necessary. Remember to #undef
them at the end of a block or a series of functions needing them.

Inline functions are usually preferable to private macros.

Public macros should not be used unless they evaluate to a constant.

### Documentation

All public APIs must have gtk-doc comments. For functions, these should
be placed in the source file, directly above the function.

    /* valid */
    /**
     * graphene_type_get_property:
     * @type: a #graphene_type_t
     *
     * Gets the property of a #graphene_type_t.
     *
     * Returns: the value of the property
     */
    float
    graphene_type_get_property (graphene_type_t *type)
    {
      ...
    }

Doc comments for macros, function types, structures, etc should be
placed next to the definitions, typically in headers.

Section introductions should be placed in the source file they describe,
after the license header:

    /* valid */
    /**
     * SECTION:graphene-type
     * @Title: Type
     * @Short_description: A representation of a type
     *
     * [...]
     */

To properly document a new function, macro, function type or struct,
it needs to be listed in the graphene-sections.txt file.

To properly document a new type, it needs to be given its own section
in `graphene-sections.txt`, and it needs to be included in `graphene-docs.xml`.

### Old code

It is ok to update the style of a code block or function when you
are touching it anyway, but sweeping whitespace changes obscure the
revision control history and should be avoided at all costs.
