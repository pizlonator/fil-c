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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_MODULE_EXPORT void
statusbar_remove_all (GtkStatusbar *s)
{
  gtk_statusbar_push (s, 0, "One");
  gtk_statusbar_push (s, 0, "Two");
  gtk_statusbar_remove_all (s, 0);
}
