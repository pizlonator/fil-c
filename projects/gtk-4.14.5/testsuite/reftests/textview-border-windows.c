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

static void
set_border_window_size (GtkTextView       *text_view,
                        GtkTextWindowType  win,
                        int                size)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_widget_set_size_request (label, size, size);
  gtk_text_view_set_gutter (text_view, win, label);
}

G_MODULE_EXPORT void
add_border_windows (GtkTextView *text_view)
{
  set_border_window_size (text_view, GTK_TEXT_WINDOW_LEFT, 30);
  set_border_window_size (text_view, GTK_TEXT_WINDOW_RIGHT, 30);
  set_border_window_size (text_view, GTK_TEXT_WINDOW_TOP, 30);
  set_border_window_size (text_view, GTK_TEXT_WINDOW_BOTTOM, 30);
}
