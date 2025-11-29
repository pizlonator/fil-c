#include "gtk/gtk.h"
#include "gtkclipperprivate.h"

struct _GtkClipper
{
  GObject parent_instance;

  GdkPaintable *paintable;
  graphene_rect_t clip;
};

struct _GtkClipperClass
{
  GObjectClass parent_class;
};

static void
gtk_clipper_paintable_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  GtkClipper *self = GTK_CLIPPER (paintable);
  float sx, sy;

  gtk_snapshot_save (snapshot);

  sx = gdk_paintable_get_intrinsic_width (self->paintable) / self->clip.size.width;
  sy = gdk_paintable_get_intrinsic_height (self->paintable) / self->clip.size.height;

  gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- self->clip.origin.x * width / self->clip.size.width,
                                                          - self->clip.origin.y * height / self->clip.size.height));
  gdk_paintable_snapshot (self->paintable, snapshot, width * sx, height * sy);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

static GdkPaintable *
gtk_clipper_paintable_get_current_image (GdkPaintable *paintable)
{
  GtkClipper *self = GTK_CLIPPER (paintable);
  GdkPaintable *current_paintable, *current_self;

  current_paintable = gdk_paintable_get_current_image (self->paintable);
  current_self = gtk_clipper_new (current_paintable, &self->clip);
  g_object_unref (current_paintable);

  return current_self;
}

static GdkPaintableFlags
gtk_clipper_paintable_get_flags (GdkPaintable *paintable)
{
  GtkClipper *self = GTK_CLIPPER (paintable);

  return gdk_paintable_get_flags (self->paintable);
}

static int
gtk_clipper_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkClipper *self = GTK_CLIPPER (paintable);

  return self->clip.size.width;
}

static int
gtk_clipper_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkClipper *self = GTK_CLIPPER (paintable);

  return self->clip.size.height;
}

static double gtk_clipper_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkClipper *self = GTK_CLIPPER (paintable);

  return self->clip.size.width / (double) self->clip.size.height;
};

static void
gtk_clipper_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_clipper_paintable_snapshot;
  iface->get_current_image = gtk_clipper_paintable_get_current_image;
  iface->get_flags = gtk_clipper_paintable_get_flags;
  iface->get_intrinsic_width = gtk_clipper_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_clipper_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_clipper_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (GtkClipper, gtk_clipper, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gtk_clipper_paintable_init))

static void
gtk_clipper_dispose (GObject *object)
{
  GtkClipper *self = GTK_CLIPPER (object);

  if (self->paintable)
    {
      const guint flags = gdk_paintable_get_flags (self->paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);

      g_clear_object (&self->paintable);
    }

  G_OBJECT_CLASS (gtk_clipper_parent_class)->dispose (object);
}

static void
gtk_clipper_class_init (GtkClipperClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_clipper_dispose;
}

static void
gtk_clipper_init (GtkClipper *self)
{
}

GdkPaintable *
gtk_clipper_new (GdkPaintable          *paintable,
                 const graphene_rect_t *clip)
{
  GtkClipper *self;
  guint flags;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  g_return_val_if_fail (clip != NULL, NULL);

  self = g_object_new (GTK_TYPE_CLIPPER, NULL);

  self->paintable = g_object_ref (paintable);
  flags = gdk_paintable_get_flags (paintable);

  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_connect_swapped (paintable, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_connect_swapped (paintable, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);

  self->clip = *clip;

  return GDK_PAINTABLE (self);
}
