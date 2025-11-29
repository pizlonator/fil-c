#include <gtk/gtk.h>
#include "pixbufpaintable.h"

struct _PixbufPaintable {
  GObject parent_instance;

  char *resource_path;
  GdkPixbufAnimation *anim;
  GdkPixbufAnimationIter *iter;

  guint timeout;
};

enum {
  PROP_RESOURCE_PATH = 1,
  NUM_PROPERTIES
};

static void
pixbuf_paintable_snapshot (GdkPaintable *paintable,
                           GdkSnapshot  *snapshot,
                           double        width,
                           double        height)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (paintable);
  GTimeVal val;
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  g_get_current_time (&val);
  gdk_pixbuf_animation_iter_advance (self->iter, &val);
  pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (self->iter);
  texture = gdk_texture_new_for_pixbuf (pixbuf);

  gdk_paintable_snapshot (GDK_PAINTABLE (texture), snapshot, width, height);

  g_object_unref (texture);
}

static int
pixbuf_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (paintable);

  return gdk_pixbuf_animation_get_width (self->anim);
}

static int
pixbuf_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (paintable);

  return gdk_pixbuf_animation_get_height (self->anim);
}

static void
pixbuf_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = pixbuf_paintable_snapshot;
  iface->get_intrinsic_width = pixbuf_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = pixbuf_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE(PixbufPaintable, pixbuf_paintable, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               pixbuf_paintable_init_interface))

static void
pixbuf_paintable_init (PixbufPaintable *paintable)
{
}

static gboolean
delay_cb (gpointer data)
{
  PixbufPaintable *self = data;
  int delay;

  delay = gdk_pixbuf_animation_iter_get_delay_time (self->iter);
  self->timeout = g_timeout_add (delay, delay_cb, self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return G_SOURCE_REMOVE;
}

static void
pixbuf_paintable_set_resource_path (PixbufPaintable *self,
                                    const char      *resource_path)
{
  int delay;

  g_free (self->resource_path);
  self->resource_path = g_strdup (resource_path);

  g_clear_object (&self->anim);
  self->anim = gdk_pixbuf_animation_new_from_resource (resource_path, NULL);
  g_clear_object (&self->iter);
  self->iter = gdk_pixbuf_animation_get_iter (self->anim, NULL);

  delay = gdk_pixbuf_animation_iter_get_delay_time (self->iter);
  self->timeout = g_timeout_add (delay, delay_cb, self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify (G_OBJECT (self), "resource-path");
}

static void
pixbuf_paintable_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_RESOURCE_PATH:
      pixbuf_paintable_set_resource_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pixbuf_paintable_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_RESOURCE_PATH:
      g_value_set_string (value, self->resource_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pixbuf_paintable_dispose (GObject *object)
{
  PixbufPaintable *self = PIXBUF_PAINTABLE (object);

  g_clear_pointer (&self->resource_path, g_free);
  g_clear_object (&self->anim);
  g_clear_object (&self->iter);
  if (self->timeout)
    {
      g_source_remove (self->timeout);
      self->timeout = 0;
    }

  G_OBJECT_CLASS (pixbuf_paintable_parent_class)->dispose (object);
}

static void
pixbuf_paintable_class_init (PixbufPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = pixbuf_paintable_dispose;
  object_class->get_property = pixbuf_paintable_get_property;
  object_class->set_property = pixbuf_paintable_set_property;

  g_object_class_install_property (object_class, PROP_RESOURCE_PATH,
      g_param_spec_string ("resource-path", "Resource path", "Resource path",
                           NULL, G_PARAM_READWRITE));

}

GdkPaintable *
pixbuf_paintable_new_from_resource (const char *path)
{
  return g_object_new (PIXBUF_TYPE_PAINTABLE,
                       "resource-path", path,
                       NULL);
}
