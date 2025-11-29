/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsslookupprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkprivate.h"

void
_gtk_css_lookup_init (GtkCssLookup     *lookup)
{
  memset (lookup, 0, sizeof (*lookup));

  lookup->set_values = _gtk_bitmask_new ();
}

void
_gtk_css_lookup_destroy (GtkCssLookup *lookup)
{
  _gtk_bitmask_free (lookup->set_values);
}

gboolean
_gtk_css_lookup_is_missing (const GtkCssLookup *lookup,
                            guint               id)
{
  gtk_internal_return_val_if_fail (lookup != NULL, FALSE);

  return !_gtk_bitmask_get (lookup->set_values, id);
}

/**
 * _gtk_css_lookup_set:
 * @lookup: the lookup
 * @id: id of the property to set, see _gtk_style_property_get_id()
 * @section: (nullable): The @section the value was defined in
 * @value: the “cascading value” to use
 *
 * Sets the @value for a given @id. No value may have been set for @id
 * before. See _gtk_css_lookup_is_missing(). This function is used to
 * set the “winning declaration” of a lookup. Note that for performance
 * reasons @value and @section are not copied. It is your responsibility
 * to ensure they are kept alive until _gtk_css_lookup_free() is called.
 **/
void
_gtk_css_lookup_set (GtkCssLookup  *lookup,
                     guint          id,
                     GtkCssSection *section,
                     GtkCssValue   *value)
{
  gtk_internal_return_if_fail (lookup != NULL);
  gtk_internal_return_if_fail (value != NULL);
  gtk_internal_return_if_fail (lookup->values[id].value == NULL);

  lookup->values[id].value = value;
  lookup->values[id].section = section;
  lookup->set_values = _gtk_bitmask_set (lookup->set_values, id, TRUE);
}
