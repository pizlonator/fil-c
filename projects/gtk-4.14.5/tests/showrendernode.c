#include <gtk/gtk.h>

static char *write_to_filename = NULL;
static gboolean compare_node;

static GOptionEntry options[] = {
  { "write", 'o', 0, G_OPTION_ARG_STRING, &write_to_filename, "Write PNG file", NULL },
  { "compare", 'c', 0, G_OPTION_ARG_NONE, &compare_node, "Compare render to render_texture", NULL },
  { NULL }
};



typedef struct _GtkNodeView      GtkNodeView;
typedef struct _GtkNodeViewClass GtkNodeViewClass;

#define GTK_TYPE_NODE_VIEW           (gtk_node_view_get_type ())
#define GTK_NODE_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_NODE_VIEW, GtkNodeView))
#define GTK_NODE_VIEW_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_NODE_VIEW, GtkNodeViewClass))
struct _GtkNodeView
{
  GtkWidget parent_instance;

  GskRenderNode *node;
  GFileMonitor *file_monitor;
};

struct _GtkNodeViewClass
{
  GtkWidgetClass parent_class;
};

GType gtk_node_view_get_type (void) G_GNUC_CONST;


G_DEFINE_TYPE(GtkNodeView, gtk_node_view, GTK_TYPE_WIDGET)

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_warning ("Error at %s: %s", string->str, error->message);

  g_string_free (string, TRUE);
}

static void
load_file_contents (GtkNodeView *self,
                    GFile       *file)
{
  GBytes *bytes;
  GError *error = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes == NULL)
    return;

  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      g_bytes_unref (bytes);
      return;
    }

  self->node = gsk_render_node_deserialize (bytes, deserialize_error_func, &error);

  if (error)
    {
      g_critical ("Invalid node file: %s", error->message);
      g_clear_error (&error);
      return;
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_bytes_unref (bytes);
}

static void
file_changed_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  GtkNodeView *self = user_data;

  if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
    load_file_contents (self, file);
}

static void
gtk_node_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkNodeView *self = GTK_NODE_VIEW (widget);
  graphene_rect_t bounds;


  if (self->node == NULL)
    return;

  gsk_render_node_get_bounds (self->node, &bounds);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = *natural = bounds.origin.x + bounds.size.width;
    }
  else /* VERTICAL */
    {
      *minimum = *natural = bounds.origin.y + bounds.size.height;
    }
}

static void
gtk_node_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkNodeView *self = GTK_NODE_VIEW (widget);

  if (self->node != NULL)
    gtk_snapshot_append_node (snapshot, self->node);
}

static void
gtk_node_view_finalize (GObject *object)
{
  GtkNodeView *self = GTK_NODE_VIEW (object);

  if (self->node)
    gsk_render_node_unref (self->node);

  G_OBJECT_CLASS (gtk_node_view_parent_class)->finalize (object);
}

static void
gtk_node_view_init (GtkNodeView *self)
{
  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

static void
gtk_node_view_class_init (GtkNodeViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_node_view_finalize;

  widget_class->measure = gtk_node_view_measure;
  widget_class->snapshot = gtk_node_view_snapshot;
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
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *nodeview;
  graphene_rect_t node_bounds;
  GOptionContext *option_context;
  GError *error = NULL;
  gboolean done = FALSE;
  GFile *file;

  option_context = g_option_context_new ("NODE-FILE [-o OUTPUT] [--compare]");
  g_option_context_add_main_entries (option_context, options, NULL);

  if (argc < 2)
    {
      printf ("Usage: showrendernode NODEFILE [-o OUTPUT] [--compare]\n");
      return 0;
    }

  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  g_option_context_free (option_context);
  option_context = NULL;

  g_message ("Compare: %d, write to filename: %s", compare_node, write_to_filename);

  gtk_init ();

  window = gtk_window_new ();
  nodeview = g_object_new (GTK_TYPE_NODE_VIEW, NULL);

  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

  file = g_file_new_for_path (argv[1]);
  load_file_contents (GTK_NODE_VIEW (nodeview), file);
  GTK_NODE_VIEW (nodeview)->file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  g_object_unref (file);

  if (error)
    {
      g_warning ("%s", error->message);
      return -1;
    }

  g_signal_connect (GTK_NODE_VIEW (nodeview)->file_monitor,
                    "changed", G_CALLBACK (file_changed_cb), nodeview);

  if (write_to_filename != NULL)
    {
      GdkSurface *surface = gdk_surface_new_toplevel (gdk_display_get_default());
      GskRenderer *renderer = gsk_renderer_new_for_surface (surface);
      GdkTexture *texture = gsk_renderer_render_texture (renderer, GTK_NODE_VIEW (nodeview)->node, NULL);

      g_message ("Writing .node file to .png using %s", G_OBJECT_TYPE_NAME (renderer));

      g_assert (texture != NULL);

      gdk_texture_save_to_png (texture, write_to_filename);

      gsk_renderer_unrealize (renderer);

      g_object_unref (texture);
      g_object_unref (renderer);
      g_object_unref (surface);
    }

  if (compare_node)
    {
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      GdkSurface *gdk_surface = gdk_surface_new_toplevel (gdk_display_get_default());
      GskRenderer *renderer = gsk_renderer_new_for_surface (gdk_surface);
      GdkTexture *texture = gsk_renderer_render_texture (renderer, GTK_NODE_VIEW (nodeview)->node, NULL);
      GtkWidget *image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));

      gtk_widget_set_size_request (image,
                                   gdk_texture_get_width (texture),
                                   gdk_texture_get_height (texture));

      gtk_box_append (GTK_BOX (box), nodeview);
      gtk_box_append (GTK_BOX (box), image);
      gtk_window_set_child (GTK_WINDOW (window), box);

      gsk_renderer_unrealize (renderer);
      g_object_unref (texture);
      g_object_unref (renderer);
      g_object_unref (gdk_surface);
    }
  else
    {
      gtk_window_set_child (GTK_WINDOW (window), nodeview);
    }

  gsk_render_node_get_bounds (GTK_NODE_VIEW (nodeview)->node, &node_bounds);
  gtk_window_set_default_size (GTK_WINDOW (window),
                               MAX (600, node_bounds.size.width),
                               MAX (500, node_bounds.size.height));

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
