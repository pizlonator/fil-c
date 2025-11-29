/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkprivate.h"
#include "gtkcssvalueprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

#undef CSS_VALUE_ACCOUNTING

#ifdef CSS_VALUE_ACCOUNTING
static GHashTable *counters;

typedef struct
{
  guint all;
  guint alive;
  guint computed;
  guint transitioned;
} ValueAccounting;

static void
dump_value_counts (void)
{
  int col_widths[5] = { 0, strlen ("all"), strlen ("alive"), strlen ("computed"), strlen("transitioned") };
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  int sum_all = 0, sum_alive = 0, sum_computed = 0, sum_transitioned = 0;

  g_hash_table_iter_init (&iter, counters);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
       const char *class = key;
       const ValueAccounting *c = value;
       char *str;

       sum_all += c->all;
       sum_alive += c->alive;
       sum_computed += c->computed;
       sum_transitioned += c->transitioned;

       col_widths[0] = MAX (col_widths[0], strlen (class));

       str = g_strdup_printf ("%'d", sum_all);
       col_widths[1] = MAX (col_widths[1], strlen (str));
       g_free (str);

       str = g_strdup_printf ("%'d", sum_alive);
       col_widths[2] = MAX (col_widths[2], strlen (str));
       g_free (str);

       str = g_strdup_printf ("%'d", sum_computed);
       col_widths[3] = MAX (col_widths[3], strlen (str));
       g_free (str);

       str = g_strdup_printf ("%'d", sum_transitioned);
       col_widths[4] = MAX (col_widths[4], strlen (str));
       g_free (str);
    }
  /* Some spacing */
  col_widths[0] += 4;
  col_widths[1] += 4;
  col_widths[2] += 4;
  col_widths[3] += 4;
  col_widths[4] += 4;

  g_print("%*s%*s%*s%*s%*s\n", col_widths[0] + 1, " ",
          col_widths[1] + 1, "All",
          col_widths[2] + 1, "Alive",
          col_widths[3] + 1, "Computed",
          col_widths[4] + 1, "Transitioned");

  g_hash_table_iter_init (&iter, counters);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
       const char *class = key;
       const ValueAccounting *c = value;

       g_print ("%*s:", col_widths[0], class);
       g_print (" %'*d", col_widths[1], c->all);
       g_print (" %'*d", col_widths[2], c->alive);
       g_print (" %'*d", col_widths[3], c->computed);
       g_print (" %'*d", col_widths[4], c->transitioned);
       g_print("\n");
    }

  g_print("%*s%'*d%'*d%'*d%'*d\n", col_widths[0] + 1, " ",
          col_widths[1] + 1, sum_all,
          col_widths[2] + 1, sum_alive,
          col_widths[3] + 1, sum_computed,
          col_widths[4] + 1, sum_transitioned);
}

static ValueAccounting *
get_accounting_data (const char *class)
{
  ValueAccounting *c;

  if (!counters)
    {
      counters = g_hash_table_new (g_str_hash, g_str_equal);
      atexit (dump_value_counts);
    }
  c = g_hash_table_lookup (counters, class);
  if (!c)
    {
       c = g_malloc0 (sizeof (ValueAccounting));
       g_hash_table_insert (counters, (gpointer)class, c);
    }

  return c;
}
#endif

GtkCssValue *
_gtk_css_value_alloc (const GtkCssValueClass *klass,
                      gsize                   size)
{
  GtkCssValue *value;

  value = g_malloc0 (size);

  value->class = klass;
  value->ref_count = 1;

#ifdef CSS_VALUE_ACCOUNTING
  {
    ValueAccounting *c;

    c = get_accounting_data (klass->type_name);
    c->all++;
    c->alive++;
  }
#endif

  return value;
}

GtkCssValue *
gtk_css_value_ref (GtkCssValue *value)
{
  gtk_internal_return_val_if_fail (value != NULL, NULL);

  value->ref_count += 1;

  return value;
}

void
gtk_css_value_unref (GtkCssValue *value)
{
  if (value == NULL)
    return;

  value->ref_count -= 1;
  if (value->ref_count > 0)
    return;

#ifdef CSS_VALUE_ACCOUNTING
  {
    ValueAccounting *c;

    c = get_accounting_data (value->class->type_name);
    c->alive--;
  }
#endif

  value->class->free (value);
}

/**
 * _gtk_css_value_compute:
 * @value: the value to compute from
 * @property_id: the ID of the property to compute
 * @provider: Style provider for looking up extra information
 * @style: Style to compute for
 * @parent_style: parent style to use for inherited values
 *
 * Converts the specified @value into the computed value for the CSS
 * property given by @property_id using the information in @context.
 * This step is explained in detail in the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#computed).
 *
 * Returns: the computed value
 **/
GtkCssValue *
_gtk_css_value_compute (GtkCssValue      *value,
                        guint             property_id,
                        GtkStyleProvider *provider,
                        GtkCssStyle      *style,
                        GtkCssStyle      *parent_style)
{
  if (gtk_css_value_is_computed (value))
    return _gtk_css_value_ref (value);

#ifdef CSS_VALUE_ACCOUNTING
  get_accounting_data (value->class->type_name)->computed++;
#endif

  return value->class->compute (value, property_id, provider, style, parent_style);
}

gboolean
_gtk_css_value_equal (const GtkCssValue *value1,
                      const GtkCssValue *value2)
{
  gtk_internal_return_val_if_fail (value1 != NULL, FALSE);
  gtk_internal_return_val_if_fail (value2 != NULL, FALSE);

  if (value1 == value2)
    return TRUE;

  if (value1->class != value2->class)
    return FALSE;

  return value1->class->equal (value1, value2);
}

gboolean
_gtk_css_value_equal0 (const GtkCssValue *value1,
                       const GtkCssValue *value2)
{
  /* Includes both values being NULL */
  if (value1 == value2)
    return TRUE;

  if (value1 == NULL || value2 == NULL)
    return FALSE;

  return _gtk_css_value_equal (value1, value2);
}

GtkCssValue *
_gtk_css_value_transition (GtkCssValue *start,
                           GtkCssValue *end,
                           guint        property_id,
                           double       progress)
{
  gtk_internal_return_val_if_fail (start != NULL, NULL);
  gtk_internal_return_val_if_fail (end != NULL, NULL);

  if (start->class != end->class)
    return NULL;

  if (progress == 0)
    return _gtk_css_value_ref (start);

  if (progress == 1)
    return _gtk_css_value_ref (end);

  if (start == end)
    return _gtk_css_value_ref (start);

#ifdef CSS_VALUE_ACCOUNTING
  get_accounting_data (start->class->type_name)->transitioned++;
#endif

  return start->class->transition (start, end, property_id, progress);
}

char *
_gtk_css_value_to_string (const GtkCssValue *value)
{
  GString *string;

  gtk_internal_return_val_if_fail (value != NULL, NULL);

  string = g_string_new (NULL);
  _gtk_css_value_print (value, string);
  return g_string_free (string, FALSE);
}

/**
 * _gtk_css_value_print:
 * @value: the value to print
 * @string: the string to print to
 *
 * Prints @value to the given @string in CSS format. The @value must be a
 * valid specified value as parsed using the parse functions or as assigned
 * via _gtk_style_property_assign().
 **/
void
_gtk_css_value_print (const GtkCssValue *value,
                      GString           *string)
{
  gtk_internal_return_if_fail (value != NULL);
  gtk_internal_return_if_fail (string != NULL);

  value->class->print (value, string);
}

/**
 * gtk_css_value_is_dynamic:
 * @value: a `GtkCssValue`
 *
 * A "dynamic" value has a different value at different times. This means that
 * the value needs to be animated when time is progressing.
 *
 * Examples of dynamic values are animated images, such as videos or dynamic shaders.
 *
 * Use gtk_css_value_get_dynamic_value() to get the value for a given timestamp.
 *
 * Returns %TRUE if the value is dynamic
 */
gboolean
gtk_css_value_is_dynamic (const GtkCssValue *value)
{
  gtk_internal_return_val_if_fail (value != NULL, FALSE);

  if (!value->class->is_dynamic)
    return FALSE;

  return value->class->is_dynamic (value);
}

/**
 * gtk_css_value_get_dynamic_value:
 * @value: a `GtkCssValue`
 * @monotonic_time: the timestamp for which to get the dynamic value
 *
 * Gets the dynamic value for a given timestamp. If @monotonic_time is 0,
 * the default value is returned.
 *
 * See gtk_css_value_is_dynamic() for details about dynamic values.
 *
 * Returns: (transfer full): The dynamic value for @value at the given
 *   timestamp
 */
GtkCssValue *
gtk_css_value_get_dynamic_value (GtkCssValue *value,
                                 gint64       monotonic_time)
{
  gtk_internal_return_val_if_fail (value != NULL, NULL);

  if (!value->class->get_dynamic_value)
    return gtk_css_value_ref (value);

  return value->class->get_dynamic_value (value, monotonic_time);
}

gboolean
gtk_css_value_is_computed (const GtkCssValue *value)
{
  return value->is_computed;
}
