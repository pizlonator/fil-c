#include <gtk/gtk.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined (G_OS_WIN32)
#include <io.h>
#endif

static GdkTexture *
render_paintable_to_texture (GdkPaintable *paintable)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;
  GBytes *bytes;

  width = gdk_paintable_get_intrinsic_width (paintable);
  if (width == 0)
    width = 32;

  height = gdk_paintable_get_intrinsic_height (paintable);
  if (height == 0)
    height = 32;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  gsk_render_node_unref (node);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);
  cairo_surface_destroy (surface);

  return texture;
}

static GdkTexture *
get_image_texture (GtkImage *image)
{
  GtkIconTheme *icon_theme;
  const char *icon_name;
  int width = 48;
  GdkPaintable *paintable = NULL;
  GdkTexture *texture = NULL;
  GtkIconPaintable *icon;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PAINTABLE:
      paintable = gtk_image_get_paintable (image);
      break;
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      icon = gtk_icon_theme_lookup_icon (icon_theme,
                                         icon_name,
                                         NULL,
                                         width, 1,
                                         gtk_widget_get_direction (GTK_WIDGET (image)),
                                         0);
      paintable = GDK_PAINTABLE (icon);
      break;
    case GTK_IMAGE_GICON:
    case GTK_IMAGE_EMPTY:
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
    }

  if (paintable)
    texture = render_paintable_to_texture (paintable);

  g_object_unref (paintable);

  return texture;
}

enum {
  TOP_LEFT,
  CENTER,
  BOTTOM_RIGHT
};

static void
got_texture (GObject *source,
             GAsyncResult *result,
             gpointer data)
{
  GdkDrop *drop = GDK_DROP (source);
  GtkWidget *image = data;
  const GValue *value;
  GError *error = NULL;

  value = gdk_drop_read_value_finish (drop, result, &error);
  if (value)
    {
      GdkTexture *texture = g_value_get_object (value);
      gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (texture));
      gdk_drop_finish (drop, GDK_ACTION_COPY);
    }
  else
    {
      g_error_free (error);
      gdk_drop_finish (drop, 0);
    }

  g_object_set_data (G_OBJECT (image), "drop", NULL);
}

static void
perform_drop (GdkDrop   *drop,
              GtkWidget *image)
{
  if (gdk_content_formats_contain_gtype (gdk_drop_get_formats (drop), GDK_TYPE_TEXTURE))
    gdk_drop_read_value_async (drop, GDK_TYPE_TEXTURE, G_PRIORITY_DEFAULT, NULL, got_texture, image);
  else
    {
      gdk_drop_finish (drop, 0);
      g_object_set_data (G_OBJECT (image), "drop", NULL);
    }
}

static void
do_copy (GtkWidget *button)
{
  GtkWidget *popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  GtkWidget *image = gtk_widget_get_parent (popover);
  GdkDrop *drop = GDK_DROP (g_object_get_data (G_OBJECT (image), "drop"));

  gtk_popover_popdown (GTK_POPOVER (popover));
  perform_drop (drop, image);
}

static void
do_cancel (GtkWidget *button)
{
  GtkWidget *popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  GtkWidget *image = gtk_widget_get_parent (popover);
  GdkDrop *drop = GDK_DROP (g_object_get_data (G_OBJECT (image), "drop"));

  gtk_popover_popdown (GTK_POPOVER (popover));
  gdk_drop_finish (drop, 0);

  g_object_set_data (G_OBJECT (image), "drop", NULL);
}

static void
ask_actions (GdkDrop *drop,
             GtkWidget *image)
{
  GtkWidget *popover, *box, *button;

  popover = g_object_get_data (G_OBJECT (image), "popover");
  if (!popover)
    {
      popover = gtk_popover_new ();
      gtk_widget_set_parent (popover, image);
      g_object_set_data (G_OBJECT (image), "popover", popover);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_popover_set_child (GTK_POPOVER (popover), box);
      button = gtk_button_new_with_label ("Copy");
      g_signal_connect (button, "clicked", G_CALLBACK (do_copy), NULL);
      gtk_box_append (GTK_BOX (box), button);
      button = gtk_button_new_with_label ("Move");
      g_signal_connect (button, "clicked", G_CALLBACK (do_copy), NULL);
      gtk_box_append (GTK_BOX (box), button);
      button = gtk_button_new_with_label ("Cancel");
      g_signal_connect (button, "clicked", G_CALLBACK (do_cancel), NULL);
      gtk_box_append (GTK_BOX (box), button);
    }
  gtk_popover_popup (GTK_POPOVER (popover));
}

static gboolean
delayed_deny (gpointer data)
{
  GtkDropTargetAsync *dest = data;
  GtkWidget *image = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  GdkDrop *drop = GDK_DROP (g_object_get_data (G_OBJECT (image), "drop"));

  if (drop)
    {
      g_print ("denying drop, late\n");
      gtk_drop_target_async_reject_drop (dest, drop);
    }

  return G_SOURCE_REMOVE;
}

static gboolean
image_drag_accept (GtkDropTargetAsync *dest,
                   GdkDrop            *drop,
                   gpointer            data)
{
  GtkWidget *image = data;
  g_object_set_data_full (G_OBJECT (image), "drop", g_object_ref (drop), g_object_unref);

  g_timeout_add (1000, delayed_deny, dest);

  return TRUE;
}

static gboolean
image_drag_drop (GtkDropTarget    *dest,
                 GdkDrop          *drop,
                 double            x,
                 double            y,
                 gpointer          data)
{
  GtkWidget *image = data;
  GdkDragAction action = gdk_drop_get_actions (drop);

  g_object_set_data_full (G_OBJECT (image), "drop", g_object_ref (drop), g_object_unref);

  g_print ("drop, actions %d\n", action);
  if (!gdk_drag_action_is_unique (action))
    ask_actions (drop, image);
  else
    perform_drop (drop, image);

  return TRUE;
}

static void
update_source_icon (GtkDragSource *source,
                    const char *icon_name,
                    int hotspot)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  GtkIconPaintable *icon;
  int hot_x, hot_y;
  int size = 48;

  if (widget == NULL)
    return;

  icon = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_for_display (gtk_widget_get_display (widget)),
                                     icon_name,
                                     NULL,
                                     size, 1,
                                     gtk_widget_get_direction (widget),
                                     0);
  switch (hotspot)
    {
    default:
    case TOP_LEFT:
      hot_x = 0;
      hot_y = 0;
      break;
    case CENTER:
      hot_x = size / 2;
      hot_y = size / 2;
      break;
    case BOTTOM_RIGHT:
      hot_x = size;
      hot_y = size;
      break;
    }
  gtk_drag_source_set_icon (source, GDK_PAINTABLE (icon), hot_x, hot_y);
  g_object_unref (icon);
}

static GdkContentProvider *
drag_prepare (GtkDragSource *source,
              double         x,
              double         y)
{
  GtkImage *image = GTK_IMAGE (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source)));
  GdkTexture *texture;
  GdkContentProvider *content;

  content = gdk_content_provider_new_typed (G_TYPE_STRING, gtk_image_get_icon_name (GTK_IMAGE (image)));

  texture = get_image_texture (image);
  if (texture)
    {
      content = gdk_content_provider_new_union ((GdkContentProvider *[2]) {
                                                  gdk_content_provider_new_typed (GDK_TYPE_TEXTURE, texture),
                                                  content,
                                                }, 2);
      g_object_unref (texture);
    }

  return content;
}

static void
drag_begin (GtkDragSource *source)
{
  g_print ("drag begin\n");
}

static void
drag_end (GtkDragSource *source)
{
  g_print ("drag end\n");
}

static gboolean
drag_cancel (GtkDragSource       *source,
             GdkDrag             *drag,
             GdkDragCancelReason  reason)
{
  g_print ("drag failed: %d\n", reason);
  return FALSE;
}

static GtkWidget *
make_image (const char *icon_name, int hotspot)
{
  GtkWidget *image;
  GtkDragSource *source;
  GtkDropTargetAsync *dest;
  GdkContentFormats *formats;
  GdkContentFormatsBuilder *builder;

  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_TEXTURE);
  gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_ASK);
  update_source_icon (source, icon_name, hotspot);

  g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare), NULL);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  g_signal_connect (source, "drag-cancel", G_CALLBACK (drag_cancel), NULL);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));

  dest = gtk_drop_target_async_new (formats, GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_ASK);
  g_signal_connect (dest, "accept", G_CALLBACK (image_drag_accept), image);
  g_signal_connect (dest, "drop", G_CALLBACK (image_drag_drop), image);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (dest));

  return image;
}

static void
spinner_drag_begin (GtkDragSource *source,
                    GdkDrag       *drag,
                    GtkWidget     *widget)
{
  GdkPaintable *paintable;

  paintable = gtk_widget_paintable_new (widget);
  gtk_drag_source_set_icon (source, paintable, 0, 0);
  g_object_unref (paintable);
}

static GtkWidget *
make_spinner (void)
{
  GtkWidget *spinner;
  GtkDragSource *source;
  GdkContentProvider *content;

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));

  content = gdk_content_provider_new_typed (G_TYPE_STRING, "ACTIVE");
  source = gtk_drag_source_new ();
  gtk_drag_source_set_content (source, content);
  g_signal_connect (source, "drag-begin", G_CALLBACK (spinner_drag_begin), spinner);
  gtk_widget_add_controller (spinner, GTK_EVENT_CONTROLLER (source));

  g_object_unref (content);

  return spinner;
}

int
main (int argc, char *Argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *entry;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Drag And Drop");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  grid = gtk_grid_new ();
  g_object_set (grid,
                "margin-start", 20,
                "margin-end", 20,
                "margin-top", 20,
                "margin-bottom", 20,
                "row-spacing", 20,
                "column-spacing", 20,
                NULL);
  gtk_window_set_child (GTK_WINDOW (window), grid);
  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-warning", TOP_LEFT), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), make_image ("process-stop", BOTTOM_RIGHT), 1, 0, 1, 1);

  entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), entry, 0, 1, 2, 1);

  gtk_grid_attach (GTK_GRID (grid), make_spinner (), 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), make_image ("weather-clear", CENTER), 1, 2, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-question", TOP_LEFT), 0, 3, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), make_image ("dialog-information", CENTER), 1, 3, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
