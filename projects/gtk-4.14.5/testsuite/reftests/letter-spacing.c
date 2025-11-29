/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>


G_MODULE_EXPORT void
set_letter_spacing (GtkLabel *label)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_letter_spacing_new (10 * PANGO_SCALE));
  gtk_label_set_attributes (label, attrs);
  pango_attr_list_unref (attrs);
}
