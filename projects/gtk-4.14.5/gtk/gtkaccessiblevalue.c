/* gtkaccessiblevalue.c: Accessible value
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*< private >
 * GtkAccessibleValue:
 *
 * GtkAccessibleValue is a reference counted, generic container for values used
 * to represent the state and properties of a `GtkAccessible` implementation.
 *
 * There are two kinds of accessible value types:
 *
 *  - hard coded, static values; GTK owns them, and their contents, and are
 *    guaranteed to exist for the duration of the application's life time
 *  - dynamic values; the accessible state owns the value and their contents,
 *    and they can be allocated and freed
 *
 * Typically, the former type of values is used for boolean, tristate, and
 * token value; the latter is used for numbers, strings, and token lists.
 *
 * For more information on the types of values, see the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/states_and_properties#propcharacteristic_value)
 * reference.
 */

#include "config.h"

#include "gtkaccessiblevalueprivate.h"

#include "gtkaccessible.h"
#include "gtkbuilderprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"

#include <math.h>
#include <float.h>
#include <errno.h>

G_DEFINE_QUARK (gtk-accessible-value-error-quark, gtk_accessible_value_error)

G_DEFINE_BOXED_TYPE (GtkAccessibleValue, gtk_accessible_value,
                     gtk_accessible_value_ref,
                     gtk_accessible_value_unref)

/*< private >
 * gtk_accessible_value_alloc:
 * @value_class: a `GtkAccessibleValueClass` structure
 *
 * Allocates a new `GtkAccessibleValue` subclass using @value_class as the
 * type definition.
 *
 * Returns: (transfer full): the newly allocated `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_alloc (const GtkAccessibleValueClass *value_class)
{
  g_return_val_if_fail (value_class != NULL, NULL);
  g_return_val_if_fail (value_class->instance_size >= sizeof (GtkAccessibleValue), NULL);

  GtkAccessibleValue *res = g_malloc0 (value_class->instance_size);

  /* We do not use grefcount, here, because we want to have statically
   * allocated GtkAccessibleValue subclasses, and those cannot be initialized
   * with g_ref_count_init()
   */
  res->ref_count = 1;
  res->value_class = value_class;

  if (res->value_class->init != NULL)
    res->value_class->init (res);

  return res;
}

/*< private >
 * gtk_accessible_value_ref:
 * @self: a `GtkAccessibleValue`
 *
 * Acquires a reference on the given `GtkAccessibleValue`.
 *
 * Returns: (transfer full): the value, with an additional reference
 */
GtkAccessibleValue *
gtk_accessible_value_ref (GtkAccessibleValue *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count += 1;

  return self;
}

/*< private >
 * gtk_accessible_value_unref:
 * @self: (transfer full): a `GtkAccessibleValue`
 *
 * Releases a reference on the given `GtkAccessibleValue`.
 */
void
gtk_accessible_value_unref (GtkAccessibleValue *self)
{
  g_return_if_fail (self != NULL);

  self->ref_count -= 1;
  if (self->ref_count == 0)
    {
      if (self->value_class->finalize != NULL)
        self->value_class->finalize (self);

      g_free (self);
    }
}

/*< private >
 * gtk_accessible_value_print:
 * @self: a `GtkAccessibleValue`
 * @buffer: a `GString`
 *
 * Prints the contents of a `GtkAccessibleValue` into the given @buffer.
 */
void
gtk_accessible_value_print (const GtkAccessibleValue *self,
                            GString                  *buffer)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (buffer != NULL);

  if (self->value_class->print != NULL)
    self->value_class->print (self, buffer);
}

/*< private >
 * gtk_accessible_value_to_string:
 * @self: a `GtkAccessibleValue`
 *
 * Fills a string with the contents of the given `GtkAccessibleValue`.
 *
 * Returns: (transfer full): a string with the contents of the value
 */
char *
gtk_accessible_value_to_string (const GtkAccessibleValue *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  GString *buffer = g_string_new (NULL);

  gtk_accessible_value_print (self, buffer);

  return g_string_free (buffer, FALSE);
}

/*< private >
 * gtk_accessible_value_equal:
 * @value_a: (nullable): the first `GtkAccessibleValue`
 * @value_b: (nullable): the second `GtkAccessibleValue`
 *
 * Checks whether @value_a and @value_b are equal.
 *
 * This function is %NULL-safe.
 *
 * Returns: %TRUE if the given `GtkAccessibleValue` instances are equal,
 *   and %FALSE otherwise
 */
gboolean
gtk_accessible_value_equal (const GtkAccessibleValue *value_a,
                            const GtkAccessibleValue *value_b)
{
  if (value_a == value_b)
    return TRUE;

  if (value_a == NULL || value_b == NULL)
    return FALSE;

  if (value_a->value_class != value_b->value_class)
    return FALSE;

  if (value_a->value_class->equal == NULL)
    return FALSE;

  return value_a->value_class->equal (value_a, value_b);
}

/* {{{ Basic allocated types */

typedef struct {
  GtkAccessibleValue parent;

  int value;
} GtkIntAccessibleValue;

static gboolean
gtk_int_accessible_value_equal (const GtkAccessibleValue *value_a,
                                const GtkAccessibleValue *value_b)
{
  const GtkIntAccessibleValue *self_a = (GtkIntAccessibleValue *) value_a;
  const GtkIntAccessibleValue *self_b = (GtkIntAccessibleValue *) value_b;

  return self_a->value == self_b->value;
}

static void
gtk_int_accessible_value_print (const GtkAccessibleValue *value,
                                GString                  *buffer)
{
  const GtkIntAccessibleValue *self = (GtkIntAccessibleValue *) value;

  g_string_append_printf (buffer, "%d", self->value);
}

static const GtkAccessibleValueClass GTK_INT_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_INTEGER,
  .type_name = "GtkIntAccessibleValue",
  .instance_size = sizeof (GtkIntAccessibleValue),
  .equal = gtk_int_accessible_value_equal,
  .print = gtk_int_accessible_value_print,
};

GtkAccessibleValue *
gtk_int_accessible_value_new (int value)
{
  GtkAccessibleValue *res = gtk_accessible_value_alloc (&GTK_INT_ACCESSIBLE_VALUE);

  /* XXX: Possible optimization: statically allocate the first N values
   *      and hand out references to them, instead of dynamically
   *      allocating a new GtkAccessibleValue instance. Needs some profiling
   *      to figure out the common integer values used by large applications
   */
  GtkIntAccessibleValue *self = (GtkIntAccessibleValue *) res;
  self->value = value;

  return res;
}

int
gtk_int_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkIntAccessibleValue *self = (GtkIntAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->value_class == &GTK_INT_ACCESSIBLE_VALUE, 0);

  return self->value;
}

typedef struct {
  GtkAccessibleValue parent;

  double value;
} GtkNumberAccessibleValue;

static gboolean
gtk_number_accessible_value_equal (const GtkAccessibleValue *value_a,
                                   const GtkAccessibleValue *value_b)
{
  const GtkNumberAccessibleValue *self_a = (GtkNumberAccessibleValue *) value_a;
  const GtkNumberAccessibleValue *self_b = (GtkNumberAccessibleValue *) value_b;

  return G_APPROX_VALUE (self_a->value, self_b->value, 0.001);
}

static void
gtk_number_accessible_value_print (const GtkAccessibleValue *value,
                                   GString                  *buffer)
{
  const GtkNumberAccessibleValue *self = (GtkNumberAccessibleValue *) value;

  g_string_append_printf (buffer, "%g", self->value);
}

static const GtkAccessibleValueClass GTK_NUMBER_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_NUMBER,
  .type_name = "GtkNumberAccessibleValue",
  .instance_size = sizeof (GtkNumberAccessibleValue),
  .equal = gtk_number_accessible_value_equal,
  .print = gtk_number_accessible_value_print,
};

GtkAccessibleValue *
gtk_number_accessible_value_new (double value)
{
  GtkAccessibleValue *res = gtk_accessible_value_alloc (&GTK_NUMBER_ACCESSIBLE_VALUE);

  GtkNumberAccessibleValue *self = (GtkNumberAccessibleValue *) res;
  self->value = value;

  return res;
}

double
gtk_number_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkNumberAccessibleValue *self = (GtkNumberAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->value_class == &GTK_NUMBER_ACCESSIBLE_VALUE, 0);

  return self->value;
}

typedef struct {
  GtkAccessibleValue parent;

  char *value;
  gsize length;
} GtkStringAccessibleValue;

static void
gtk_string_accessible_value_finalize (GtkAccessibleValue *value)
{
  GtkStringAccessibleValue *self = (GtkStringAccessibleValue *) value;

  g_free (self->value);
}

static gboolean
gtk_string_accessible_value_equal (const GtkAccessibleValue *value_a,
                                   const GtkAccessibleValue *value_b)
{
  const GtkStringAccessibleValue *self_a = (GtkStringAccessibleValue *) value_a;
  const GtkStringAccessibleValue *self_b = (GtkStringAccessibleValue *) value_b;

  if (self_a->length != self_b->length)
    return FALSE;

  return g_strcmp0 (self_a->value, self_b->value) == 0;
}

static void
gtk_string_accessible_value_print (const GtkAccessibleValue *value,
                                   GString                  *buffer)
{
  const GtkStringAccessibleValue *self = (GtkStringAccessibleValue *) value;

  g_string_append (buffer, self->value);
}

static const GtkAccessibleValueClass GTK_STRING_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_STRING,
  .type_name = "GtkStringAccessibleValue",
  .instance_size = sizeof (GtkStringAccessibleValue),
  .finalize = gtk_string_accessible_value_finalize,
  .equal = gtk_string_accessible_value_equal,
  .print = gtk_string_accessible_value_print,
};

GtkAccessibleValue *
gtk_string_accessible_value_new (const char *str)
{
  GtkAccessibleValue *res = gtk_accessible_value_alloc (&GTK_STRING_ACCESSIBLE_VALUE);

  GtkStringAccessibleValue *self = (GtkStringAccessibleValue *) res;

  self->value = g_strdup (str);
  self->length = strlen (str);

  return res;
}

const char *
gtk_string_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkStringAccessibleValue *self = (GtkStringAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->value_class == &GTK_STRING_ACCESSIBLE_VALUE, 0);

  return self->value;
}

typedef struct {
  GtkAccessibleValue parent;

  GtkAccessible *ref;
} GtkReferenceAccessibleValue;

static void
remove_weak_ref (gpointer  data,
                 GObject  *old_reference)
{
  GtkReferenceAccessibleValue *self = data;

  self->ref = NULL;
}

static void
gtk_reference_accessible_value_finalize (GtkAccessibleValue *value)
{
  GtkReferenceAccessibleValue *self = (GtkReferenceAccessibleValue *) value;

  if (self->ref != NULL)
    g_object_weak_unref (G_OBJECT (self->ref), remove_weak_ref, self);
}

static gboolean
gtk_reference_accessible_value_equal (const GtkAccessibleValue *value_a,
                                      const GtkAccessibleValue *value_b)
{
  const GtkReferenceAccessibleValue *self_a = (GtkReferenceAccessibleValue *) value_a;
  const GtkReferenceAccessibleValue *self_b = (GtkReferenceAccessibleValue *) value_b;

  return self_a->ref == self_b->ref;
}

static void
gtk_reference_accessible_value_print (const GtkAccessibleValue *value,
                                      GString                  *buffer)
{
  const GtkReferenceAccessibleValue *self = (GtkReferenceAccessibleValue *) value;

  if (self->ref != NULL)
    {
      g_string_append_printf (buffer, "%s<%p>",
                              G_OBJECT_TYPE_NAME (self->ref),
                              self->ref);
    }
  else
    {
      g_string_append (buffer, "<null>");
    }
}

static const GtkAccessibleValueClass GTK_REFERENCE_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_REFERENCE,
  .type_name = "GtkReferenceAccessibleValue",
  .instance_size = sizeof (GtkReferenceAccessibleValue),
  .finalize = gtk_reference_accessible_value_finalize,
  .equal = gtk_reference_accessible_value_equal,
  .print = gtk_reference_accessible_value_print,
};

GtkAccessibleValue *
gtk_reference_accessible_value_new (GtkAccessible *ref)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (ref), NULL);

  GtkAccessibleValue *res = gtk_accessible_value_alloc (&GTK_REFERENCE_ACCESSIBLE_VALUE);

  GtkReferenceAccessibleValue *self = (GtkReferenceAccessibleValue *) res;

  self->ref = ref;
  g_object_weak_ref (G_OBJECT (self->ref), remove_weak_ref, self);

  return res;
}

GtkAccessible *
gtk_reference_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkReferenceAccessibleValue *self = (GtkReferenceAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->value_class == &GTK_REFERENCE_ACCESSIBLE_VALUE, 0);

  return self->ref;
}

typedef struct {
  GtkAccessibleValue parent;

  GList *refs;
} GtkReferenceListAccessibleValue;

static void
remove_weak_ref_from_list (gpointer  data,
                           GObject  *old_reference)
{
  GtkReferenceListAccessibleValue *self = data;

  GList *item = g_list_find (self->refs, old_reference);

  if (item != NULL)
    {
      self->refs = g_list_remove_link (self->refs, item);
      g_list_free (item);
    }
}

static void
gtk_reference_list_accessible_value_finalize (GtkAccessibleValue *value)
{
  GtkReferenceListAccessibleValue *self = (GtkReferenceListAccessibleValue *) value;

  for (GList *l = self->refs; l != NULL; l = l->next)
    {
      if (l->data != NULL)
        g_object_weak_unref (G_OBJECT (l->data), remove_weak_ref_from_list, self);
    }

  g_list_free (self->refs);
}

static gboolean
gtk_reference_list_accessible_value_equal (const GtkAccessibleValue *value_a,
                                           const GtkAccessibleValue *value_b)
{
  const GtkReferenceListAccessibleValue *self_a = (GtkReferenceListAccessibleValue *) value_a;
  const GtkReferenceListAccessibleValue *self_b = (GtkReferenceListAccessibleValue *) value_b;

  if (g_list_length (self_a->refs) != g_list_length (self_b->refs))
    return FALSE;

  for (GList *l = self_a->refs; l != NULL; l = l->next)
    {
      if (g_list_find (self_b->refs, l->data) == NULL)
        return FALSE;
    }

  return TRUE;
}

static void
gtk_reference_list_accessible_value_print (const GtkAccessibleValue *value,
                                           GString                  *buffer)
{
  const GtkReferenceListAccessibleValue *self = (GtkReferenceListAccessibleValue *) value;

  if (self->refs == NULL)
    {
      g_string_append (buffer, "<null>");
      return;
    }

  for (GList *l = self->refs; l != NULL; l = l->next)
    {
      g_string_append_printf (buffer, "%s<%p>",
                              G_OBJECT_TYPE_NAME (l->data),
                              l->data);
    }
}

static const GtkAccessibleValueClass GTK_REFERENCE_LIST_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_REFERENCE_LIST,
  .type_name = "GtkReferenceListAccessibleValue",
  .instance_size = sizeof (GtkReferenceListAccessibleValue),
  .finalize = gtk_reference_list_accessible_value_finalize,
  .equal = gtk_reference_list_accessible_value_equal,
  .print = gtk_reference_list_accessible_value_print,
};

/*< private >
 * gtk_reference_list_accessible_value_new:
 * @value: (element-type GtkAccessible) (transfer full): a list of accessible objects
 *
 * Creates a new `GtkAccessible` that stores a list of references
 * to `GtkAccessible` objects.
 *
 * Returns: (transfer full): the newly created `GtkAccessible`
 */
GtkAccessibleValue *
gtk_reference_list_accessible_value_new (GList *value)
{
  GtkAccessibleValue *res = gtk_accessible_value_alloc (&GTK_REFERENCE_LIST_ACCESSIBLE_VALUE);

  GtkReferenceListAccessibleValue *self = (GtkReferenceListAccessibleValue *) res;

  self->refs = value;
  if (self->refs != NULL)
    {
      for (GList *l = self->refs; l != NULL; l = l->next)
        g_object_weak_ref (l->data, remove_weak_ref_from_list, self);
    }

  return res;
}

GList *
gtk_reference_list_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkReferenceListAccessibleValue *self = (GtkReferenceListAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, 0);
  g_return_val_if_fail (value->value_class == &GTK_REFERENCE_LIST_ACCESSIBLE_VALUE, 0);

  return self->refs;
}

/* }}} */

/* {{{ Collection API */

typedef enum {
  GTK_ACCESSIBLE_COLLECT_INVALID = -1,
  
  /* true/false */
  GTK_ACCESSIBLE_COLLECT_BOOLEAN = 0,

  /* true/false/mixed/undefined */
  GTK_ACCESSIBLE_COLLECT_TRISTATE,

  /* one token */
  GTK_ACCESSIBLE_COLLECT_TOKEN,

  /* integer number */
  GTK_ACCESSIBLE_COLLECT_INTEGER,

  /* real number */
  GTK_ACCESSIBLE_COLLECT_NUMBER,

  /* string */
  GTK_ACCESSIBLE_COLLECT_STRING,

  /* reference */
  GTK_ACCESSIBLE_COLLECT_REFERENCE,

  /* references list */
  GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,

  /* allows collecting GTK_ACCESSIBLE_VALUE_UNDEFINED; implied
   * by GTK_ACCESSIBLE_COLLECT_TRISTATE
   */
  GTK_ACCESSIBLE_COLLECT_UNDEFINED = 1 << 16
} GtkAccessibleCollectType;

typedef struct {
  int value;
  GtkAccessibleCollectType ctype;
  const char *name;

  /* The constructor and getter will be derived by the
   * @ctype field and by the collected value, except for the
   * GTK_ACCESSIBLE_COLLECT_TOKEN collection type. You can
   * override the default ones by filling out these two
   * pointers
   */
  GCallback ctor;
  GCallback getter;
  GCallback parser;
  GCallback init_value;
} GtkAccessibleCollect;

static const GtkAccessibleCollect collect_states[] = {
  [GTK_ACCESSIBLE_STATE_BUSY] = {
    .value = GTK_ACCESSIBLE_STATE_BUSY,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "busy"
  },
  [GTK_ACCESSIBLE_STATE_CHECKED] = {
    .value = GTK_ACCESSIBLE_STATE_CHECKED,
    .ctype = GTK_ACCESSIBLE_COLLECT_TRISTATE,
    .name = "checked"
  },
  [GTK_ACCESSIBLE_STATE_DISABLED] = {
    .value = GTK_ACCESSIBLE_STATE_DISABLED,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "disabled"
  },
  [GTK_ACCESSIBLE_STATE_EXPANDED] = {
    .value = GTK_ACCESSIBLE_STATE_EXPANDED,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN | GTK_ACCESSIBLE_COLLECT_UNDEFINED,
    .name = "expanded"
  },
  [GTK_ACCESSIBLE_STATE_HIDDEN] = {
    .value = GTK_ACCESSIBLE_STATE_HIDDEN,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "hidden"
  },
  [GTK_ACCESSIBLE_STATE_INVALID] = {
    .value = GTK_ACCESSIBLE_STATE_INVALID,
    .ctype = GTK_ACCESSIBLE_COLLECT_TOKEN,
    .name = "invalid",
    .ctor = (GCallback) gtk_invalid_accessible_value_new,
    .getter = (GCallback) gtk_invalid_accessible_value_get,
    .parser = (GCallback) gtk_invalid_accessible_value_parse,
    .init_value = (GCallback) gtk_invalid_accessible_value_init_value,
  },
  [GTK_ACCESSIBLE_STATE_PRESSED] = {
    .value = GTK_ACCESSIBLE_STATE_PRESSED,
    .ctype = GTK_ACCESSIBLE_COLLECT_TRISTATE,
    .name = "pressed"
  },
  [GTK_ACCESSIBLE_STATE_SELECTED] = {
    .value = GTK_ACCESSIBLE_STATE_SELECTED,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN | GTK_ACCESSIBLE_COLLECT_UNDEFINED,
    .name = "selected"
  },
  [GTK_ACCESSIBLE_STATE_VISITED] = {
    .value = GTK_ACCESSIBLE_STATE_VISITED,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN|GTK_ACCESSIBLE_COLLECT_UNDEFINED,
    .name = "visited"
  }
};

/* § 6.6.1 Widget attributes */
static const GtkAccessibleCollect collect_props[] = {
  [GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE] = {
    .value = GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE,
    .ctype = GTK_ACCESSIBLE_COLLECT_TOKEN,
    .name = "autocomplete",
    .ctor = (GCallback) gtk_autocomplete_accessible_value_new,
    .getter = (GCallback) gtk_autocomplete_accessible_value_get,
    .parser = (GCallback) gtk_autocomplete_accessible_value_parse,
    .init_value = (GCallback) gtk_autocomplete_accessible_value_init_value,
  },
  [GTK_ACCESSIBLE_PROPERTY_DESCRIPTION] = {
    .value = GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "description"
  },
  [GTK_ACCESSIBLE_PROPERTY_HAS_POPUP] = {
    .value = GTK_ACCESSIBLE_PROPERTY_HAS_POPUP,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "haspopup"
  },
  [GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS] = {
    .value = GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "keyshortcuts"
  },
  [GTK_ACCESSIBLE_PROPERTY_LABEL] = {
    .value = GTK_ACCESSIBLE_PROPERTY_LABEL,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "label"
  },
  [GTK_ACCESSIBLE_PROPERTY_LEVEL] = {
    .value = GTK_ACCESSIBLE_PROPERTY_LEVEL,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "level"
  },
  [GTK_ACCESSIBLE_PROPERTY_MODAL] = {
    .value = GTK_ACCESSIBLE_PROPERTY_MODAL,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "modal"
  },
  [GTK_ACCESSIBLE_PROPERTY_MULTI_LINE] = {
    .value = GTK_ACCESSIBLE_PROPERTY_MULTI_LINE,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "multiline"
  },
  [GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE] = {
    .value = GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "multiselectable"
  },
  /* "orientation" is a bit special; it maps to GtkOrientation, but it
   * can also be "undefined". This means we need to override constructor
   * and getter, in order to properly handle GTK_ACCESSIBLE_VALUE_UNDEFINED
   */
  [GTK_ACCESSIBLE_PROPERTY_ORIENTATION] = {
    .value = GTK_ACCESSIBLE_PROPERTY_ORIENTATION,
    .ctype = GTK_ACCESSIBLE_COLLECT_TOKEN | GTK_ACCESSIBLE_COLLECT_UNDEFINED,
    .name = "orientation",
    .ctor = (GCallback) gtk_orientation_accessible_value_new,
    .getter = (GCallback) gtk_orientation_accessible_value_get,
    .parser = (GCallback) gtk_orientation_accessible_value_parse,
    .init_value = (GCallback) gtk_orientation_accessible_value_init_value,
  },
  [GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER] = {
    .value = GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "placeholder"
  },
  [GTK_ACCESSIBLE_PROPERTY_READ_ONLY] = {
    .value = GTK_ACCESSIBLE_PROPERTY_READ_ONLY,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "readonly"
  },
  [GTK_ACCESSIBLE_PROPERTY_REQUIRED] = {
    .value = GTK_ACCESSIBLE_PROPERTY_REQUIRED,
    .ctype = GTK_ACCESSIBLE_COLLECT_BOOLEAN,
    .name = "required"
  },
  [GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION] = {
    .value = GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "roledescription"
  },
  [GTK_ACCESSIBLE_PROPERTY_SORT] = {
    .value = GTK_ACCESSIBLE_PROPERTY_SORT,
    .ctype = GTK_ACCESSIBLE_COLLECT_TOKEN,
    .name = "sort",
    .ctor = (GCallback) gtk_sort_accessible_value_new,
    .getter = (GCallback) gtk_sort_accessible_value_get,
    .parser = (GCallback) gtk_sort_accessible_value_parse,
    .init_value = (GCallback) gtk_sort_accessible_value_init_value,
  },
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MAX] = {
    .value = GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
    .ctype = GTK_ACCESSIBLE_COLLECT_NUMBER,
    .name = "valuemax"
  },
  [GTK_ACCESSIBLE_PROPERTY_VALUE_MIN] = {
    .value = GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
    .ctype = GTK_ACCESSIBLE_COLLECT_NUMBER,
    .name = "valuemin"
  },
  [GTK_ACCESSIBLE_PROPERTY_VALUE_NOW] = {
    .value = GTK_ACCESSIBLE_PROPERTY_VALUE_NOW,
    .ctype = GTK_ACCESSIBLE_COLLECT_NUMBER,
    .name = "valuenow"
  },
  [GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT] = {
    .value = GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "valuetext"
  },
};

/* § 6.6.4 Relationship Attributes */
static const GtkAccessibleCollect collect_rels[] = {
  [GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT] = {
    .value = GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE,
    .name = "activedescendant"
  },
  [GTK_ACCESSIBLE_RELATION_COL_COUNT] = {
    .value = GTK_ACCESSIBLE_RELATION_COL_COUNT,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "colcount"
  },
  [GTK_ACCESSIBLE_RELATION_COL_INDEX] = {
    .value = GTK_ACCESSIBLE_RELATION_COL_INDEX,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "colindex"
  },
  [GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT] = {
    .value = GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "colindextext"
  },
  [GTK_ACCESSIBLE_RELATION_COL_SPAN] = {
    .value = GTK_ACCESSIBLE_RELATION_COL_SPAN,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "colspan"
  },
  [GTK_ACCESSIBLE_RELATION_CONTROLS] = {
    .value = GTK_ACCESSIBLE_RELATION_CONTROLS,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "controls"
  },
  [GTK_ACCESSIBLE_RELATION_DESCRIBED_BY] = {
    .value = GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "describedby"
  },
  [GTK_ACCESSIBLE_RELATION_DETAILS] = {
    .value = GTK_ACCESSIBLE_RELATION_DETAILS,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "details"
  },
  [GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE] = {
    .value = GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE,
    .name = "errormessage"
  },
  [GTK_ACCESSIBLE_RELATION_FLOW_TO] = {
    .value = GTK_ACCESSIBLE_RELATION_FLOW_TO,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "flowto"
  },
  [GTK_ACCESSIBLE_RELATION_LABELLED_BY] = {
    .value = GTK_ACCESSIBLE_RELATION_LABELLED_BY,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "labelledby"
  },
  [GTK_ACCESSIBLE_RELATION_OWNS] = {
    .value = GTK_ACCESSIBLE_RELATION_OWNS,
    .ctype = GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST,
    .name = "owns"
  },
  [GTK_ACCESSIBLE_RELATION_POS_IN_SET] = {
    .value = GTK_ACCESSIBLE_RELATION_POS_IN_SET,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "posinset"
  },
  [GTK_ACCESSIBLE_RELATION_ROW_COUNT] = {
    .value = GTK_ACCESSIBLE_RELATION_ROW_COUNT,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "rowcount"
  },
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX] = {
    .value = GTK_ACCESSIBLE_RELATION_ROW_INDEX,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "rowindex"
  },
  [GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT] = {
    .value = GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT,
    .ctype = GTK_ACCESSIBLE_COLLECT_STRING,
    .name = "rowindextext"
  },
  [GTK_ACCESSIBLE_RELATION_ROW_SPAN] = {
    .value = GTK_ACCESSIBLE_RELATION_ROW_SPAN,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "rowspan"
  },
  [GTK_ACCESSIBLE_RELATION_SET_SIZE] = {
    .value = GTK_ACCESSIBLE_RELATION_SET_SIZE,
    .ctype = GTK_ACCESSIBLE_COLLECT_INTEGER,
    .name = "posinset"
  },
};

typedef GtkAccessibleValue * (* GtkAccessibleValueBooleanCtor)  (gboolean value);
typedef GtkAccessibleValue * (* GtkAccessibleValueIntCtor)      (int value);
typedef GtkAccessibleValue * (* GtkAccessibleValueTristateCtor) (int value);
typedef GtkAccessibleValue * (* GtkAccessibleValueEnumCtor)     (int value);
typedef GtkAccessibleValue * (* GtkAccessibleValueNumberCtor)   (double value);
typedef GtkAccessibleValue * (* GtkAccessibleValueStringCtor)   (const char *value);
typedef GtkAccessibleValue * (* GtkAccessibleValueRefCtor)      (GtkAccessible *value);
typedef GtkAccessibleValue * (* GtkAccessibleValueRefListCtor)  (GList *value);

typedef GtkAccessibleValue * (* GtkAccessibleValueEnumParser)   (const char  *str,
                                                                 gsize        len,
                                                                 GError     **error);
typedef void                 (* GtkAccessibleValueEnumInit)     (GValue      *value);

/*< private >
 * gtk_accessible_value_get_default_for_state:
 * @state: a `GtkAccessibleState`
 *
 * Retrieves the `GtkAccessibleValue` that contains the default for the
 * given @state.
 *
 * Returns: (transfer full): the `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_get_default_for_state (GtkAccessibleState state)
{
  const GtkAccessibleCollect *cstate = &collect_states[state];

  g_return_val_if_fail (state <= GTK_ACCESSIBLE_STATE_VISITED, NULL);

  switch (cstate->value)
    {
    case GTK_ACCESSIBLE_STATE_BUSY:
    case GTK_ACCESSIBLE_STATE_DISABLED:
    case GTK_ACCESSIBLE_STATE_HIDDEN:
      return gtk_boolean_accessible_value_new (FALSE);

    case GTK_ACCESSIBLE_STATE_CHECKED:
    case GTK_ACCESSIBLE_STATE_EXPANDED:
    case GTK_ACCESSIBLE_STATE_PRESSED:
    case GTK_ACCESSIBLE_STATE_SELECTED:
    case GTK_ACCESSIBLE_STATE_VISITED:
      return gtk_undefined_accessible_value_new ();

    case GTK_ACCESSIBLE_STATE_INVALID:
      return gtk_invalid_accessible_value_new (GTK_ACCESSIBLE_INVALID_FALSE);

    default:
      g_critical ("Unknown value for accessible state “%s”", cstate->name);
      break;
    }

  return NULL;
}

static GtkAccessibleValue *
gtk_accessible_value_collect_valist (const GtkAccessibleCollect  *cstate,
                                     GError                     **error,
                                     va_list                     *args)
{
  GtkAccessibleValue *res = NULL;
  GtkAccessibleCollectType ctype = cstate->ctype;
  gboolean collects_undef = (ctype & GTK_ACCESSIBLE_COLLECT_UNDEFINED) != 0;

  ctype &= (GTK_ACCESSIBLE_COLLECT_UNDEFINED - 1);

  /* Tristate values include "undefined" by definition */
  if (ctype == GTK_ACCESSIBLE_COLLECT_TRISTATE)
    collects_undef = TRUE;

  switch (ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        if (collects_undef)
          {
            int value = va_arg (*args, int);

            if (value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
              res = gtk_undefined_accessible_value_new ();
            else
              res = gtk_boolean_accessible_value_new (value == 0 ? FALSE : TRUE);
          }
        else
          {
            gboolean value = va_arg (*args, gboolean);

            res = gtk_boolean_accessible_value_new (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      {
        int value = va_arg (*args, int);

        if (collects_undef && value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
          res = gtk_undefined_accessible_value_new ();
        else
          res = gtk_tristate_accessible_value_new (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TOKEN:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value = va_arg (*args, int);

        if (collects_undef && value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
          {
            res = gtk_undefined_accessible_value_new ();
          }
        else
          {
            /* Token collection requires a constructor */
            g_assert (ctor != NULL);

            res = (* ctor) (value);
          }

        if (res == NULL)
          {
            g_set_error (error, GTK_ACCESSIBLE_VALUE_ERROR,
                         GTK_ACCESSIBLE_VALUE_ERROR_INVALID_TOKEN,
                         "Invalid value for token attribute: %d",
                         value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value = va_arg (*args, int);

        if (ctor == NULL)
          res = gtk_int_accessible_value_new (value);
        else
          res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_NUMBER:
      {
        GtkAccessibleValueNumberCtor ctor =
          (GtkAccessibleValueNumberCtor) cstate->ctor;

        double value = va_arg (*args, double);

        if (isnan (value) || isinf (value))
          {
            g_set_error_literal (error, GTK_ACCESSIBLE_VALUE_ERROR,
                                 GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                                 "Invalid numeric value");
            return NULL;
          }

        if (ctor == NULL)
          res = gtk_number_accessible_value_new (value);
        else
          res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_STRING:
      {
        GtkAccessibleValueStringCtor ctor =
          (GtkAccessibleValueStringCtor) cstate->ctor;

        const char *value = va_arg (*args, char*);

        if (ctor == NULL)
          {
            if (value != NULL)
              res = gtk_string_accessible_value_new (value);
          }
        else
          {
            res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE:
      {
        GtkAccessibleValueRefCtor ctor =
          (GtkAccessibleValueRefCtor) cstate->ctor;

        gpointer value = va_arg (*args, gpointer);

        if (value != NULL && !GTK_IS_ACCESSIBLE (value))
          {
            g_set_error_literal (error, GTK_ACCESSIBLE_VALUE_ERROR,
                                 GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                                 "Reference does not implement GtkAccessible");
            return NULL;
          }

        if (ctor == NULL)
          {
            if (value != NULL)
              res = gtk_reference_accessible_value_new (value);
            else
              res = gtk_undefined_accessible_value_new ();
          }
        else
          {
            res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST:
      {
        GtkAccessibleValueRefListCtor ctor =
          (GtkAccessibleValueRefListCtor) cstate->ctor;

        GtkAccessible *ref = va_arg (*args, gpointer);
        GList *value = NULL;

        while (ref != NULL)
          {
            if (!GTK_IS_ACCESSIBLE (ref))
              {
                g_set_error (error, GTK_ACCESSIBLE_VALUE_ERROR,
                             GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                             "Reference of type “%s” [%p] does not implement GtkAccessible",
                             G_OBJECT_TYPE_NAME (ref), ref);
                return NULL;
              }

            value = g_list_prepend (value, ref);

            ref = va_arg (*args, gpointer);
          }

        if (value == NULL)
          res = gtk_undefined_accessible_value_new ();
        else
          {
            value = g_list_reverse (value);

            if (ctor == NULL)
              res = gtk_reference_list_accessible_value_new (value);
            else
              res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_UNDEFINED:
    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_assert_not_reached ();
      break;
    }

  return res;
}

static GtkAccessibleValue *
gtk_accessible_value_collect_value (const GtkAccessibleCollect  *cstate,
                                    const GValue                *value_,
                                    GError                     **error)
{
  GtkAccessibleValue *res = NULL;
  GtkAccessibleCollectType ctype = cstate->ctype;
  gboolean collects_undef = (ctype & GTK_ACCESSIBLE_COLLECT_UNDEFINED) != 0;

  ctype &= (GTK_ACCESSIBLE_COLLECT_UNDEFINED - 1);

  /* Tristate values include "undefined" by definition */
  if (ctype == GTK_ACCESSIBLE_COLLECT_TRISTATE)
    collects_undef = TRUE;

  switch (ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        if (collects_undef)
          {
            int value = g_value_get_int (value_);

            if (value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
              res = gtk_undefined_accessible_value_new ();
            else
              res = gtk_boolean_accessible_value_new (value == 0 ? FALSE : TRUE);
          }
        else
          {
            gboolean value = g_value_get_boolean (value_);

            res = gtk_boolean_accessible_value_new (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      {
        int value = g_value_get_int (value_);

        if (collects_undef && value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
          res = gtk_undefined_accessible_value_new ();
        else
          res = gtk_tristate_accessible_value_new (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TOKEN:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value = g_value_get_int (value_);

        if (collects_undef && value == GTK_ACCESSIBLE_VALUE_UNDEFINED)
          {
            res = gtk_undefined_accessible_value_new ();
          }
        else
          {
            /* Token collection requires a constructor */
            g_assert (ctor != NULL);

            res = (* ctor) (value);
          }

        if (res == NULL)
          {
            g_set_error (error, GTK_ACCESSIBLE_VALUE_ERROR,
                         GTK_ACCESSIBLE_VALUE_ERROR_INVALID_TOKEN,
                         "Invalid value for token attribute: %d",
                         value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      {
        GtkAccessibleValueEnumCtor ctor =
          (GtkAccessibleValueEnumCtor) cstate->ctor;

        int value = g_value_get_int (value_);

        if (ctor == NULL)
          res = gtk_int_accessible_value_new (value);
        else
          res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_NUMBER:
      {
        GtkAccessibleValueNumberCtor ctor =
          (GtkAccessibleValueNumberCtor) cstate->ctor;

        double value = g_value_get_double (value_);

        if (isnan (value) || isinf (value))
          {
            g_set_error_literal (error, GTK_ACCESSIBLE_VALUE_ERROR,
                                 GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                                 "Invalid numeric value");
            return NULL;
          }

        if (ctor == NULL)
          res = gtk_number_accessible_value_new (value);
        else
          res = (* ctor) (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_STRING:
      {
        GtkAccessibleValueStringCtor ctor =
          (GtkAccessibleValueStringCtor) cstate->ctor;

        const char *value = g_value_get_string (value_);

        if (ctor == NULL)
          {
            if (value != NULL)
              res = gtk_string_accessible_value_new (value);
          }
        else
          {
            res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE:
      {
        GtkAccessibleValueRefCtor ctor =
          (GtkAccessibleValueRefCtor) cstate->ctor;

        gpointer value = g_value_get_object (value_);

        if (value != NULL && !GTK_IS_ACCESSIBLE (value))
          {
            g_set_error_literal (error, GTK_ACCESSIBLE_VALUE_ERROR,
                                 GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                                 "Reference does not implement GtkAccessible");
            return NULL;
          }

        if (ctor == NULL)
          {
            if (value != NULL)
              res = gtk_reference_accessible_value_new (value);
          }
        else
          {
            res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST:
      {
        GtkAccessibleValueRefListCtor ctor =
          (GtkAccessibleValueRefListCtor) cstate->ctor;

        GList *value;

        if (g_type_is_a (G_VALUE_TYPE(value_), GTK_ACCESSIBLE_LIST))
          {
            GtkAccessibleList *boxed = g_value_get_boxed (value_);

            value = gtk_accessible_list_get_objects (boxed);
          }
        else 
          {
            value = g_value_get_pointer (value_);
          }

        if (ctor == NULL)
          {
            if (value != NULL)
              res = gtk_reference_list_accessible_value_new (value);
          }
        else
          {
            res = (* ctor) (value);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_UNDEFINED:
    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_assert_not_reached ();
      break;
    }

  return res;
}

static GtkAccessibleValue *
gtk_accessible_value_parse (const GtkAccessibleCollect  *cstate,
                            const char                  *str,
                            gsize                        len,
                            GError                     **error)
{
  GtkAccessibleValue *res = NULL;
  GtkAccessibleCollectType ctype = cstate->ctype;
  gboolean collects_undef = (ctype & GTK_ACCESSIBLE_COLLECT_UNDEFINED) != 0;

  ctype &= (GTK_ACCESSIBLE_COLLECT_UNDEFINED - 1);

  /* Tristate values include "undefined" by definition */
  if (ctype == GTK_ACCESSIBLE_COLLECT_TRISTATE)
    collects_undef = TRUE;

  switch (ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        gboolean b;

        if (collects_undef && strncmp (str, "undefined", 9) == 0)
          res = gtk_undefined_accessible_value_new ();
        else if (_gtk_builder_boolean_from_string (str, &b, error))
          res = gtk_boolean_accessible_value_new (b);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      {
        int value;

        if (collects_undef && strncmp (str, "undefined", 9) == 0)
          res = gtk_undefined_accessible_value_new ();
        else if (_gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_TRISTATE, str, &value, error))
          res = gtk_boolean_accessible_value_new (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TOKEN:
      {
        GtkAccessibleValueEnumParser parser =
          (GtkAccessibleValueEnumParser) cstate->parser;

        if (collects_undef && strncmp (str, "undefined", 9) == 0)
          {
            res = gtk_undefined_accessible_value_new ();
          }
        else
          {
            /* Token collection requires a constructor */
            g_assert (parser != NULL);

            res = (* parser) (str, len, error);
          }
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      {
        char *end = NULL;
        gint64 value = g_ascii_strtoll (str, &end, 10);

        if (str == end)
          {
            int saved_errno = errno;
            g_set_error (error, GTK_ACCESSIBLE_VALUE_ERROR,
                         GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                         "Invalid integer value “%s”: %s",
                         str, g_strerror (saved_errno));

            return NULL;
          }
        else
          res = gtk_int_accessible_value_new ((int) value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_NUMBER:
      {
        char *end = NULL;
        double value = g_ascii_strtod (str, &end);

        if (str == end || isnan (value) || isinf (value))
          {
            g_set_error (error, GTK_ACCESSIBLE_VALUE_ERROR,
                         GTK_ACCESSIBLE_VALUE_ERROR_INVALID_VALUE,
                         "Invalid numeric value “%s”",
                         str);
            return NULL;
          }

        res = gtk_number_accessible_value_new (value);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_STRING:
      {
        res = gtk_string_accessible_value_new (str);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE:
    case GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST:
      {
        /* We do not error out, to let the caller code deal
         * with the references themselves
         */
        res = NULL;
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_UNDEFINED:
    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_assert_not_reached ();
      break;
    }

  return res;
}

static void
gtk_accessible_attribute_init_value (const GtkAccessibleCollect *cstate,
                                     GValue                     *value)
{
  GtkAccessibleCollectType ctype = cstate->ctype;
  gboolean collects_undef = (ctype & GTK_ACCESSIBLE_COLLECT_UNDEFINED) != 0;

  ctype &= (GTK_ACCESSIBLE_COLLECT_UNDEFINED - 1);

  /* Tristate values include "undefined" by definition */
  if (ctype == GTK_ACCESSIBLE_COLLECT_TRISTATE)
    collects_undef = TRUE;

  switch (ctype)
    {
    case GTK_ACCESSIBLE_COLLECT_BOOLEAN:
      {
        if (collects_undef)
          g_value_init (value, G_TYPE_INT);
        else
          g_value_init (value, G_TYPE_BOOLEAN);
      }
      break;

    case GTK_ACCESSIBLE_COLLECT_TRISTATE:
      g_value_init (value, GTK_TYPE_ACCESSIBLE_TRISTATE);
      break;

    case GTK_ACCESSIBLE_COLLECT_TOKEN:
      if (cstate->init_value != NULL)
        {
          GtkAccessibleValueEnumInit init_value =
            (GtkAccessibleValueEnumInit) cstate->init_value;

          (* init_value) (value);
        }
      else
        g_value_init (value, G_TYPE_INT);
      break;

    case GTK_ACCESSIBLE_COLLECT_INTEGER:
      g_value_init (value, G_TYPE_INT);
      break;

    case GTK_ACCESSIBLE_COLLECT_NUMBER:
      g_value_init (value, G_TYPE_DOUBLE);
      break;

    case GTK_ACCESSIBLE_COLLECT_STRING:
      g_value_init (value, G_TYPE_STRING);
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE:
      g_value_init (value, GTK_TYPE_ACCESSIBLE);
      break;

    case GTK_ACCESSIBLE_COLLECT_REFERENCE_LIST:
      g_value_init (value, G_TYPE_POINTER);
      break;

    case GTK_ACCESSIBLE_COLLECT_UNDEFINED:
    case GTK_ACCESSIBLE_COLLECT_INVALID:
    default:
      g_assert_not_reached ();
      break;
    }
}

/*< private >
 * gtk_accessible_value_collect_for_state:
 * @state: a `GtkAccessibleState`
 * @error: return location for a `GError`
 * @args: a `va_list` reference
 *
 * Collects and consumes the next item in the @args variadic arguments list,
 * and returns a `GtkAccessibleValue` for it.
 *
 * If the collection fails, @error is set and %NULL is returned.
 *
 * The returned value could be %NULL even on success, in which case the state
 * should be reset to its default value by the caller.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_state (GtkAccessibleState   state,
                                        GError             **error,
                                        va_list             *args)
{
  const GtkAccessibleCollect *cstate = &collect_states[state];

  g_return_val_if_fail (state <= GTK_ACCESSIBLE_STATE_VISITED, NULL);

  return gtk_accessible_value_collect_valist (cstate, error, args);
}

/*< private >
 * gtk_accessible_value_collect_for_state_value:
 * @state: a `GtkAccessibleState`
 * @value: a `GValue`
 * @error: return location for a `GError`
 *
 * Retrieves the value stored inside @value and returns a `GtkAccessibleValue`
 * for the given @state.
 *
 * If the collection fails, @error is set and %NULL is returned.
 *
 * The returned value could be %NULL even on success, in which case the state
 * should be reset to its default value by the caller.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_state_value (GtkAccessibleState   state,
                                              const GValue        *value,
                                              GError             **error)
{
  const GtkAccessibleCollect *cstate = &collect_states[state];

  g_return_val_if_fail (state <= GTK_ACCESSIBLE_STATE_VISITED, NULL);

  return gtk_accessible_value_collect_value (cstate, value, error);
}

GtkAccessibleValue *
gtk_accessible_value_parse_for_state (GtkAccessibleState   state,
                                      const char          *str,
                                      gsize                len,
                                      GError             **error)
{
  const GtkAccessibleCollect *cstate = &collect_states[state];

  g_return_val_if_fail (state <= GTK_ACCESSIBLE_STATE_VISITED, NULL);

  return gtk_accessible_value_parse (cstate, str, len, error);
}

/**
 * gtk_accessible_state_init_value:
 * @state: a `GtkAccessibleState`
 * @value: an uninitialized `GValue`
 *
 * Initializes @value with the appropriate type for the @state.
 *
 * This function is mostly meant for language bindings, in conjunction
 * with gtk_accessible_update_relation_state().
 */
void
gtk_accessible_state_init_value (GtkAccessibleState  state,
                                 GValue             *value)
{
  const GtkAccessibleCollect *cstate = &collect_states[state];

  g_return_if_fail (state <= GTK_ACCESSIBLE_STATE_SELECTED);

  gtk_accessible_attribute_init_value (cstate, value);
}

/*< private >
 * gtk_accessible_value_get_default_for_property:
 * @property: a `GtkAccessibleProperty`
 *
 * Retrieves the `GtkAccessibleValue` that contains the default for the
 * given @property.
 *
 * Returns: (transfer full): the `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_get_default_for_property (GtkAccessibleProperty property)
{
  const GtkAccessibleCollect *cstate = &collect_props[property];

  g_return_val_if_fail (property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  switch (cstate->value)
    {
    /* Boolean properties */
    case GTK_ACCESSIBLE_PROPERTY_HAS_POPUP:
    case GTK_ACCESSIBLE_PROPERTY_MODAL:
    case GTK_ACCESSIBLE_PROPERTY_MULTI_LINE:
    case GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE:
    case GTK_ACCESSIBLE_PROPERTY_READ_ONLY:
    case GTK_ACCESSIBLE_PROPERTY_REQUIRED:
      return gtk_boolean_accessible_value_new (FALSE);

    /* Integer properties */
    case GTK_ACCESSIBLE_PROPERTY_LEVEL:
      return gtk_int_accessible_value_new (0);

    /* Number properties */
    case GTK_ACCESSIBLE_PROPERTY_VALUE_MAX:
    case GTK_ACCESSIBLE_PROPERTY_VALUE_MIN:
    case GTK_ACCESSIBLE_PROPERTY_VALUE_NOW:
      return gtk_number_accessible_value_new (0);

    /* String properties */
    case GTK_ACCESSIBLE_PROPERTY_DESCRIPTION:
    case GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS:
    case GTK_ACCESSIBLE_PROPERTY_LABEL:
    case GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER:
    case GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION:
    case GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT:
      return gtk_undefined_accessible_value_new ();

    /* Token properties */
    case GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE:
      return gtk_autocomplete_accessible_value_new (GTK_ACCESSIBLE_AUTOCOMPLETE_NONE);

    case GTK_ACCESSIBLE_PROPERTY_ORIENTATION:
      return gtk_undefined_accessible_value_new ();

    case GTK_ACCESSIBLE_PROPERTY_SORT:
      return gtk_sort_accessible_value_new (GTK_ACCESSIBLE_SORT_NONE);

    default:
      g_critical ("Unknown value for accessible property “%s”", cstate->name);
      break;
    }

  return NULL;
}

/*< private >
 * gtk_accessible_value_collect_for_property:
 * @property: a `GtkAccessibleProperty`
 * @error: return location for a `GError`
 * @args: a `va_list` reference
 *
 * Collects and consumes the next item in the @args variadic arguments list,
 * and returns a `GtkAccessibleValue` for it.
 *
 * If the collection fails, @error is set.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_property (GtkAccessibleProperty   property,
                                           GError                **error,
                                           va_list                *args)
{
  const GtkAccessibleCollect *cstate = &collect_props[property];

  g_return_val_if_fail (property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  return gtk_accessible_value_collect_valist (cstate, error, args);
}

/*< private >
 * gtk_accessible_value_collect_for_property_value:
 * @property: a `GtkAccessibleProperty`
 * @value: a `GValue`
 * @error: return location for a `GError`
 *
 * Retrieves the value stored inside @value and returns a `GtkAccessibleValue`
 * for the given @property.
 *
 * If the collection fails, @error is set.
 *
 * The returned value could be %NULL even on success, in which case the property
 * should be reset to its default value by the caller.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_property_value (GtkAccessibleProperty   property,
                                                 const GValue           *value,
                                                 GError                **error)
{
  const GtkAccessibleCollect *cstate = &collect_props[property];

  g_return_val_if_fail (property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  return gtk_accessible_value_collect_value (cstate, value, error);
}

GtkAccessibleValue *
gtk_accessible_value_parse_for_property (GtkAccessibleProperty   property,
                                         const char             *str,
                                         gsize                   len,
                                         GError                **error)
{
  const GtkAccessibleCollect *cstate = &collect_props[property];

  g_return_val_if_fail (property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  return gtk_accessible_value_parse (cstate, str, len, error);
}

/**
 * gtk_accessible_property_init_value:
 * @property: a `GtkAccessibleProperty`
 * @value: an uninitialized `GValue`
 *
 * Initializes @value with the appropriate type for the @property.
 *
 * This function is mostly meant for language bindings, in conjunction
 * with gtk_accessible_update_property_value().
 */
void
gtk_accessible_property_init_value (GtkAccessibleProperty  property,
                                    GValue                *value)
{
  const GtkAccessibleCollect *cstate = &collect_props[property];

  g_return_if_fail (property <= GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT);

  gtk_accessible_attribute_init_value (cstate, value);
}

/*< private >
 * gtk_accessible_value_get_default_for_relation:
 * @relation: a `GtkAccessibleRelation`
 *
 * Retrieves the `GtkAccessibleValue` that contains the default for the
 * given @relation.
 *
 * Returns: (transfer full): the `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_get_default_for_relation (GtkAccessibleRelation relation)
{
  const GtkAccessibleCollect *cstate = &collect_rels[relation];

  g_return_val_if_fail (relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE, NULL);

  switch (cstate->value)
    {
    /* References */
    case GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT:
    case GTK_ACCESSIBLE_RELATION_CONTROLS:
    case GTK_ACCESSIBLE_RELATION_DESCRIBED_BY:
    case GTK_ACCESSIBLE_RELATION_DETAILS:
    case GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE:
    case GTK_ACCESSIBLE_RELATION_FLOW_TO:
    case GTK_ACCESSIBLE_RELATION_LABELLED_BY:
    case GTK_ACCESSIBLE_RELATION_OWNS:
      return gtk_undefined_accessible_value_new ();

    /* Integers */
    case GTK_ACCESSIBLE_RELATION_COL_COUNT:
    case GTK_ACCESSIBLE_RELATION_COL_INDEX:
    case GTK_ACCESSIBLE_RELATION_COL_SPAN:
    case GTK_ACCESSIBLE_RELATION_POS_IN_SET:
    case GTK_ACCESSIBLE_RELATION_ROW_COUNT:
    case GTK_ACCESSIBLE_RELATION_ROW_INDEX:
    case GTK_ACCESSIBLE_RELATION_ROW_SPAN:
    case GTK_ACCESSIBLE_RELATION_SET_SIZE:
      return gtk_int_accessible_value_new (0);

    /* Strings */
    case GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT:
    case GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT:
      return gtk_undefined_accessible_value_new ();

    default:
      g_critical ("Unknown value for accessible property “%s”", cstate->name);
      break;
    }

  return NULL;
}

/*< private >
 * gtk_accessible_value_collect_for_relation:
 * @relation: a `GtkAccessibleRelation`
 * @error: return location for a `GError`
 * @args: a `va_list` reference
 *
 * Collects and consumes the next item in the @args variadic arguments list,
 * and returns a `GtkAccessibleValue` for it.
 *
 * If the collection fails, @error is set and %NULL is returned.
 *
 * The returned value could be %NULL even on success, in which case the relation
 * should be reset to its default value by the caller.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_relation (GtkAccessibleRelation   relation,
                                           GError                **error,
                                           va_list                *args)
{
  const GtkAccessibleCollect *cstate = &collect_rels[relation];

  g_return_val_if_fail (relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE, NULL);

  return gtk_accessible_value_collect_valist (cstate, error, args);
}

/*< private >
 * gtk_accessible_value_collect_for_relation_value:
 * @relation: a `GtkAccessibleRelation`
 * @value: a `GValue`
 * @error: return location for a `GError`
 *
 * Retrieves the value stored inside @value and returns a `GtkAccessibleValue`
 * for the given @relation.
 *
 * If the collection fails, @error is set and %NULL is returned.
 *
 * The returned value could be %NULL even on success, in which case the relation
 * should be reset to its default value by the caller.
 *
 * Returns: (transfer full) (nullable): a `GtkAccessibleValue`
 */
GtkAccessibleValue *
gtk_accessible_value_collect_for_relation_value (GtkAccessibleRelation   relation,
                                                 const GValue           *value,
                                                 GError                **error)
{
  const GtkAccessibleCollect *cstate = &collect_rels[relation];

  g_return_val_if_fail (relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE, NULL);
 
  return gtk_accessible_value_collect_value (cstate, value, error);
}

GtkAccessibleValue *
gtk_accessible_value_parse_for_relation (GtkAccessibleRelation   relation,
                                         const char             *str,
                                         gsize                   len,
                                         GError                **error)
{
  const GtkAccessibleCollect *cstate = &collect_rels[relation];

  g_return_val_if_fail (relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE, NULL);

  return gtk_accessible_value_parse (cstate, str, len, error);
}

/**
 * gtk_accessible_relation_init_value:
 * @relation: a `GtkAccessibleRelation`
 * @value: an uninitialized `GValue`
 *
 * Initializes @value with the appropriate type for the @relation.
 *
 * This function is mostly meant for language bindings, in conjunction
 * with gtk_accessible_update_relation_value().
 */
void
gtk_accessible_relation_init_value (GtkAccessibleRelation  relation,
                                    GValue                *value)
{
  const GtkAccessibleCollect *cstate = &collect_rels[relation];

  g_return_if_fail (relation <= GTK_ACCESSIBLE_RELATION_SET_SIZE);

  gtk_accessible_attribute_init_value (cstate, value);
}

/* }}} */
