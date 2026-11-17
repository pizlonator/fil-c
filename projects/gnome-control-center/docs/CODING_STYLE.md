# Style

GNOME Settings has a coding style based on GTK Coding Style, but with a few more
rules. Please read them carefully and, if in doubt, ask a maintainer for directions.

## General

The most important rule is: **see the surrounding code, and copy its style**.

That said, GNOME Settings assumes:

 * 2 spaces of indentation
 * 120 columns of line width
 * Newline before `{`

Another rule that applies to function declarations is that all parameters are
aligned by the last '*'. There are plenty of examples below.

## Comments

Comment blocks should be formatted as following:

```c
/* Single line comment */

/* Multiline comments start at the first line of the comment block,
 * but have the closing slash a line after. Every line starts with
 * an asterisk that is aligned with every the rest of the block.
 */
```

## Conditionals

Conditionals should either be all in one line, or one per line. Newlines inside
conditionals are aligned by the last parenthesis.


Some examples below:

```c
// Single line if
if (a || b || (c && d))
  return;

// Multiline if with nested parenthesis
if (long_boolean_variable_used_in_this_condition_a ||
    long_boolean_variable_used_in_this_condition_b ||
    (long_boolean_variable_used_in_this_condition_c &&
     long_boolean_variable_used_in_this_condition_d))
  {
    return;
  }

// Another single line example with do {} while (...)
do
  {
    /* something */
  }
while (a || b || (c && d));
```

## Structs and Enums

Structures and enums are formatted as following:

```c
struct _FooBar
{
  guint32    field_one;
  gchar     *text;
};

typedef struct
{
  FooParent     parent;

  guint32       field_one;
  gchar        *text;

  struct
  {
    CustomType *something;
    guint       something_else;
  } inner_struct;

  gboolean      flag : 1;
} FooBar;

enum
{
  FIRST,
  SECOND,
  LAST,
};

typedef enum
{
  FOO_BAR_FIRST,
  FOO_BAR_SECOND,
  FOO_BAR_LAST,
} FooEnumBar;
```

## Header (.h) files

It is organized by the following structure:

 1. GPL header
 2. Local includes
 3. System includes
 4. `G_BEGIN_DECLS`
 5. `#defines`
 6. `G_DECLARE_{FINAL,DERIVABLE}_TYPE`
 7. Public API
 8. `G_END_DECLS`

The following style rules apply:

 * The '*' and the type come together, without any spaces in between.
 * Function names are aligned by the widest return value.
 * Parenthesis after function name is aligned by the widest function name
 * The last '*' in parameters are aligned by the widest parameter type
 * No new line at the end of the file

As an example, this is how a header file should look like (extracted from
the `cc-object-storage.h` file):

```c
/* cc-object-storage.h
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* Default storage keys */
#define CC_OBJECT_NMCLIENT  "CcObjectStorage::nm-client"


#define CC_TYPE_OBJECT_STORAGE (cc_object_storage_get_type())

G_DECLARE_FINAL_TYPE (CcObjectStorage, cc_object_storage, CC, OBJECT_STORAGE, GObject)

gboolean cc_object_storage_has_object             (const gchar         *key);

void     cc_object_storage_add_object             (const gchar         *key,
                                                   gpointer             object);

gpointer cc_object_storage_get_object             (const gchar         *key);

gpointer cc_object_storage_create_dbus_proxy_sync (GBusType             bus_type,
                                                   GDBusProxyFlags     flags,
                                                   const gchar         *name,
                                                   const gchar         *path,
                                                   const gchar         *interface,
                                                   GCancellable        *cancellable,
                                                   GError             **error);

void     cc_object_storage_create_dbus_proxy      (GBusType              bus_type,
                                                   GDBusProxyFlags       flags,
                                                   const gchar          *name,
                                                   const gchar          *path,
                                                   const gchar          *interface,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);

G_END_DECLS
```

## Source code

The source file keeps an order of methods. The order will be as following:

  1. GPL header
  2. Structures
  3. Function prototypes
  4. G_DEFINE_TYPE()
  5. Enums
  6. Static variables
  7. Auxiliary methods
  8. Callbacks
  9. Interface implementations
  10. Parent class overrides
  11. class_init and init
  12. Public API

### Structures

The structures must have the first pointer asterisk aligned one space after the
widest type name. For example:

```c
typedef struct
{
  GBusType         bus_type;
  GDBusProxyFlags  flags;
  gchar           *name;
  gchar           *path;
  gchar           *interface;
  gboolean         cached;
} TaskData;

```

### Function Prototypes

Function prototypes must be formatted just like in header files.

### Auxiliary Methods

Auxiliary method names must have a verb in the dictionary form, and should always
perform an action over something. They don't have the `cc_` prefix. For example:

```c
static void
execute_something_on_data (Foo *data,
                           Bar *bar)
{
  /* ... */
}
```

### Callbacks

 * Callbacks always have the `_cb` suffix
 * Signal callbacks always have the `on_<object_name>` prefix
 * Callback names must have the name of the signal in the past

For example:

```c
static void
on_foo_size_allocated_cb (GtkWidget     *widget,
                          GtkAllocation *allocation,
                          gpointer       user_data)
{
  /* ... */
}
```

### Line Splitting

Line splitting works following the GTK code style, but legibility comes over above
all. If a function call looks unbalanced following the GTK style, it is fine to
slightly escape the rules.

For example, this feels extremelly unbalanced:

```c
foo_bar_do_somthing_sync (a,
                          1,
                          object,
                          data,
                          something
                          cancellable,
                          &error);
```

Notice the empty space before the arguments, and how empty and odd it looks. In
comparison, it will look better if written like this:

```c
foo_bar_do_somthing_sync (a, 1, object, data,
                          something
                          cancellable,
                          &error);
```

# Contributing guidelines

See CONTRIBUTIONS.md file for the contribution guidelines, and the Code of Conduct
that contributors are expected to follow.