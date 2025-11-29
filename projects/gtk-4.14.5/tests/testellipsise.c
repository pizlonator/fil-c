/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
combo_changed_cb (GtkWidget *combo,
		  gpointer   data)
{
  GtkWidget *label = GTK_WIDGET (data);
  int active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  gtk_label_set_ellipsize (GTK_LABEL (label), (PangoEllipsizeMode)active);
}

static void
overlay_draw (GtkDrawingArea *da,
              cairo_t        *cr,
              int             width,
              int             height,
              gpointer        data)
{
  GtkWidget *widget = GTK_WIDGET (da);
  PangoLayout *layout;
  const double dashes[] = { 6, 18 };
  GtkAllocation label_allocation;
  GtkRequisition minimum_size, natural_size;
  GtkWidget *label = data;
  double x, y;

  cairo_translate (cr, -0.5, -0.5);
  cairo_set_line_width (cr, 1);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  gtk_widget_translate_coordinates (label, widget, 0, 0, &x, &y);
  layout = gtk_widget_create_pango_layout (widget, "");

  gtk_widget_get_preferred_size (label, &minimum_size, &natural_size); 

  pango_layout_set_markup (layout,
    "<span color='#c33'>\342\227\217 requisition</span>\n"
    "<span color='#3c3'>\342\227\217 natural size</span>\n"
    "<span color='#33c'>\342\227\217 allocation</span>", -1);

  pango_cairo_show_layout (cr, layout);
  g_object_unref (layout);

  gtk_widget_get_allocation (label, &label_allocation);

  cairo_rectangle (cr,
                   x + 0.5 * (label_allocation.width - minimum_size.width),
                   y + 0.5 * (label_allocation.height - minimum_size.height),
                   minimum_size.width, minimum_size.height);
  cairo_set_source_rgb (cr, 0.8, 0.2, 0.2);
  cairo_set_dash (cr, NULL, 0, 0);
  cairo_stroke (cr);

  cairo_rectangle (cr, x, y, label_allocation.width, label_allocation.height);
  cairo_set_source_rgb (cr, 0.2, 0.2, 0.8);
  cairo_set_dash (cr, dashes, 2, 0.5);
  cairo_stroke (cr);

  cairo_rectangle (cr,
                   x + 0.5 * (label_allocation.width - natural_size.width),
                   y + 0.5 * (label_allocation.height - natural_size.height),
                   natural_size.width, natural_size.height);
  cairo_set_source_rgb (cr, 0.2, 0.8, 0.2);
  cairo_set_dash (cr, dashes, 2, 12.5);
  cairo_stroke (cr);
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
  GtkWidget *window, *vbox, *label;
  GtkWidget *combo, *scale, *overlay, *da;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  combo = gtk_combo_box_text_new ();
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                    0, 360, 1);
  label = gtk_label_new ("This label may be ellipsized\nto make it fit.");

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "NONE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "START");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "MIDDLE");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "END");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), overlay_draw, label, NULL);

  overlay = gtk_overlay_new ();
  gtk_overlay_set_child (GTK_OVERLAY (overlay), da);
  gtk_widget_set_vexpand (overlay, TRUE);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);

  gtk_box_append (GTK_BOX (vbox), combo);
  gtk_box_append (GTK_BOX (vbox), scale);
  gtk_box_append (GTK_BOX (vbox), overlay);

  g_object_set_data (G_OBJECT (label), "combo", combo);

  g_signal_connect (combo, "changed", G_CALLBACK (combo_changed_cb), label);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
