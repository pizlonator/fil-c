
#include "config.h"
#include "subsurfaceoverlay.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gdksurfaceprivate.h"
#include "gdksubsurfaceprivate.h"

struct _GtkSubsurfaceOverlay
{
  GtkInspectorOverlay parent_instance;
};

struct _GtkSubsurfaceOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkSubsurfaceOverlay, gtk_subsurface_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
gtk_subsurface_overlay_snapshot (GtkInspectorOverlay *overlay,
                                 GtkSnapshot         *snapshot,
                                 GskRenderNode       *node,
                                 GtkWidget           *widget)
{
  GdkSurface *surface = gtk_widget_get_surface (widget);
  double native_x, native_y;

  gtk_native_get_surface_transform (GTK_NATIVE (widget), &native_x, &native_y);

  gtk_snapshot_save (snapshot);

  /* Subsurface positions are relative to the surface, so undo the surface
   * transform that gtk_inspector_prepare_render does.
   */
  gtk_snapshot_translate (snapshot, &(graphene_point_t) { - native_x, - native_y });

  for (gsize i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
    {
      GdkSubsurface *subsurface = gdk_surface_get_subsurface (surface, i);
      graphene_rect_t dest;
      GdkRGBA color;

      if (gdk_subsurface_get_texture (subsurface) == NULL)
        continue;

      if (gdk_subsurface_is_above_parent (subsurface))
        gdk_rgba_parse (&color, "goldenrod");
      else
        gdk_rgba_parse (&color, "magenta");

      gdk_subsurface_get_dest (subsurface, &dest);

      /* Use 4 color nodes since a border node overlaps and prevents
       * the subsurface from being raised.
       */
      gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (dest.origin.x - 2, dest.origin.y - 2, 2, dest.size.height + 4));
      gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (dest.origin.x - 2, dest.origin.y - 2, dest.size.width + 4, 2));
      gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (dest.origin.x - 2, dest.origin.y + dest.size.height, dest.size.width + 4, 2));
      gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (dest.origin.x + dest.size.width, dest.origin.y - 2, 2, dest.size.height + 4));
    }

  gtk_snapshot_restore (snapshot);
}

static void
gtk_subsurface_overlay_init (GtkSubsurfaceOverlay *self)
{

}

static void
gtk_subsurface_overlay_class_init (GtkSubsurfaceOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = (GtkInspectorOverlayClass *)klass;

  overlay_class->snapshot = gtk_subsurface_overlay_snapshot;
}

GtkInspectorOverlay *
gtk_subsurface_overlay_new (void)
{
  return g_object_new (GTK_TYPE_SUBSURFACE_OVERLAY, NULL);
}
