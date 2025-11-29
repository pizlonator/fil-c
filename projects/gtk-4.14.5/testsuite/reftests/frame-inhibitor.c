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
tick_callback_for_1_frame (GtkWidget     *widget,
                           GdkFrameClock *frame_clock,
                           gpointer       unused)
{
  reftest_uninhibit_snapshot ();
  gtk_widget_queue_draw (widget);

  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT gboolean
inhibit_for_1_frame (GtkWidget *widget)
{
  reftest_inhibit_snapshot ();
  gtk_widget_add_tick_callback (widget,
                                tick_callback_for_1_frame,
                                NULL, NULL);

  return FALSE;
}

static gboolean 
tick_callback_for_2_frames (GtkWidget     *widget,
                            GdkFrameClock *frame_clock,
                            gpointer       unused)
{
  inhibit_for_1_frame (widget);
  reftest_uninhibit_snapshot ();

  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT gboolean
inhibit_for_2_frames (GtkWidget *widget)
{
  reftest_inhibit_snapshot ();
  gtk_widget_add_tick_callback (widget,
                                tick_callback_for_2_frames,
                                NULL, NULL);

  return FALSE;
}

static gboolean 
tick_callback_for_3_frames (GtkWidget     *widget,
                            GdkFrameClock *frame_clock,
                            gpointer       unused)
{
  inhibit_for_2_frames (widget);
  reftest_uninhibit_snapshot ();

  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT gboolean
inhibit_for_3_frames (GtkWidget *widget)
{
  reftest_inhibit_snapshot ();
  gtk_widget_add_tick_callback (widget,
                                tick_callback_for_3_frames,
                                NULL, NULL);

  return FALSE;
}

G_MODULE_EXPORT gboolean
add_reference_class_if_no_animation (GtkWidget *widget)
{
  gboolean enabled;

  g_object_get (gtk_widget_get_settings (widget), "gtk-enable-animations", &enabled, NULL);
  if (enabled)
    return FALSE;

  g_message ("Adding reference class because animation is disabled");

  gtk_widget_add_css_class (widget, "reference");

  return FALSE;
}
