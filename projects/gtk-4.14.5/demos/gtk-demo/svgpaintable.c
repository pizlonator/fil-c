#include "svgpaintable.h"

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

struct _SvgPaintable
{
  GObject parent_instance;
  GFile *file;
  RsvgHandle *handle;
};

struct _SvgPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FILE = 1,
  NUM_PROPERTIES
};

static void
svg_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  cairo_t *cr;
  GError *error = NULL;

  cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot),
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (!rsvg_handle_render_document (self->handle, cr,
                                    &(RsvgRectangle) {0, 0, width, height},
                                    &error))
    {
      g_error ("%s", error->message);
    }

  cairo_destroy (cr);
}

static int
svg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  double width;

  if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, NULL))
    return 0;

  return ceil (width);
}

static int
svg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  SvgPaintable *self = SVG_PAINTABLE (paintable);
  double height;

  if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, NULL, &height))
    return 0;

  return ceil (height);
}

static void
svg_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = svg_paintable_snapshot;
  iface->get_intrinsic_width = svg_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = svg_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (SvgPaintable, svg_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                svg_paintable_init_interface))

static void
svg_paintable_init (SvgPaintable *self)
{
}

static void
svg_paintable_dispose (GObject *object)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  g_clear_object (&self->file);
  g_clear_object (&self->handle);

  G_OBJECT_CLASS (svg_paintable_parent_class)->dispose (object);
}

static void
svg_paintable_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      {
        GFile *file = g_value_get_object (value);
        RsvgHandle *handle = rsvg_handle_new_from_gfile_sync (file,
                                                              RSVG_HANDLE_FLAGS_NONE,
                                                              NULL,
                                                              NULL);
        rsvg_handle_set_dpi (handle, 90);

        g_set_object (&self->file, file);
        g_set_object (&self->handle, handle);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
svg_paintable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SvgPaintable *self = SVG_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
svg_paintable_class_init (SvgPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = svg_paintable_dispose;
  object_class->set_property = svg_paintable_set_property;
  object_class->get_property = svg_paintable_get_property;

  g_object_class_install_property (object_class, PROP_FILE,
                                   g_param_spec_object ("file", "File", "File",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE));
}

GdkPaintable *
svg_paintable_new (GFile *file)
{
  return g_object_new (SVG_TYPE_PAINTABLE,
                       "file", file,
                       NULL);
}
