#pragma once

#include "gdksubsurfaceprivate.h"


typedef struct _GdkWaylandSubsurface GdkWaylandSubsurface;
typedef struct _GdkWaylandSubsurfaceClass GdkWaylandSubsurfaceClass;

#define GDK_TYPE_WAYLAND_SUBSURFACE              (gdk_wayland_subsurface_get_type ())
#define GDK_WAYLAND_SUBSURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_SUBSURFACE, GdkWaylandSubsurface))
#define GDK_WAYLAND_SUBSURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_SUBSURFACE, GdkWaylandSubsurfaceClass))
#define GDK_IS_WAYLAND_SUBSURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_SUBSURFACE))
#define GDK_WAYLAND_SUBSURFACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_SUBSURFACE, GdkWaylandSubsurfaceClass))

struct _GdkWaylandSubsurface
{
  GdkSubsurface parent_instance;

  struct wl_surface *surface;
  struct wl_subsurface *subsurface;
  struct wp_viewport *viewport;

  GdkTexture *texture;
  cairo_rectangle_int_t dest;
  graphene_rect_t source;

  struct wl_region *opaque_region;

  struct wl_callback *frame_callback;
};

struct _GdkWaylandSubsurfaceClass
{
  GdkSubsurfaceClass parent_class;
};

GType gdk_wayland_subsurface_get_type (void) G_GNUC_CONST;

void gdk_wayland_subsurface_request_frame        (GdkSubsurface *subsurface);
void gdk_wayland_subsurface_clear_frame_callback (GdkSubsurface *subsurface);

GdkSubsurface * gdk_wayland_surface_create_subsurface (GdkSurface *surface);

