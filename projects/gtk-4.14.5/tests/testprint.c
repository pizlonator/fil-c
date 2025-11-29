/* GTK - The GIMP Toolkit
 * testprint.c: Print example
 * Copyright (C) 2006, Red Hat, Inc.
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
#include <math.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>
#include "testprintfileoperation.h"

static void
request_page_setup (GtkPrintOperation *operation,
		    GtkPrintContext *context,
		    int page_nr,
		    GtkPageSetup *setup)
{
  /* Make the second page landscape mode a5 */
  if (page_nr == 1)
    {
      GtkPaperSize *a5_size = gtk_paper_size_new ("iso_a5");
      
      gtk_page_setup_set_orientation (setup, GTK_PAGE_ORIENTATION_LANDSCAPE);
      gtk_page_setup_set_paper_size (setup, a5_size);
      gtk_paper_size_free (a5_size);
    }
}

static void
draw_page (GtkPrintOperation *operation,
	   GtkPrintContext *context,
	   int page_nr)
{
  cairo_t *cr;
  PangoLayout *layout;
  PangoFontDescription *desc;
  
  cr = gtk_print_context_get_cairo_context (context);

  /* Draw a red rectangle, as wide as the paper (inside the margins) */
  cairo_set_source_rgb (cr, 1.0, 0, 0);
  cairo_rectangle (cr, 0, 0, gtk_print_context_get_width (context), 50);
  
  cairo_fill (cr);

  /* Draw some lines */
  cairo_move_to (cr, 20, 10);
  cairo_line_to (cr, 40, 20);
  cairo_arc (cr, 60, 60, 20, 0, G_PI);
  cairo_line_to (cr, 80, 20);
  
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_set_line_width (cr, 5);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  
  cairo_stroke (cr);

  /* Draw some text */
  
  layout = gtk_print_context_create_pango_layout (context);
  pango_layout_set_text (layout, "Hello World! Printing is easy", -1);
  desc = pango_font_description_from_string ("sans 28");
  pango_layout_set_font_description (layout, desc);
  pango_font_description_free (desc);

  cairo_move_to (cr, 30, 20);
  pango_cairo_layout_path (cr, layout);

  /* Font Outline */
  cairo_set_source_rgb (cr, 0.93, 1.0, 0.47);
  cairo_set_line_width (cr, 0.5);
  cairo_stroke_preserve (cr);

  /* Font Fill */
  cairo_set_source_rgb (cr, 0, 0.0, 1.0);
  cairo_fill (cr);
  
  g_object_unref (layout);
}

int
main (int argc, char **argv)
{
  GtkPrintOperation *print;
  TestPrintFileOperation *print_file;

  gtk_init ();

  /* Test some random drawing, with per-page paper settings */
  print = gtk_print_operation_new ();
  gtk_print_operation_set_n_pages (print, 2);
  gtk_print_operation_set_unit (print, GTK_UNIT_MM);
  gtk_print_operation_set_export_filename (print, "test.pdf");
  g_signal_connect (print, "draw_page", G_CALLBACK (draw_page), NULL);
  g_signal_connect (print, "request_page_setup", G_CALLBACK (request_page_setup), NULL);
  gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_EXPORT, NULL, NULL);

  /* Test subclassing of GtkPrintOperation */
  print_file = test_print_file_operation_new ("testprint.c");
  test_print_file_operation_set_font_size (print_file, 12.0);
  gtk_print_operation_set_export_filename (GTK_PRINT_OPERATION (print_file), "test2.pdf");
  gtk_print_operation_run (GTK_PRINT_OPERATION (print_file), GTK_PRINT_OPERATION_ACTION_EXPORT, NULL, NULL);
  
  return 0;
}
