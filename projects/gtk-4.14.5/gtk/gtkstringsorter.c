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

#include "gtkstringsorter.h"

#include "gtksorterprivate.h"
#include "gtktypebuiltins.h"

/**
 * GtkStringSorter:
 *
 * `GtkStringSorter` is a `GtkSorter` that compares strings.
 *
 * It does the comparison in a linguistically correct way using the
 * current locale by normalizing Unicode strings and possibly case-folding
 * them before performing the comparison.
 *
 * To obtain the strings to compare, this sorter evaluates a
 * [class@Gtk.Expression].
 */

struct _GtkStringSorter
{
  GtkSorter parent_instance;

  gboolean ignore_case;
  GtkCollation collation;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_IGNORE_CASE,
  PROP_COLLATION,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringSorter, gtk_string_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static char *
gtk_string_sorter_get_key (GtkExpression *expression,
                           gboolean       ignore_case,
                           GtkCollation   collation,
                           gpointer       item1)
{
  GValue value = G_VALUE_INIT;
  const char *string;
  char *s;
  char *key;

  if (expression == NULL)
    return NULL;

  if (!gtk_expression_evaluate (expression, item1, &value))
    return NULL;

  string = g_value_get_string (&value);
  if (string == NULL)
    {
      g_value_unset (&value);
      return NULL;
    }

  if (ignore_case)
    s = g_utf8_casefold (string, -1);
  else
    s = (char *) string;

  switch (collation)
    {
    case GTK_COLLATION_NONE:
      if (ignore_case)
        key = g_steal_pointer (&s);
      else
        key = g_strdup (s);
      break;

    case GTK_COLLATION_UNICODE:
      key = g_utf8_collate_key (s, -1);
      break;

    case GTK_COLLATION_FILENAME:
      key = g_utf8_collate_key_for_filename (s, -1);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (s != string)
    g_free (s);

  g_value_unset (&value);

  return key;
}

static GtkOrdering
gtk_string_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkStringSorter *self = GTK_STRING_SORTER (sorter);
  char *s1, *s2;
  GtkOrdering result;

  if (self->expression == NULL)
    return GTK_ORDERING_EQUAL;

  s1 = gtk_string_sorter_get_key (self->expression, self->ignore_case, self->collation, item1);
  s2 = gtk_string_sorter_get_key (self->expression, self->ignore_case, self->collation, item2);

  result = gtk_ordering_from_cmpfunc (g_strcmp0 (s1, s2));

  g_free (s1);
  g_free (s2);

  return result;
}

static GtkSorterOrder
gtk_string_sorter_get_order (GtkSorter *sorter)
{
  GtkStringSorter *self = GTK_STRING_SORTER (sorter);

  if (self->expression == NULL)
    return GTK_SORTER_ORDER_NONE;

  return GTK_SORTER_ORDER_PARTIAL;
}

typedef struct _GtkStringSortKeys GtkStringSortKeys;
struct _GtkStringSortKeys
{
  GtkSortKeys keys;

  GtkExpression *expression;
  gboolean ignore_case;
  GtkCollation collation;
};

static void
gtk_string_sort_keys_free (GtkSortKeys *keys)
{
  GtkStringSortKeys *self = (GtkStringSortKeys *) keys;

  gtk_expression_unref (self->expression);
  g_free (self);
}

static int
gtk_string_sort_keys_compare (gconstpointer a,
                              gconstpointer b,
                              gpointer      unused)
{
  const char *sa = *(const char **) a;
  const char *sb = *(const char **) b;

  if (sa == NULL)
    return sb == NULL ? GTK_ORDERING_EQUAL : GTK_ORDERING_LARGER;
  else if (sb == NULL)
    return GTK_ORDERING_SMALLER;

  return gtk_ordering_from_cmpfunc (strcmp (sa, sb));
}

static gboolean
gtk_string_sort_keys_is_compatible (GtkSortKeys *keys,
                                    GtkSortKeys *other)
{
  return FALSE;
}

static void
gtk_string_sort_keys_init_key (GtkSortKeys *keys,
                               gpointer     item,
                               gpointer     key_memory)
{
  GtkStringSortKeys *self = (GtkStringSortKeys *) keys;
  char **key = (char **) key_memory;

  *key = gtk_string_sorter_get_key (self->expression, self->ignore_case, self->collation, item);
}

static void
gtk_string_sort_keys_clear_key (GtkSortKeys *keys,
                                gpointer     key_memory)
{
  char **key = (char **) key_memory;

  g_free (*key);
}

static const GtkSortKeysClass GTK_STRING_SORT_KEYS_CLASS =
{
  gtk_string_sort_keys_free,
  gtk_string_sort_keys_compare,
  gtk_string_sort_keys_is_compatible,
  gtk_string_sort_keys_init_key,
  gtk_string_sort_keys_clear_key,
};

static GtkSortKeys *
gtk_string_sort_keys_new (GtkStringSorter *self)
{
  GtkStringSortKeys *result;

  if (self->expression == NULL)
    return gtk_sort_keys_new_equal ();

  result = gtk_sort_keys_new (GtkStringSortKeys,
                              &GTK_STRING_SORT_KEYS_CLASS,
                              sizeof (char *),
                              G_ALIGNOF (char *));

  result->expression = gtk_expression_ref (self->expression);
  result->ignore_case = self->ignore_case;
  result->collation = self->collation;

  return (GtkSortKeys *) result;
}

static void
gtk_string_sorter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_string_sorter_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_IGNORE_CASE:
      gtk_string_sorter_set_ignore_case (self, g_value_get_boolean (value));
      break;

    case PROP_COLLATION:
      gtk_string_sorter_set_collation (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_sorter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
      break;

    case PROP_IGNORE_CASE:
      g_value_set_boolean (value, self->ignore_case);
      break;

    case PROP_COLLATION:
      g_value_set_enum (value, self->collation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_sorter_dispose (GObject *object)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_string_sorter_parent_class)->dispose (object);
}

static void
gtk_string_sorter_class_init (GtkStringSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_string_sorter_compare;
  sorter_class->get_order = gtk_string_sorter_get_order;

  object_class->get_property = gtk_string_sorter_get_property;
  object_class->set_property = gtk_string_sorter_set_property;
  object_class->dispose = gtk_string_sorter_dispose;

  /**
   * GtkStringSorter:expression: (type GtkExpression) (attributes org.gtk.Property.get=gtk_string_sorter_get_expression org.gtk.Property.set=gtk_string_sorter_set_expression)
   *
   * The expression to evaluate on item to get a string to compare with.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression", NULL, NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringSorter:ignore-case: (attributes org.gtk.Property.get=gtk_string_sorter_get_ignore_case org.gtk.Property.set=gtk_string_sorter_set_ignore_case)
   *
   * If sorting is case sensitive.
   */
  properties[PROP_IGNORE_CASE] =
      g_param_spec_boolean ("ignore-case", NULL, NULL,
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringSorter:collation: (attributes org.gtk.Property.get=gtk_string_sorter_get_collation org.gtk.Property.set=gtk_string_sorter_set_collation)
   *
   * The collation method to use for sorting.
   *
   * The `GTK_COLLATION_NONE` value is useful when the expression already
   * returns collation keys, or strings that need to be compared byte-by-byte.
   *
   * The default value, `GTK_COLLATION_UNICODE`, compares strings according
   * to the [Unicode collation algorithm](https://www.unicode.org/reports/tr10/).
   *
   * Since: 4.10
   */
  properties[PROP_COLLATION] =
      g_param_spec_enum ("collation", NULL, NULL,
                         GTK_TYPE_COLLATION,
                         GTK_COLLATION_UNICODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_string_sorter_init (GtkStringSorter *self)
{
  self->ignore_case = TRUE;
  self->collation = GTK_COLLATION_UNICODE;

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_string_sort_keys_new (self));
}

/**
 * gtk_string_sorter_new:
 * @expression: (transfer full) (nullable): The expression to evaluate
 *
 * Creates a new string sorter that compares items using the given
 * @expression.
 *
 * Unless an expression is set on it, this sorter will always
 * compare items as invalid.
 *
 * Returns: a new `GtkStringSorter`
 */
GtkStringSorter *
gtk_string_sorter_new (GtkExpression *expression)
{
  GtkStringSorter *result;

  result = g_object_new (GTK_TYPE_STRING_SORTER,
                         "expression", expression,
                         NULL);

  g_clear_pointer (&expression, gtk_expression_unref);

  return result;
}

/**
 * gtk_string_sorter_get_expression: (attributes org.gtk.Method.get_property=expression)
 * @self: a `GtkStringSorter`
 *
 * Gets the expression that is evaluated to obtain strings from items.
 *
 * Returns: (transfer none) (nullable): a `GtkExpression`
 */
GtkExpression *
gtk_string_sorter_get_expression (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), NULL);

  return self->expression;
}

/**
 * gtk_string_sorter_set_expression: (attributes org.gtk.Method.set_property=expression)
 * @self: a `GtkStringSorter`
 * @expression: (nullable) (transfer none): a `GtkExpression`
 *
 * Sets the expression that is evaluated to obtain strings from items.
 *
 * The expression must have the type %G_TYPE_STRING.
 */
void
gtk_string_sorter_set_expression (GtkStringSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));
  g_return_if_fail (expression == NULL || gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  if (expression)
    self->expression = gtk_expression_ref (expression);

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_string_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_string_sorter_get_ignore_case: (attributes org.gtk.Method.get_property=ignore-case)
 * @self: a `GtkStringSorter`
 *
 * Gets whether the sorter ignores case differences.
 *
 * Returns: %TRUE if @self is ignoring case differences
 */
gboolean
gtk_string_sorter_get_ignore_case (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), TRUE);

  return self->ignore_case;
}

/**
 * gtk_string_sorter_set_ignore_case: (attributes org.gtk.Method.set_property=ignore-case)
 * @self: a `GtkStringSorter`
 * @ignore_case: %TRUE to ignore case differences
 *
 * Sets whether the sorter will ignore case differences.
 */
void
gtk_string_sorter_set_ignore_case (GtkStringSorter *self,
                                   gboolean         ignore_case)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));

  if (self->ignore_case == ignore_case)
    return;

  self->ignore_case = ignore_case;

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                ignore_case ? GTK_SORTER_CHANGE_LESS_STRICT : GTK_SORTER_CHANGE_MORE_STRICT,
                                gtk_string_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_CASE]);
}

/**
 * gtk_string_sorter_get_collation: (attributes org.gtk.Method.get_property=collation)
 * @self: a `GtkStringSorter`
 *
 * Gets which collation method the sorter uses.
 *
 * Returns: The collation method
 *
 * Since: 4.10
 */
GtkCollation
gtk_string_sorter_get_collation (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), GTK_COLLATION_UNICODE);

  return self->collation;
}

/**
 * gtk_string_sorter_set_collation: (attributes org.gtk.Method.set_property=collation)
 * @self: a `GtkStringSorter`
 * @collation: the collation method
 *
 * Sets the collation method to use for sorting.
 *
 * Since: 4.10
 */
void
gtk_string_sorter_set_collation (GtkStringSorter *self,
                                 GtkCollation     collation)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));

  if (self->collation == collation)
    return;

  self->collation = collation;

  gtk_sorter_changed_with_keys (GTK_SORTER (self),
                                GTK_SORTER_CHANGE_DIFFERENT,
                                gtk_string_sort_keys_new (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLLATION]);
}
