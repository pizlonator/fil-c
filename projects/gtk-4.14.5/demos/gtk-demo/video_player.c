/* Video Player
 * #Keywords: GtkVideo, GtkMediaStream, GtkMediaFile, GdkPaintable
 * #Keywords: GtkMediaControls
 *
 * This is a simple video player using just GTK widgets.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
open_dialog_response_cb (GObject *source,
                         GAsyncResult *result,
                         void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkWidget *video = user_data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      gtk_video_set_file (GTK_VIDEO (video), file);
      g_object_unref (file);
    }
}

static void
open_clicked_cb (GtkWidget *button,
                 GtkWidget *video)
{
  GtkFileDialog *dialog;
  GtkFileFilter *filter;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Select a video");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_filter_set_name (filter, "All Files");
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/*");
  gtk_file_filter_set_name (filter, "Images");
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "video/*");
  gtk_file_filter_set_name (filter, "Video");
  g_list_store_append (filters, filter);

  gtk_file_dialog_set_default_filter (dialog, filter);
  g_object_unref (filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (button)),
                        NULL,
                        open_dialog_response_cb, video);
}

static void
logo_clicked_cb (GtkWidget *button,
                 gpointer   video)
{
  GFile *file;

  file = g_file_new_for_uri ("resource:///images/gtk-logo.webm");
  gtk_video_set_file (GTK_VIDEO (video), file);
  g_object_unref (file);
}

static void
bbb_clicked_cb (GtkWidget *button,
                gpointer   video)
{
  GFile *file;

  file = g_file_new_for_uri ("https://download.blender.org/peach/trailer/trailer_400p.ogg");
  gtk_video_set_file (GTK_VIDEO (video), file);
  g_object_unref (file);
}

static void
fullscreen_clicked_cb (GtkWidget *button,
                       gpointer   unused)
{
  GtkWidget *widget_window = GTK_WIDGET (gtk_widget_get_root (button));

  gtk_window_fullscreen (GTK_WINDOW (widget_window));
}

static gboolean
toggle_fullscreen (GtkWidget *widget,
                   GVariant  *args,
                   gpointer   data)
{
  GdkSurface *surface;
  GdkToplevelState state;

  surface = gtk_native_get_surface (GTK_NATIVE (widget));
  state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));

  if (state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    gtk_window_unfullscreen (GTK_WINDOW (widget));
  else
    gtk_window_fullscreen (GTK_WINDOW (widget));

  return TRUE;
}

GtkWidget *
do_video_player (GtkWidget *do_widget)
{
  GtkWidget *title;
  GtkWidget *video;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *fullscreen_button;
  GtkEventController *controller;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Video Player");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      video = gtk_video_new ();
      gtk_video_set_autoplay (GTK_VIDEO (video), TRUE);
      gtk_video_set_graphics_offload (GTK_VIDEO (video), GTK_GRAPHICS_OFFLOAD_ENABLED);
      gtk_window_set_child (GTK_WINDOW (window), video);

      title = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), title);

      button = gtk_button_new_with_mnemonic ("_Open");
      g_signal_connect (button, "clicked", G_CALLBACK (open_clicked_cb), video);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      button = gtk_button_new ();
      image = gtk_image_new_from_resource ("/cursors/images/gtk_logo_cursor.png");
      gtk_accessible_update_relation (GTK_ACCESSIBLE (image),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, button, NULL,
                                      -1);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 24);
      gtk_button_set_child (GTK_BUTTON (button), image);
      g_signal_connect (button, "clicked", G_CALLBACK (logo_clicked_cb), video);
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "GTK Logo",
                                      -1);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      button = gtk_button_new ();
      image = gtk_image_new_from_resource ("/video-player/bbb.png");
      gtk_accessible_update_relation (GTK_ACCESSIBLE (image),
                                      GTK_ACCESSIBLE_RELATION_LABELLED_BY, button, NULL,
                                      -1);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 24);
      gtk_button_set_child (GTK_BUTTON (button), image);
      g_signal_connect (button, "clicked", G_CALLBACK (bbb_clicked_cb), video);
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Big Buck Bunny",
                                      -1);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      fullscreen_button = gtk_button_new_from_icon_name ("view-fullscreen-symbolic");
      g_signal_connect (fullscreen_button, "clicked", G_CALLBACK (fullscreen_clicked_cb), NULL);
      gtk_accessible_update_property (GTK_ACCESSIBLE (fullscreen_button),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Fullscreen",
                                      -1);

      gtk_header_bar_pack_end (GTK_HEADER_BAR (title), fullscreen_button);

      controller = gtk_shortcut_controller_new ();
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller),
                                         GTK_SHORTCUT_SCOPE_GLOBAL);
      gtk_widget_add_controller (window, controller);
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F11, 0),
                             gtk_callback_action_new (toggle_fullscreen, NULL, NULL)));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
