/* Paintable/SVG
 *
 * This demo shows wrapping a librsvg RsvgHandle in a GdkPaintable
 * to display an SVG image that can be scaled by resizing the window.
 *
 * This demo relies on librsvg, which GTK itself does not link against.
 */

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

#include "svgpaintable.h"


static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkPicture *picture = data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      GdkPaintable *paintable;

      paintable = svg_paintable_new (file);
      gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);
      g_object_unref (paintable);
      g_object_unref (file);
    }
}

static void
show_file_open (GtkWidget  *button,
                GtkPicture *picture)
{
  GtkFileFilter *filter;
  GtkFileDialog *dialog;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open node file");

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/svg+xml");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (button)),
                        NULL,
                        open_response_cb, picture);
}

static GtkWidget *window;

GtkWidget *
do_paintable_svg (GtkWidget *do_widget)
{
  GtkWidget *header;
  GtkWidget *picture;
  GtkWidget *button;
  GFile *file;
  GdkPaintable *paintable;

  if (!window)
    {
      window = gtk_window_new ();
      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_title (GTK_WINDOW (window), "Paintable — SVG");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      button = gtk_button_new_with_mnemonic ("_Open");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      picture = gtk_picture_new ();
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
      gtk_widget_set_size_request (picture, 16, 16);

      g_signal_connect (button, "clicked", G_CALLBACK (show_file_open), picture);

      gtk_window_set_child (GTK_WINDOW (window), picture);

      file = g_file_new_for_uri ("resource:///paintable_svg/org.gtk.gtk4.NodeEditor.Devel.svg");
      paintable = svg_paintable_new (file);
      gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);
      g_object_unref (paintable);
      g_object_unref (file);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
