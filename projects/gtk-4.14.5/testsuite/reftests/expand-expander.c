/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
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

#include "gtk-reftest.h"

static gboolean
unblock (gpointer data)
{
  reftest_uninhibit_snapshot ();
  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT void
expand_expander (GtkWidget *widget)
{
  reftest_inhibit_snapshot ();
  gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);
  g_timeout_add (500, unblock, NULL);
}
