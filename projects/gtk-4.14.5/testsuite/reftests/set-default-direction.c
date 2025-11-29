/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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


G_MODULE_EXPORT void
set_default_direction_ltr (void)
{
  g_test_message ("Attention: globally setting default text direction to LTR");
  gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
}

G_MODULE_EXPORT void
set_default_direction_rtl (void)
{
  g_test_message ("Attention: globally setting default text direction to RTL");
  gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
}

G_MODULE_EXPORT void
switch_default_direction (void)
{
  switch (gtk_widget_get_default_direction ())
    {
    case GTK_TEXT_DIR_LTR:
      g_test_message ("Attention: globally switching default text direction from LTR to RTL");
      gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
      break;
    case GTK_TEXT_DIR_RTL:
      g_test_message ("Attention: globally switching default text direction from RTL to LTR");
      gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
      break;
    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

G_MODULE_EXPORT void
switch_direction (GtkWidget *widget)
{
  switch (gtk_widget_get_direction (widget))
    {
    case GTK_TEXT_DIR_LTR:
      gtk_widget_set_direction (widget, GTK_TEXT_DIR_RTL);
      break;
    case GTK_TEXT_DIR_RTL:
      gtk_widget_set_direction (widget, GTK_TEXT_DIR_LTR);
      break;
    case GTK_TEXT_DIR_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

G_MODULE_EXPORT void
swap_child (GtkWidget *window)
{
  GtkWidget *image;

  image = gtk_image_new_from_icon_name ("go-next");
  gtk_window_set_child (GTK_WINDOW (window), image);
}
