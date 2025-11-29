/* gtkaccessibleattributeset.c: Accessible attribute content
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

#include "gtkaccessibleattributesetprivate.h"

#include "gtkbitmaskprivate.h"
#include "gtkenums.h"

struct _GtkAccessibleAttributeSet
{
  gsize n_attributes;

  GtkAccessibleAttributeNameFunc name_func;
  GtkAccessibleAttributeDefaultFunc default_func;

  GtkBitmask *attributes_set;

  GtkAccessibleValue **attribute_values;
};

static GtkAccessibleAttributeSet *
gtk_accessible_attribute_set_init (GtkAccessibleAttributeSet          *self,
                                   gsize                               n_attributes,
                                   GtkAccessibleAttributeNameFunc      name_func,
                                   GtkAccessibleAttributeDefaultFunc   default_func)
{
  self->n_attributes = n_attributes;
  self->name_func = name_func;
  self->default_func = default_func;
  self->attribute_values = g_new (GtkAccessibleValue *, n_attributes);
  self->attributes_set = _gtk_bitmask_new ();

  /* Initialize all attribute values, so we can always get the full attribute */
  for (int i = 0; i < self->n_attributes; i++)
    self->attribute_values[i] = (* self->default_func) (i);

  return self;
}

GtkAccessibleAttributeSet *
gtk_accessible_attribute_set_new (gsize                               n_attributes,
                                  GtkAccessibleAttributeNameFunc      name_func,
                                  GtkAccessibleAttributeDefaultFunc   default_func)
{
  GtkAccessibleAttributeSet *set = g_rc_box_new0 (GtkAccessibleAttributeSet);

  return gtk_accessible_attribute_set_init (set, n_attributes, name_func, default_func);
}

GtkAccessibleAttributeSet *
gtk_accessible_attribute_set_ref (GtkAccessibleAttributeSet *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_rc_box_acquire (self);
}

static void
gtk_accessible_attribute_set_free (gpointer data)
{
  GtkAccessibleAttributeSet *self = data;

  for (int i = 0; i < self->n_attributes; i++)
    {
      if (self->attribute_values[i] != NULL)
        gtk_accessible_value_unref (self->attribute_values[i]);
    }

  g_free (self->attribute_values);

  _gtk_bitmask_free (self->attributes_set);
}

void
gtk_accessible_attribute_set_unref (GtkAccessibleAttributeSet *self)
{
  g_rc_box_release_full (self, gtk_accessible_attribute_set_free);
}

/*< private >
 * gtk_accessible_attribute_set_add:
 * @self: a `GtkAccessibleAttributeSet`
 * @attribute: the attribute to set
 * @value: (nullable): a `GtkAccessibleValue`
 *
 * Adds @attribute to the attributes set, and sets its value.
 *
 * If @value is %NULL, the @attribute is reset to its default value.
 *
 * If you want to remove @attribute from the set, use gtk_accessible_attribute_set_remove()
 * instead.
 *
 * Returns: %TRUE if the set was modified, and %FALSE otherwise
 */
gboolean
gtk_accessible_attribute_set_add (GtkAccessibleAttributeSet *self,
                                  int                        attribute,
                                  GtkAccessibleValue        *value)
{
  g_return_val_if_fail (attribute >= 0 && attribute < self->n_attributes, FALSE);

  if (value != NULL)
    {
      if (gtk_accessible_value_equal (value, self->attribute_values[attribute]))
        {
          if (!_gtk_bitmask_get (self->attributes_set, attribute))
            {
              self->attributes_set = _gtk_bitmask_set (self->attributes_set, attribute, TRUE);
              return TRUE;
            }
          else
            return FALSE;
        }
    }
  else
    {
      if (!_gtk_bitmask_get (self->attributes_set, attribute))
        return FALSE;
    }

  g_clear_pointer (&(self->attribute_values[attribute]), gtk_accessible_value_unref);

  if (value != NULL)
    self->attribute_values[attribute] = gtk_accessible_value_ref (value);
  else
    self->attribute_values[attribute] = (* self->default_func) (attribute);

  self->attributes_set = _gtk_bitmask_set (self->attributes_set, attribute, TRUE);

  return TRUE;
}

/*< private >
 * gtk_accessible_attribute_set_remove:
 * @self: a `GtkAccessibleAttributeSet`
 * @attribute: the attribute to be removed
 *
 * Resets the @attribute in the given `GtkAccessibleAttributeSet`
 * to its default value.
 *
 * Returns: %TRUE if the set was modified, and %FALSE otherwise
 */
gboolean
gtk_accessible_attribute_set_remove (GtkAccessibleAttributeSet *self,
                                     int                        attribute)
{
  g_return_val_if_fail (attribute >= 0 && attribute < self->n_attributes, FALSE);

  if (!_gtk_bitmask_get (self->attributes_set, attribute))
    return FALSE;

  g_clear_pointer (&(self->attribute_values[attribute]), gtk_accessible_value_unref);

  self->attribute_values[attribute] = (* self->default_func) (attribute);
  self->attributes_set = _gtk_bitmask_set (self->attributes_set, attribute, FALSE);

  return TRUE;
}

gboolean
gtk_accessible_attribute_set_contains (GtkAccessibleAttributeSet *self,
                                       int                        attribute)
 {
  g_return_val_if_fail (attribute >= 0 && attribute < self->n_attributes, FALSE);

  return _gtk_bitmask_get (self->attributes_set, attribute);
}

/*< private >
 * gtk_accessible_attribute_set_get_value:
 * @self: a `GtkAccessibleAttributeSet`
 * @attribute: the attribute to retrieve
 *
 * Retrieves the value of the given @attribute in the set.
 *
 * Returns: (transfer none): the value for the attribute
 */
GtkAccessibleValue *
gtk_accessible_attribute_set_get_value (GtkAccessibleAttributeSet *self,
                                        int                        attribute)
{
  g_return_val_if_fail (attribute >= 0 && attribute < self->n_attributes, NULL);

  return self->attribute_values[attribute];
}

gsize
gtk_accessible_attribute_set_get_length (GtkAccessibleAttributeSet *self)
{
  return self->n_attributes;
}

guint
gtk_accessible_attribute_set_get_changed (GtkAccessibleAttributeSet *self)
{
  guint changed = 0;

  for (gsize i = 0; i < self->n_attributes; i++)
    {
      if (gtk_accessible_attribute_set_contains (self, i))
        changed |= (1 << i);
    }

  return changed;
}

/*< private >
 * gtk_accessible_attribute_set_print:
 * @self: a `GtkAccessibleAttributeSet`
 * @only_set: %TRUE if only the set attributes should be printed
 * @buffer: a `GString`
 *
 * Prints the contents of the `GtkAccessibleAttributeSet` into @buffer.
 */
void
gtk_accessible_attribute_set_print (GtkAccessibleAttributeSet *self,
                                    gboolean                   only_set,
                                    GString                   *buffer)
{
  if (only_set && _gtk_bitmask_is_empty (self->attributes_set))
    {
      g_string_append (buffer, "{}");
      return;
    }

  g_string_append (buffer, "{\n");

  for (gsize i = 0; i < self->n_attributes; i++)
    {
      if (only_set && !_gtk_bitmask_get (self->attributes_set, i))
        continue;

      g_string_append (buffer, "    ");
      g_string_append (buffer, self->name_func (i));
      g_string_append (buffer, ": ");

      gtk_accessible_value_print (self->attribute_values[i], buffer);

      g_string_append (buffer, ",\n");
    }

  g_string_append (buffer, "}");
}

/*< private >
 * gtk_accessible_attribute_set_to_string:
 * @self: a `GtkAccessibleAttributeSet`
 *
 * Prints the contents of a `GtkAccessibleAttributeSet` into a string.
 *
 * Returns: (transfer full): a newly allocated string with the contents
 *   of the `GtkAccessibleAttributeSet`
 */
char *
gtk_accessible_attribute_set_to_string (GtkAccessibleAttributeSet *self)
{
  GString *buf = g_string_new (NULL);

  gtk_accessible_attribute_set_print (self, TRUE, buf);

  return g_string_free (buf, FALSE);
}
