#include <gtk/gtk.h>



typedef struct _GtkTextureView      GtkTextureView;
typedef struct _GtkTextureViewClass GtkTextureViewClass;

#define GTK_TYPE_TEXTURE_VIEW           (gtk_texture_view_get_type ())
#define GTK_TEXTURE_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_TYPE_TEXTURE_VIEW, GtkTextureView))
#define GTK_TEXTURE_VIEW_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, GTK_TYPE_TEXTURE_VIEW, GtkTextureViewClass))
struct _GtkTextureView
{
  GtkWidget parent_instance;

  GdkTexture *texture;
};

struct _GtkTextureViewClass
{
  GtkWidgetClass parent_class;
};

GType gtk_texture_view_get_type (void) G_GNUC_CONST;


G_DEFINE_TYPE(GtkTextureView, gtk_texture_view, GTK_TYPE_WIDGET)

static void
gtk_texture_view_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  GtkTextureView *self = GTK_TEXTURE_VIEW (widget);

  if (self->texture == NULL)
    return;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = 0;
      *natural = gdk_texture_get_width (self->texture);
    }
  else /* VERTICAL */
    {
      *minimum = 0;
      *natural = gdk_texture_get_height (self->texture);
    }
}

static void
gtk_texture_view_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  GtkTextureView *self = GTK_TEXTURE_VIEW (widget);
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);

  if (self->texture != NULL)
    {
      graphene_rect_t bounds;

      bounds.origin.x = MAX (0, width / 2 - gdk_texture_get_width (self->texture));
      bounds.origin.y = MAX (0, height / 2 - gdk_texture_get_height (self->texture));

      bounds.size.width = MIN (width, gdk_texture_get_width (self->texture));
      bounds.size.height = MIN (height, gdk_texture_get_height (self->texture));

      gtk_snapshot_append_texture (snapshot, self->texture, &bounds);
    }
}

static void
gtk_texture_view_finalize (GObject *object)
{
  GtkTextureView *self = GTK_TEXTURE_VIEW (object);

  g_clear_object (&self->texture);

  G_OBJECT_CLASS (gtk_texture_view_parent_class)->finalize (object);
}

static void
gtk_texture_view_init (GtkTextureView *self)
{
}

static void
gtk_texture_view_class_init (GtkTextureViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_texture_view_finalize;

  widget_class->measure = gtk_texture_view_measure;
  widget_class->snapshot = gtk_texture_view_snapshot;
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
  GtkWidget *view;
  GdkTexture *texture;
  GFile *file;
  GError *error = NULL;
  gboolean done = FALSE;

  gtk_init ();

  if (argc != 2)
    {
      g_error ("No texture file path given.");
      return -1;
    }

  file = g_file_new_for_path (argv[1]);
  texture = gdk_texture_new_from_file (file, &error);

  if (error != NULL)
    {
      g_error ("Error: %s", error->message);
      return -1;
    }

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  view = g_object_new (GTK_TYPE_TEXTURE_VIEW, NULL);
  ((GtkTextureView*)view)->texture = g_steal_pointer (&texture);

  gtk_window_set_child (GTK_WINDOW (window), view);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (file);

  return 0;
}
