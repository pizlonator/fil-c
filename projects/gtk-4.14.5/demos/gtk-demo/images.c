/* Images
 * #Keywords: GdkPaintable, GtkWidgetPaintable
 *
 * GtkImage and GtkPicture are used to display an image; the image can be
 * in a number of formats.
 *
 * GtkImage is the widget used to display icons or images that should be
 * sized and styled like an icon, while GtkPicture is used for images
 * that should be displayed as-is.
 *
 * This demo code shows some of the more obscure cases, in the simple
 * case a call to gtk_picture_new_for_file() or
 * gtk_image_new_from_icon_name() is all you need.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <errno.h>
#include "pixbufpaintable.h"

static GtkWidget *window = NULL;
static GdkPixbufLoader *pixbuf_loader = NULL;
static guint load_timeout = 0;
static GInputStream * image_stream = NULL;

static void
progressive_prepared_callback (GdkPixbufLoader *loader,
                               gpointer         data)
{
  GdkPixbuf *pixbuf;
  GtkWidget *picture;
  GdkTexture *texture;

  picture = GTK_WIDGET (data);

  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  /* Avoid displaying random memory contents, since the pixbuf
   * isn't filled in yet.
   */
  gdk_pixbuf_fill (pixbuf, 0xaaaaaaff);

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (texture));
  g_object_unref (texture);
}

static void
progressive_updated_callback (GdkPixbufLoader *loader,
                              int                  x,
                              int                  y,
                              int                  width,
                              int                  height,
                              gpointer     data)
{
  GtkWidget *picture = GTK_WIDGET (data);
  GdkTexture *texture;

  texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_loader_get_pixbuf (loader));
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (texture));
  g_object_unref (texture);
}

static int
progressive_timeout (gpointer data)
{
  GtkWidget *picture = GTK_WIDGET (data);

  /* This shows off fully-paranoid error handling, so looks scary.
   * You could factor out the error handling code into a nice separate
   * function to make things nicer.
   */

  if (image_stream)
    {
      gssize bytes_read;
      guchar buf[256];
      GError *error = NULL;

      bytes_read = g_input_stream_read (image_stream, buf, 256, NULL, &error);

      if (bytes_read < 0)
        {
          GtkAlertDialog *dialog;

          dialog = gtk_alert_dialog_new ("Failure reading image file 'alphatest.png': %s",
                                         error->message);
          gtk_alert_dialog_show (dialog, NULL);
          g_object_unref (dialog);
          g_error_free (error);

          g_object_unref (image_stream);
          image_stream = NULL;

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (!gdk_pixbuf_loader_write (pixbuf_loader,
                                    buf, bytes_read,
                                    &error))
        {
          GtkAlertDialog *dialog;

          dialog = gtk_alert_dialog_new ("Failed to load image: %s",
                                         error->message);
          gtk_alert_dialog_show (dialog, NULL);
          g_object_unref (dialog);
          g_error_free (error);

          g_object_unref (image_stream);
          image_stream = NULL;

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (bytes_read == 0)
        {
          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!g_input_stream_close (image_stream, NULL, &error))
            {
              GtkAlertDialog *dialog;

              dialog = gtk_alert_dialog_new ("Failed to load image: %s",
                                             error->message);
              gtk_alert_dialog_show (dialog, NULL);
              g_object_unref (dialog);
              g_error_free (error);

              g_object_unref (image_stream);
              image_stream = NULL;
              g_object_unref (pixbuf_loader);
              pixbuf_loader = NULL;

              load_timeout = 0;

              return FALSE; /* uninstall the timeout */
            }

          g_object_unref (image_stream);
          image_stream = NULL;

          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!gdk_pixbuf_loader_close (pixbuf_loader, &error))
            {
              GtkAlertDialog *dialog;

              dialog = gtk_alert_dialog_new ("Failed to load image: %s",
                                             error->message);
              gtk_alert_dialog_show (dialog, NULL);
              g_object_unref (dialog);
              g_error_free (error);

              g_object_unref (pixbuf_loader);
              pixbuf_loader = NULL;

              load_timeout = 0;

              return FALSE; /* uninstall the timeout */
            }

          g_object_unref (pixbuf_loader);
          pixbuf_loader = NULL;
        }
    }
  else
    {
      GError *error = NULL;

      image_stream = g_resources_open_stream ("/images/alphatest.png", 0, &error);

      if (image_stream == NULL)
        {
          GtkAlertDialog *dialog;

          dialog = gtk_alert_dialog_new ("%s",
                                         error->message);
          gtk_alert_dialog_show (dialog, NULL);
          g_object_unref (dialog);
          g_error_free (error);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (pixbuf_loader)
        {
          gdk_pixbuf_loader_close (pixbuf_loader, NULL);
          g_object_unref (pixbuf_loader);
        }

      pixbuf_loader = gdk_pixbuf_loader_new ();

      g_signal_connect_object (pixbuf_loader, "area-prepared",
                               G_CALLBACK (progressive_prepared_callback), picture, 0);

      g_signal_connect_object (pixbuf_loader, "area-updated",
                               G_CALLBACK (progressive_updated_callback), picture, 0);
    }

  /* leave timeout installed */
  return TRUE;
}

static void
start_progressive_loading (GtkWidget *picture)
{
  /* This is obviously totally contrived (we slow down loading
   * on purpose to show how incremental loading works).
   * The real purpose of incremental loading is the case where
   * you are reading data from a slow source such as the network.
   * The timeout simply simulates a slow data source by inserting
   * pauses in the reading process.
   */
  load_timeout = g_timeout_add (300, progressive_timeout, picture);
  g_source_set_name_by_id (load_timeout, "[gtk] progressive_timeout");
}

static void
cleanup_callback (gpointer  data,
                  GObject  *former_object)
{
  *(gpointer**)data = NULL;

  if (load_timeout)
    {
      g_source_remove (load_timeout);
      load_timeout = 0;
    }

  if (pixbuf_loader)
    {
      gdk_pixbuf_loader_close (pixbuf_loader, NULL);
      g_object_unref (pixbuf_loader);
      pixbuf_loader = NULL;
    }

  if (image_stream)
    {
      g_object_unref (image_stream);
      image_stream = NULL;
    }
}

static void
toggle_sensitivity_callback (GtkWidget *togglebutton,
                             gpointer   user_data)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (user_data));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      /* don't disable our toggle */
      if (child != togglebutton)
        gtk_widget_set_sensitive (child, !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (togglebutton)));
    }
}


GtkWidget *
do_images (GtkWidget *do_widget)
{
  GtkWidget *video;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *base_vbox;
  GtkWidget *image;
  GtkWidget *picture;
  GtkWidget *label;
  GtkWidget *button;
  GdkPaintable *paintable;
  GIcon *gicon;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Images");
      g_object_weak_ref (G_OBJECT (window), cleanup_callback, &window);

      base_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (base_vbox, 16);
      gtk_widget_set_margin_end (base_vbox, 16);
      gtk_widget_set_margin_top (base_vbox, 16);
      gtk_widget_set_margin_bottom (base_vbox, 16);
      gtk_window_set_child (GTK_WINDOW (window), base_vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
      gtk_box_append (GTK_BOX (base_vbox), hbox);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Image from a resource");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      image = gtk_image_new_from_resource ("/images/org.gtk.Demo4.svg");
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

      gtk_frame_set_child (GTK_FRAME (frame), image);


      /* Animation */

      label = gtk_label_new ("Animation from a resource");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      paintable = pixbuf_paintable_new_from_resource ("/images/floppybuddy.gif");
      picture = gtk_picture_new_for_paintable (paintable);
      g_object_unref (paintable);

      gtk_frame_set_child (GTK_FRAME (frame), picture);

      /* Symbolic icon */

      label = gtk_label_new ("Symbolic themed icon");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      gicon = g_themed_icon_new_with_default_fallbacks ("battery-caution-charging-symbolic");
      image = gtk_image_new_from_gicon (gicon);
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
      g_object_unref (gicon);

      gtk_frame_set_child (GTK_FRAME (frame), image);


      /* Progressive */
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Progressive image loading");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      /* Create an empty image for now; the progressive loader
       * will create the pixbuf and fill it in.
       */
      picture = gtk_picture_new ();
      gtk_picture_set_alternative_text (GTK_PICTURE (picture), "A slowly loading image");
      gtk_frame_set_child (GTK_FRAME (frame), picture);

      start_progressive_loading (picture);

      /* Video */
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Displaying video");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      video = gtk_video_new_for_resource ("/images/gtk-logo.webm");
      gtk_media_stream_set_loop (gtk_video_get_media_stream (GTK_VIDEO (video)), TRUE);
      gtk_frame_set_child (GTK_FRAME (frame), video);

      /* Widget paintables */
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("GtkWidgetPaintable");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      paintable = gtk_widget_paintable_new (do_widget);
      picture = gtk_picture_new_for_paintable (paintable);
      gtk_widget_set_size_request (picture, 100, 100);
      gtk_widget_set_valign (picture, GTK_ALIGN_START);
      gtk_box_append (GTK_BOX (vbox), picture);

      /* Sensitivity control */
      button = gtk_toggle_button_new_with_mnemonic ("_Insensitive");
      gtk_box_append (GTK_BOX (base_vbox), button);

      g_signal_connect (button, "toggled",
                        G_CALLBACK (toggle_sensitivity_callback),
                        base_vbox);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
