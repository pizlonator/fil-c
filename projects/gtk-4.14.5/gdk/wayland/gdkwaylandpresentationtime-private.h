#pragma once

#include <gdk/gdkframeclock.h>
#include <gdk/wayland/gdkdisplay-wayland.h>
#include <gdk/wayland/gdksurface-wayland.h>
#include <gdk/wayland/gdkwaylanddisplay.h>

G_BEGIN_DECLS

typedef struct _GdkWaylandPresentationTime GdkWaylandPresentationTime;

GdkWaylandPresentationTime *gdk_wayland_presentation_time_new   (GdkWaylandDisplay          *display);
void                        gdk_wayland_presentation_time_track (GdkWaylandPresentationTime *self,
                                                                 GdkFrameClock              *frame_clock,
                                                                 struct wl_surface          *surface,
                                                                 gint64                      frame_number);
void                        gdk_wayland_presentation_time_free  (GdkWaylandPresentationTime *self);

G_END_DECLS
