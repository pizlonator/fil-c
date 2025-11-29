/*
 * Copyright © 2019 Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtknumericsorter.h"

#include "gtksorterprivate.h"
#include "gtktypebuiltins.h"

#include <math.h>

/**
 * GtkNumericSorter:
 *
 * `GtkNumericSorter` is a `GtkSorter` that compares numbers.
 *
 * To obtain the numbers to compare, this sorter evaluates a
 * [class@Gtk.Expression].
 */

struct _GtkNumericSorter
{
  GtkSorter parent_instance;

  GtkSortType sort_order;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_SORT_ORDER,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkNumericSorter, gtk_numeric_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

#define DO_COMPARISON(_result, _type, _getter, _order) G_STMT_START{ \
  _type num1 = _getter (&value1); \
  _type num2 = _getter (&value2); \
\
  if (num1 < num2) \
    _result = _order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER; \
  else if (num1 > num2) \
    _result = _order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER; \
  else \
    _result = GTK_ORDERING_EQUAL; \
}G_STMT_END

typedef struct _GtkNumericSortKeys GtkNumericSortKeys;
struct _GtkNumericSortKeys
{
  GtkSortKeys keys;

  GtkExpression *expression;
};

static void
gtk_numeric_sort_keys_free (GtkSortKeys *keys)
{
  GtkNumericSortKeys *self = (GtkNumericSortKeys *) keys;

  gtk_expression_unref (self->expression);
  g_free (self);
}

#define COMPARE_FUNC(type, name, _a, _b) \
static int \
gtk_ ## type ## _sort_keys_compare_ ## name (gconstpointer a, \
                                             gconstpointer b, \
                                             gpointer      unused) \
{ \
  type num1 = *(type *) _a; \
  type num2 = *(type *) _b; \
\
  if (num1 < num2) \
    return GTK_ORDERING_SMALLER; \
  else if (num1 > num2) \
    return GTK_ORDERING_LARGER; \
  else \
    return GTK_ORDERING_EQUAL; \
}
#define COMPARE_FUNCS(type) \
  COMPARE_FUNC(type, ascending, a, b) \
  COMPARE_FUNC(type, descending, b, a)

#define FLOAT_COMPARE_FUNC(type, name, _a, _b) \
static int \
gtk_ ## type ## _sort_keys_compare_ ## name (gconstpointer a, \
                                             gconstpointer b, \
                                             gpointer      unused) \
{ \
  type num1 = *(type *) _a; \
  type num2 = *(type *) _b; \
\
  if (isnan (num1) && isnan (num2)) \
    return GTK_ORDERING_EQUAL; \
  else if (isnan (num1)) \
    return GTK_ORDERING_LARGER; \
  else if (isnan (num2)) \
    return GTK_ORDERING_SMALLER; \
  else if (num1 < num2) \
    return GTK_ORDERING_SMALLER; \
  else if (num1 > num2) \
    return GTK_ORDERING_LARGER; \
  else \
    return GTK_ORDERING_EQUAL; \
}
#define FLOAT_COMPARE_FUNCS(type) \
  FLOAT_COMPARE_FUNC(type, ascending, a, b) \
  FLOAT_COMPARE_FUNC(type, descending, b, a)

COMPARE_FUNCS(char)
COMPARE_FUNCS(guchar)
COMPARE_FUNCS(int)
COMPARE_FUNCS(guint)
FLOAT_COMPARE_FUNCS(float)
FLOAT_COMPARE_FUNCS(double)
COMPARE_FUNCS(long)
COMPARE_FUNCS(gulong)
COMPARE_FUNCS(gint64)
COMPARE_FUNCS(guint64)

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define NUMERIC_SORT_KEYS(TYPE, key_type, type, default_value) \
static void \
gtk_ ## type ## _sort_keys_init_key (GtkSortKeys *keys, \
                                     gpointer     item, \
                                     gpointer     key_memory) \
{ \
  GtkNumericSortKeys *self = (GtkNumericSortKeys *) keys; \
  key_type *key = (key_type *) key_memory; \
  GValue value = G_VALUE_INIT; \
\
  if (gtk_expression_evaluate (self->expression, item, &value)) \
    *key = g_value_get_ ## type (&value); \
  else \
    *key = default_value; \
\
  g_value_unset (&value); \
} \
\
static gboolean \
gtk_ ## type ## _sort_keys_is_compatible (GtkSortKeys *keys, \
                                          GtkSortKeys *other); \
\
static const GtkSortKeysClass GTK_ASCENDING_ ## TYPE ## _SORT_KEYS_CLASS = \
{ \
  gtk_numeric_sort_keys_free, \
  gtk_ ## key_type ## _sort_keys_compare_ascending, \
  gtk_ ## type ## _sort_keys_is_compatible, \
  gtk_ ## type ## _sort_keys_init_key, \
  NULL \
}; \
\
static const GtkSortKeysClass GTK_DESCENDING_ ## TYPE ## _SORT_KEYS_CLASS = \
{ \
  gtk_numeric_sort_keys_free, \
  gtk_ ## key_type ## _sort_keys_compare_descending, \
  gtk_ ## type ## _sort_keys_is_compatible, \
  gtk_ ## type ## _sort_keys_init_key, \
  NULL \
}; \
\
static gboolean \
gtk_ ## type ## _sort_keys_is_compatible (GtkSortKeys *keys, \
                                          GtkSortKeys *other) \
{ \
  GtkNumericSorter *self = (GtkNumericSorter *) keys; \
  GtkNumericSorter *compare = (GtkNumericSorter *) other; \
\
  if (other->klass != &GTK_ASCENDING_ ## TYPE ## _SORT_KEYS_CLASS && \
      other->klass != &GTK_DESCENDING_ ## TYPE ## _SORT_KEYS_CLASS) \
    return FALSE; \
\
  return self->expression == compare->expression; \
}

NUMERIC_SORT_KEYS(BOOLEAN, char, boolean, FALSE)
NUMERIC_SORT_KEYS(CHAR, char, char, G_MININT8)
NUMERIC_SORT_KEYS(UCHAR, guchar, uchar, G_MAXUINT8)
NUMERIC_SORT_KEYS(INT, int, int, G_MININT)
NUMERIC_SORT_KEYS(UINT, guint, uint, G_MAXUINT)
NUMERIC_SORT_KEYS(FLOAT, float, float, NAN)
NUMERIC_SORT_KEYS(DOUBLE, double, double, NAN)
NUMERIC_SORT_KEYS(LONG, long, long, G_MINLONG)
NUMERIC_SORT_KEYS(ULONG, gulong, ulong, G_MAXLONG)
NUMERIC_SORT_KEYS(INT64, gint64, int64, G_MININT64)
NUMERIC_SORT_KEYS(UINT64, guint64, uint64, G_MAXUINT64)

G_GNUC_END_IGNORE_DEPRECATIONS

static GtkSortKeys *
gtk_numeric_sort_keys_new (GtkNumericSorter *self)
{
  GtkNumericSortKeys *result;

  if (self->expression == NULL)
    return gtk_sort_keys_new_equal ();

  switch (gtk_expression_get_value_type (self->expression))
    {
    case G_TYPE_BOOLEAN:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_BOOLEAN_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_BOOLEAN_SORT_KEYS_CLASS,
                                  sizeof (char),
                                  G_ALIGNOF (char));
      break;

    case G_TYPE_CHAR:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_CHAR_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_CHAR_SORT_KEYS_CLASS,
                                  sizeof (char),
                                  G_ALIGNOF (char));
      break;

    case G_TYPE_UCHAR:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_UCHAR_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_UCHAR_SORT_KEYS_CLASS,
                                  sizeof (guchar),
                                  G_ALIGNOF (guchar));
      break;

    case G_TYPE_INT:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_INT_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_INT_SORT_KEYS_CLASS,
                                  sizeof (int),
                                  G_ALIGNOF (int));
      break;

    case G_TYPE_UINT:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_UINT_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_UINT_SORT_KEYS_CLASS,
                                  sizeof (guint),
                                  G_ALIGNOF (guint));
      break;

    case G_TYPE_FLOAT:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_FLOAT_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_FLOAT_SORT_KEYS_CLASS,
                                  sizeof (float),
                                  G_ALIGNOF (float));
      break;

    case G_TYPE_DOUBLE:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_DOUBLE_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_DOUBLE_SORT_KEYS_CLASS,
                                  sizeof (double),
                                  G_ALIGNOF (double));
      break;

    case G_TYPE_LONG:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_LONG_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_LONG_SORT_KEYS_CLASS,
                                  sizeof (long),
                                  G_ALIGNOF (long));
      break;

    case G_TYPE_ULONG:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_ULONG_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_ULONG_SORT_KEYS_CLASS,
                                  sizeof (gulong),
                                  G_ALIGNOF (gulong));
      break;

    case G_TYPE_INT64:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_INT64_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_INT64_SORT_KEYS_CLASS,
                                  sizeof (gint64),
                                  G_ALIGNOF (gint64));
      break;

    case G_TYPE_UINT64:
      result = gtk_sort_keys_new (GtkNumericSortKeys,
                                  self->sort_order == GTK_SORT_ASCENDING
                                                      ? &GTK_ASCENDING_UINT64_SORT_KEYS_CLASS
                                                      : &GTK_DESCENDING_UINT64_SORT_KEYS_CLASS,
                                  sizeof (guint64),
                                  G_ALIGNOF (guint64));
      break;

    default:
      g_critical ("Invalid value type %s for expression\n", g_type_name (gtk_expression_get_value_type (self->expression)));
      return gtk_sort_keys_new_equal ();
    }

  result->expression = gtk_expression_ref (self->expression);

  return (GtkSortKeys *) result;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkOrdering
gtk_numeric_sorter_compare (GtkSorter *sorter,
                            gpointer   item1,
                            gpointer   item2)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (sorter);
  GValue value1 = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  gboolean res1, res2;
  GtkOrdering result;

  if (self->expression == NULL)
    return GTK_ORDERING_EQUAL;

  res1 = gtk_expression_evaluate (self->expression, item1, &value1);
  res2 = gtk_expression_evaluate (self->expression, item2, &value2);

  /* If items don't evaluate, order them at the end, so they aren't
   * in the way. */
  if (!res1)
    {
      result = res2 ? GTK_ORDERING_LARGER : GTK_ORDERING_EQUAL;
      goto out;
    }
  else if (!res2)
    {
      result = GTK_ORDERING_SMALLER;
      goto out;
    }

  switch (g_type_fundamental (G_VALUE_TYPE (&value1)))
    {
    case G_TYPE_BOOLEAN:
      DO_COMPARISON (result, gboolean, g_value_get_boolean, self->sort_order);
      break;

    case G_TYPE_CHAR:
      DO_COMPARISON (result, char, g_value_get_char, self->sort_order);
      break;

    case G_TYPE_UCHAR:
      DO_COMPARISON (result, guchar, g_value_get_uchar, self->sort_order);
      break;

    case G_TYPE_INT:
      DO_COMPARISON (result, int, g_value_get_int, self->sort_order);
      break;

    case G_TYPE_UINT:
      DO_COMPARISON (result, guint, g_value_get_uint, self->sort_order);
      break;

    case G_TYPE_FLOAT:
      {
        float num1 = g_value_get_float (&value1);
        float num2 = g_value_get_float (&value2);

        if (isnan (num1) && isnan (num2))
          result = GTK_ORDERING_EQUAL;
        else if (isnan (num1))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else if (isnan (num2))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 < num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 > num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else
          result = GTK_ORDERING_EQUAL;
        break;
      }

    case G_TYPE_DOUBLE:
      {
        double num1 = g_value_get_double (&value1);
        double num2 = g_value_get_double (&value2);

        if (isnan (num1) && isnan (num2))
          result = GTK_ORDERING_EQUAL;
        else if (isnan (num1))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else if (isnan (num2))
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 < num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_SMALLER : GTK_ORDERING_LARGER;
        else if (num1 > num2)
          result = self->sort_order == GTK_SORT_ASCENDING ? GTK_ORDERING_LARGER : GTK_ORDERING_SMALLER;
        else
          result = GTK_ORDERING_EQUAL;
        break;
      }

    case G_TYPE_LONG:
      DO_COMPARISON (result, long, g_value_get_long, self->sort_order);
      break;

    case G_TYPE_ULONG:
      DO_COMPARISON (result, gulong, g_value_get_ulong, self->sort_order);
      break;

    case G_TYPE_INT64:
      DO_COMPARISON (result, gint64, g_value_get_int64, self->sort_order);
      break;

    case G_TYPE_UINT64:
      DO_COMPARISON (result, guint64, g_value_get_uint64, self->sort_order);
      break;

    default:
      g_critical ("Invalid value type %s for expression\n", g_type_name (gtk_expression_get_value_type (self->expression)));
      result = GTK_ORDERING_EQUAL;
      break;
    }

out:
  g_value_unset (&value1);
  g_value_unset (&value2);

  return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS

static GtkSorterOrder
gtk_numeric_sorter_get_order (GtkSorter *sorter)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (sorter);

  if (self->expression == NULL)
    return GTK_SORTER_ORDER_NONE;

  return GTK_SORTER_ORDER_PARTIAL;
}

static void
gtk_numeric_sorter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_numeric_sorter_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_SORT_ORDER:
      gtk_numeric_sorter_set_sort_order (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_numeric_sorter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value, self->sort_order);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_numeric_sorter_dispose (GObject *object)
{
  GtkNumericSorter *self = GTK_NUMERIC_SORTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_numeric_sorter_parent_class)->dispose (object);
}

static void
gtk_numeric_sorter_class_init (GtkNumericSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_numeric_sorter_compare;
  sorter_class->get_order = gtk_numeric_sorter_get_order;

  object_class->get_property = gtk_numeric_sorter_get_property;
  object_class->set_property = gtk_numeric_sorter_set_property;
  object_class->dispose = gtk_numeric_sorter_dispose;

  /**
   * GtkNumericSorter:expression: (type GtkExpression) (attributes org.gtk.Property.get=gtk_numeric_sorter_get_expression org.gtk.Property.set=gtk_numeric_sorter_set_expression)
   *
   * The expression to evaluate on items to get a number to compare with.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression", NULL, NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkNumericSorter:sort-order: (attributes org.gtk.Property.get=gtk_numeric_sorter_get_sort_order org.gtk.Property.set=gtk_numeric_sorter_set_sort_order)
   *
   * Whether the sorter will sort smaller numbers first.
   */
  properties[PROP_SORT_ORDER] =
    g_param_spec_enum ("sort-order", NULL, NULL,
                       GTK_TYPE_SORT_TYPE,
                       GTK_SORT_ASCENDING,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_numeric_sorter_init (GtkNumericSorter *self)
{
  self->sort_order = GTK_SORT_ASCENDING;

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_numeric_sort_keys_new (self));
}

/**
 * gtk_numeric_sorter_new:
 * @expression: (transfer full) (nullable): The expression to evaluate
 *
 * Creates a new numeric sorter using the given @expression.
 *
 * Smaller numbers will be sorted first. You can call
 * [method@Gtk.NumericSorter.set_sort_order] to change this.
 *
 * Returns: a new `GtkNumericSorter`
 */
GtkNumericSorter *
gtk_numeric_sorter_new (GtkExpression *expression)
{
  GtkNumericSorter *result;

  result = g_object_new (GTK_TYPE_NUMERIC_SORTER,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_numeric_sorter_get_expression: (attributes org.gtk.Method.get_property=expression)
 * @self: a `GtkNumericSorter`
 *
 * Gets the expression that is evaluated to obtain numbers from items.
 *
 * Returns: (transfer none) (nullable): a `GtkExpression`
 */
GtkExpression *
gtk_numeric_sorter_get_expression (GtkNumericSorter *self)
{
  g_return_val_if_fail (GTK_IS_NUMERIC_SORTER (self), NULL);

  return self->expression;
}

/**
 * gtk_numeric_sorter_set_expression: (attributes org.gtk.Method.set_property=expression)
 * @self: a `GtkNumericSorter`
 * @expression: (nullable) (transfer none): a `GtkExpression`
 *
 * Sets the expression that is evaluated to obtain numbers from items.
 *
 * Unless an expression is set on @self, the sorter will always
 * compare items as invalid.
 *
 * The expression must have a return type that can be compared
 * numerically, such as %G_TYPE_INT or %G_TYPE_DOUBLE.
 */
void
gtk_numeric_sorter_set_expression (GtkNumericSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  if (expression)
    self->expression = gtk_expression_ref (expression);

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_numeric_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_numeric_sorter_set_sort_order: (attributes org.gtk.Method.set_property=sort-order)
 * @self: a `GtkNumericSorter`
 * @sort_order: whether to sort smaller numbers first
 *
 * Sets whether to sort smaller numbers before larger ones.
 */
void
gtk_numeric_sorter_set_sort_order (GtkNumericSorter *self,
                                   GtkSortType       sort_order)
{
  g_return_if_fail (GTK_IS_NUMERIC_SORTER (self));

  if (self->sort_order == sort_order)
    return;

  self->sort_order = sort_order;

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_INVERTED,
                                gtk_numeric_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_ORDER]);
}

/**
 * gtk_numeric_sorter_get_sort_order: (attributes org.gtk.Method.get_property=sort-order)
 * @self: a `GtkNumericSorter`
 *
 * Gets whether this sorter will sort smaller numbers first.
 *
 * Returns: the order of the numbers
 */
GtkSortType
gtk_numeric_sorter_get_sort_order (GtkNumericSorter *self)
{
  g_return_val_if_fail (GTK_IS_NUMERIC_SORTER (self), GTK_SORT_ASCENDING);

  return self->sort_order;
}
