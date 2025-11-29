/* Image Scaling
 * #Keywords: zoom, scale, filter, action, menu
 *
 * The custom widget we create here is similar to a GtkPicture,
 * but allows setting a zoom level and filtering mode for the
 * displayed paintable.
 *
 * It also demonstrates how to add a context menu to a custom
 * widget and connect it with widget actions.
 *
 * The context menu has items to change the zoom level.
 */

#include <gtk/gtk.h>
#include "demo3widget.h"

static void
file_opened (GObject      *source,
             GAsyncResult *result,
             void         *data)
{
  GFile *file;
  GError *error = NULL;
  GdkTexture *texture;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (!file)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  texture = gdk_texture_new_from_file (file, &error);
  g_object_unref (file);
  if (!texture)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  g_object_set (G_OBJECT (data), "texture", texture, NULL);
  g_object_unref (texture);
}

static void
open_file (GtkWidget *picker,
           GtkWidget *demo)
{
  GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (picker));
  GtkFileDialog *dialog;
  GtkFileFilter *filter;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Images");
  gtk_file_filter_add_pixbuf_formats (filter);
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog, parent, NULL, file_opened, demo);

  g_object_unref (dialog);
}

static void
rotate (GtkWidget *button,
        GtkWidget *demo)
{
  float angle;

  g_object_get (demo, "angle", &angle, NULL);

  angle = fmodf (angle + 90.f, 360.f);

  g_object_set (demo, "angle", angle, NULL);
}

static gboolean
transform_to (GBinding     *binding,
              const GValue *src,
              GValue       *dest,
              gpointer      user_data)
{
  double from;
  float to;

  from = g_value_get_double (src);
  to = (float) pow (2., from);
  g_value_set_float (dest, to);

  return TRUE;
}

static gboolean
transform_from (GBinding     *binding,
                const GValue *src,
                GValue       *dest,
                gpointer      user_data)
{
  float to;
  double from;

  to = g_value_get_float (src);
  from = log2 (to);
  g_value_set_double (dest, from);

  return TRUE;
}

GtkWidget *
do_image_scaling (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box;
      GtkWidget *box2;
      GtkWidget *sw;
      GtkWidget *widget;
      GtkWidget *scale;
      GtkWidget *dropdown;
      GtkWidget *button;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Image Scaling");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      sw = gtk_scrolled_window_new ();
      gtk_widget_set_vexpand (sw, TRUE);
      gtk_box_append (GTK_BOX (box), sw);

      widget = demo3_widget_new ("/transparent/portland-rose.jpg");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), widget);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box2);

      button = gtk_button_new_from_icon_name ("document-open-symbolic");
      gtk_widget_set_tooltip_text (button, "Open File");
      g_signal_connect (button, "clicked", G_CALLBACK (open_file), widget);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new_from_icon_name ("object-rotate-right-symbolic");
      gtk_widget_set_tooltip_text (button, "Rotate");
      g_signal_connect (button, "clicked", G_CALLBACK (rotate), widget);
      gtk_box_append (GTK_BOX (box2), button);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -10., 10., 0.1);
      gtk_scale_add_mark (GTK_SCALE (scale), 0., GTK_POS_TOP, NULL);
      gtk_widget_set_tooltip_text (scale, "Zoom");
      gtk_accessible_update_property (GTK_ACCESSIBLE (scale),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Zoom",
                                      -1);
      gtk_range_set_value (GTK_RANGE (scale), 0.);
      gtk_widget_set_hexpand (scale, TRUE);
      gtk_box_append (GTK_BOX (box2), scale);

      dropdown = gtk_drop_down_new (G_LIST_MODEL (gtk_string_list_new ((const char *[]){ "Linear", "Nearest", "Trilinear", NULL })), NULL);
      gtk_widget_set_tooltip_text (dropdown, "Filter");
      gtk_accessible_update_property (GTK_ACCESSIBLE (dropdown),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Filter",
                                      -1);
      gtk_box_append (GTK_BOX (box2), dropdown);

      g_object_bind_property (dropdown, "selected", widget, "filter", G_BINDING_DEFAULT);

      g_object_bind_property_full (gtk_range_get_adjustment (GTK_RANGE (scale)), "value",
                                   widget, "scale",
                                   G_BINDING_BIDIRECTIONAL,
                                   transform_to,
                                   transform_from,
                                   NULL, NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
