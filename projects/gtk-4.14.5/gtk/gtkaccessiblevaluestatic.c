/* gtkaccessiblevaluestatic.c: GtkAccessibleValue static implementations
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

#include "config.h"

#include "gtkaccessiblevalueprivate.h"
#include "gtkbuilderprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"

/* {{{ Undefined value */

static void
gtk_undefined_accessible_value_print (const GtkAccessibleValue *value,
                                      GString                  *buffer)
{
  g_string_append (buffer, "undefined");
}

static const GtkAccessibleValueClass GTK_UNDEFINED_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_UNDEFINED,
  .type_name = "GtkUndefinedAccessibleValue",
  .instance_size = sizeof (GtkAccessibleValue),
  .print = gtk_undefined_accessible_value_print,
};

static GtkAccessibleValue undefined_value =
  GTK_ACCESSIBLE_VALUE_INIT (&GTK_UNDEFINED_ACCESSIBLE_VALUE);

GtkAccessibleValue *
gtk_undefined_accessible_value_new (void)
{
  return gtk_accessible_value_ref (&undefined_value);
}

int
gtk_undefined_accessible_value_get (const GtkAccessibleValue *value)
{
  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_VALUE_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_UNDEFINED_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_VALUE_UNDEFINED);

  return GTK_ACCESSIBLE_VALUE_UNDEFINED;
}

/* }}} */

/* {{{ Boolean values */ 

typedef struct
{
  GtkAccessibleValue parent;

  gboolean value;
} GtkBooleanAccessibleValue;

static gboolean 
gtk_boolean_accessible_value_equal (const GtkAccessibleValue *value_a,
                                    const GtkAccessibleValue *value_b)
{
  const GtkBooleanAccessibleValue *bool_a = (GtkBooleanAccessibleValue *) value_a;
  const GtkBooleanAccessibleValue *bool_b = (GtkBooleanAccessibleValue *) value_b;

  return bool_a->value == bool_b->value;
}

static void
gtk_boolean_accessible_value_print (const GtkAccessibleValue *value,
                                    GString                  *buffer)
{
  const GtkBooleanAccessibleValue *self = (GtkBooleanAccessibleValue *) value;

  g_string_append_printf (buffer, "%s", self->value ? "true" : "false");
}

static const GtkAccessibleValueClass GTK_BOOLEAN_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN,
  .type_name = "GtkBooleanAccessibleValue",
  .instance_size = sizeof (GtkBooleanAccessibleValue),
  .equal = gtk_boolean_accessible_value_equal,
  .print = gtk_boolean_accessible_value_print,
};

static GtkBooleanAccessibleValue boolean_values[] = {
  { GTK_ACCESSIBLE_VALUE_INIT (&GTK_BOOLEAN_ACCESSIBLE_VALUE), FALSE },
  { GTK_ACCESSIBLE_VALUE_INIT (&GTK_BOOLEAN_ACCESSIBLE_VALUE), TRUE },
};

GtkAccessibleValue *
gtk_boolean_accessible_value_new (gboolean state)
{
  if (state)
    return gtk_accessible_value_ref ((GtkAccessibleValue *) &boolean_values[1]);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &boolean_values[0]);
}

gboolean
gtk_boolean_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkBooleanAccessibleValue *self = (GtkBooleanAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (value->value_class == &GTK_BOOLEAN_ACCESSIBLE_VALUE, FALSE);

  return self->value;
}

/* }}} */

/* {{{ Tri-state values */

typedef struct {
  GtkAccessibleValue parent;

  GtkAccessibleTristate value;
} GtkTristateAccessibleValue;

static gboolean
gtk_tristate_accessible_value_equal (const GtkAccessibleValue *value_a,
                                     const GtkAccessibleValue *value_b)
{
  const GtkTristateAccessibleValue *self_a = (GtkTristateAccessibleValue *) value_a;
  const GtkTristateAccessibleValue *self_b = (GtkTristateAccessibleValue *) value_b;

  return self_a->value == self_b->value;
}

static void
gtk_tristate_accessible_value_print (const GtkAccessibleValue *value,
                                     GString                  *buffer)
{
  const GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  switch (self->value)
    {
    case GTK_ACCESSIBLE_TRISTATE_FALSE:
      g_string_append (buffer, "false");
      break;

    case GTK_ACCESSIBLE_TRISTATE_TRUE:
      g_string_append (buffer, "true");
      break;

    case GTK_ACCESSIBLE_TRISTATE_MIXED:
      g_string_append (buffer, "mixed");
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static const GtkAccessibleValueClass GTK_TRISTATE_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_TRISTATE,
  .type_name = "GtkTristateAccessibleValue",
  .instance_size = sizeof (GtkTristateAccessibleValue),
  .equal = gtk_tristate_accessible_value_equal,
  .print = gtk_tristate_accessible_value_print,
};

static GtkTristateAccessibleValue tristate_values[] = {
  [GTK_ACCESSIBLE_TRISTATE_FALSE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_TRISTATE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_TRISTATE_FALSE
  },
  [GTK_ACCESSIBLE_TRISTATE_TRUE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_TRISTATE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_TRISTATE_TRUE
  },
  [GTK_ACCESSIBLE_TRISTATE_MIXED] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_TRISTATE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_TRISTATE_MIXED
  },
};

GtkAccessibleValue *
gtk_tristate_accessible_value_new (GtkAccessibleTristate value)
{
  g_return_val_if_fail (value >= GTK_ACCESSIBLE_TRISTATE_FALSE &&
                        value <= GTK_ACCESSIBLE_TRISTATE_MIXED, NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &tristate_values[value]);
}

GtkAccessibleTristate
gtk_tristate_accessible_value_get (const GtkAccessibleValue *value)
{
  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_TRISTATE_FALSE);
  g_return_val_if_fail (value->value_class == &GTK_TRISTATE_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_TRISTATE_FALSE);

  GtkTristateAccessibleValue *self = (GtkTristateAccessibleValue *) value;

  return self->value;
}

/* }}} */

/* {{{ Enumeration values */

typedef struct {
  GtkAccessibleValue parent;

  int value;
  const char *token;
} GtkTokenAccessibleValue;

static gboolean
gtk_token_accessible_value_equal (const GtkAccessibleValue *value_a,
                                  const GtkAccessibleValue *value_b)
{
  const GtkTokenAccessibleValue *self_a = (GtkTokenAccessibleValue *) value_a;
  const GtkTokenAccessibleValue *self_b = (GtkTokenAccessibleValue *) value_b;

  return self_a->value == self_b->value;
}

static void
gtk_token_accessible_value_print (const GtkAccessibleValue *value,
                                  GString                  *buffer)
{
  const GtkTokenAccessibleValue *self = (GtkTokenAccessibleValue *) value;

  g_string_append (buffer, self->token);
}

static const GtkAccessibleValueClass GTK_INVALID_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_TOKEN,
  .type_name = "GtkInvalidAccessibleValue",
  .instance_size = sizeof (GtkTokenAccessibleValue),
  .equal = gtk_token_accessible_value_equal,
  .print = gtk_token_accessible_value_print,
};

static GtkTokenAccessibleValue invalid_values[] = {
  [GTK_ACCESSIBLE_INVALID_FALSE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_INVALID_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_INVALID_FALSE, "false"
  },
  [GTK_ACCESSIBLE_INVALID_TRUE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_INVALID_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_INVALID_TRUE, "true"
  },
  [GTK_ACCESSIBLE_INVALID_GRAMMAR] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_INVALID_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_INVALID_GRAMMAR, "grammar"
  },
  [GTK_ACCESSIBLE_INVALID_SPELLING] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_INVALID_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_INVALID_SPELLING, "spelling"
  },
};

GtkAccessibleValue *
gtk_invalid_accessible_value_new (GtkAccessibleInvalidState state)
{
  g_return_val_if_fail (state >= GTK_ACCESSIBLE_INVALID_FALSE &&
                        state <= GTK_ACCESSIBLE_INVALID_SPELLING,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &invalid_values[state]);
}

GtkAccessibleInvalidState
gtk_invalid_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTokenAccessibleValue *self = (GtkTokenAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_INVALID_FALSE);
  g_return_val_if_fail (value->value_class == &GTK_INVALID_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_INVALID_FALSE);

  return self->value;
}

GtkAccessibleValue *
gtk_invalid_accessible_value_parse (const char  *str,
                                    gsize        len,
                                    GError     **error)
{
  int value;

  if (_gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_INVALID_STATE, str, &value, error))
    return gtk_invalid_accessible_value_new (value);

  return NULL;
}

void
gtk_invalid_accessible_value_init_value (GValue *value)
{
  g_value_init (value, GTK_TYPE_ACCESSIBLE_INVALID_STATE);
}

static const GtkAccessibleValueClass GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_TOKEN,
  .type_name = "GtkAutocompleteAccessibleValue",
  .instance_size = sizeof (GtkTokenAccessibleValue),
  .equal = gtk_token_accessible_value_equal,
  .print = gtk_token_accessible_value_print,
};

static GtkTokenAccessibleValue autocomplete_values[] = {
  [GTK_ACCESSIBLE_AUTOCOMPLETE_NONE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_AUTOCOMPLETE_NONE, "none"
  },
  [GTK_ACCESSIBLE_AUTOCOMPLETE_INLINE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_AUTOCOMPLETE_INLINE, "inline"
  },
  [GTK_ACCESSIBLE_AUTOCOMPLETE_LIST] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_AUTOCOMPLETE_LIST, "list"
  },
  [GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH, "both"
  },
};

GtkAccessibleValue *
gtk_autocomplete_accessible_value_new (GtkAccessibleAutocomplete value)
{
  g_return_val_if_fail (value >= GTK_ACCESSIBLE_AUTOCOMPLETE_NONE &&
                        value <= GTK_ACCESSIBLE_AUTOCOMPLETE_BOTH,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &autocomplete_values[value]);
}

GtkAccessibleAutocomplete
gtk_autocomplete_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTokenAccessibleValue *self = (GtkTokenAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_AUTOCOMPLETE_NONE);
  g_return_val_if_fail (value->value_class == &GTK_AUTOCOMPLETE_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_AUTOCOMPLETE_NONE);

  return self->value;
}

GtkAccessibleValue *
gtk_autocomplete_accessible_value_parse (const char  *str,
                                         gsize        len,
                                         GError     **error)
{
  int value;

  if (_gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_AUTOCOMPLETE, str, &value, error))
    return gtk_autocomplete_accessible_value_new (value);

  return NULL;
}

void
gtk_autocomplete_accessible_value_init_value (GValue *value)
{
  g_value_init (value, GTK_TYPE_ACCESSIBLE_AUTOCOMPLETE);
}

static const GtkAccessibleValueClass GTK_ORIENTATION_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_TOKEN,
  .type_name = "GtkOrientationAccessibleValue",
  .instance_size = sizeof (GtkTokenAccessibleValue),
  .equal = gtk_token_accessible_value_equal,
  .print = gtk_token_accessible_value_print,
};

static GtkTokenAccessibleValue orientation_values[] = {
  [GTK_ORIENTATION_HORIZONTAL] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_ORIENTATION_ACCESSIBLE_VALUE), GTK_ORIENTATION_HORIZONTAL, "horizontal"
  },
  [GTK_ORIENTATION_VERTICAL] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_ORIENTATION_ACCESSIBLE_VALUE), GTK_ORIENTATION_VERTICAL, "vertical"
  },
};

GtkAccessibleValue *
gtk_orientation_accessible_value_new (GtkOrientation value)
{
  g_return_val_if_fail (value >= GTK_ORIENTATION_HORIZONTAL &&
                        value <= GTK_ORIENTATION_VERTICAL,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &orientation_values[value]);
}

GtkOrientation
gtk_orientation_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTokenAccessibleValue *self = (GtkTokenAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_VALUE_UNDEFINED);
  g_return_val_if_fail (value->value_class == &GTK_ORIENTATION_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_VALUE_UNDEFINED);

  return self->value;
}

GtkAccessibleValue *
gtk_orientation_accessible_value_parse (const char  *str,
                                        gsize        len,
                                        GError     **error)
{
  int value;

  if (_gtk_builder_enum_from_string (GTK_TYPE_ORIENTATION, str, &value, error))
    return gtk_orientation_accessible_value_new (value);

  return NULL;
}

void
gtk_orientation_accessible_value_init_value (GValue *value)
{
  g_value_init (value, GTK_TYPE_ORIENTATION);
}

static const GtkAccessibleValueClass GTK_SORT_ACCESSIBLE_VALUE = {
  .type = GTK_ACCESSIBLE_VALUE_TYPE_TOKEN,
  .type_name = "GtkSortAccessibleValue",
  .instance_size = sizeof (GtkTokenAccessibleValue),
  .equal = gtk_token_accessible_value_equal,
  .print = gtk_token_accessible_value_print,
};

static GtkTokenAccessibleValue sort_values[] = {
  [GTK_ACCESSIBLE_SORT_NONE] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_SORT_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_SORT_NONE, "none"
  },
  [GTK_ACCESSIBLE_SORT_ASCENDING] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_SORT_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_SORT_ASCENDING, "ascending"
  },
  [GTK_ACCESSIBLE_SORT_DESCENDING] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_SORT_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_SORT_DESCENDING, "descending"
  },
  [GTK_ACCESSIBLE_SORT_OTHER] = {
    GTK_ACCESSIBLE_VALUE_INIT (&GTK_SORT_ACCESSIBLE_VALUE), GTK_ACCESSIBLE_SORT_OTHER, "other"
  },
};

GtkAccessibleValue *
gtk_sort_accessible_value_new (GtkAccessibleSort value)
{
  g_return_val_if_fail (value >= GTK_ACCESSIBLE_SORT_NONE &&
                        value <= GTK_ACCESSIBLE_SORT_OTHER,
                        NULL);

  return gtk_accessible_value_ref ((GtkAccessibleValue *) &sort_values[value]);
}

GtkAccessibleSort
gtk_sort_accessible_value_get (const GtkAccessibleValue *value)
{
  GtkTokenAccessibleValue *self = (GtkTokenAccessibleValue *) value;

  g_return_val_if_fail (value != NULL, GTK_ACCESSIBLE_SORT_NONE);
  g_return_val_if_fail (value->value_class == &GTK_SORT_ACCESSIBLE_VALUE,
                        GTK_ACCESSIBLE_SORT_NONE);

  return self->value;
}

GtkAccessibleValue *
gtk_sort_accessible_value_parse (const char  *str,
                                 gsize        len,
                                 GError     **error)
{
  int value;

  if (_gtk_builder_enum_from_string (GTK_TYPE_ACCESSIBLE_SORT, str, &value, error))
    return gtk_sort_accessible_value_new (value);

  return NULL;
}

void
gtk_sort_accessible_value_init_value (GValue *value)
{
  g_value_init (value, GTK_TYPE_ACCESSIBLE_SORT);
}

/* }}} */
