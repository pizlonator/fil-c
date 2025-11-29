
/* This library is free software; you can redistribute it and/or
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
hello (GtkButton *button)
{
  g_print ("Hello!\n");
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *fixed, *button;
  GtkWidget *fixed2, *frame;
  gboolean done = FALSE;
  GskTransform *transform;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  fixed = gtk_fixed_new ();
  gtk_widget_set_halign (fixed, GTK_ALIGN_FILL);
  gtk_widget_set_valign (fixed, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (fixed, TRUE);
  gtk_widget_set_vexpand (fixed, TRUE);

  button = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "Button");
  g_signal_connect (button, "clicked", G_CALLBACK (hello), NULL);

  gtk_fixed_put (GTK_FIXED (fixed), button, 0, 0);

  transform = NULL;
  transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (0, 0, 50));
  transform = gsk_transform_perspective (transform, 170);
  transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (50, 0, 50));
  transform = gsk_transform_rotate (transform, 20);
  transform = gsk_transform_rotate_3d (transform, 20, graphene_vec3_y_axis ());
  gtk_fixed_set_child_transform (GTK_FIXED (fixed), button, transform);

  frame = gtk_frame_new ("Frame");
  gtk_widget_add_css_class (frame, "view");
  gtk_frame_set_child (GTK_FRAME (frame), fixed);

  fixed2 = gtk_fixed_new ();

  gtk_fixed_put (GTK_FIXED (fixed2), frame, 0, 0);

  transform = NULL;
  transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (0, 0, 50));
  transform = gsk_transform_perspective (transform, 170);
  transform = gsk_transform_translate_3d (transform, &GRAPHENE_POINT3D_INIT (50, 0, 50));
  transform = gsk_transform_rotate (transform, 20);
  transform = gsk_transform_rotate_3d (transform, 20, graphene_vec3_y_axis ());
  gtk_fixed_set_child_transform (GTK_FIXED (fixed2), frame, transform);

  gtk_window_set_child (GTK_WINDOW (window), fixed2);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
