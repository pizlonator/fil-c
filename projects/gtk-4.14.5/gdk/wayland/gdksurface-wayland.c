/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdksurface-wayland.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontext-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkpopupprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkrectangleprivate.h"
#include "gdkseat-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkdevice-wayland-private.h"
#include "gdkdmabuftextureprivate.h"
#include "gdksubsurfaceprivate.h"
#include "gdksubsurface-wayland-private.h"

#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <unistd.h>

#include "gdksurface-wayland-private.h"
#include "gdktoplevel-wayland-private.h"

#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"


/**
 * GdkWaylandSurface:
 *
 * The Wayland implementation of `GdkSurface`.
 *
 * Beyond the [class@Gdk.Surface] API, the Wayland implementation offers
 * access to the Wayland `wl_surface` object with
 * [method@GdkWayland.WaylandSurface.get_wl_surface].
 */

G_DEFINE_TYPE (GdkWaylandSurface, gdk_wayland_surface, GDK_TYPE_SURFACE)

static void gdk_wayland_surface_configure (GdkSurface *surface);

/* {{{ Utilities */

static void
fill_presentation_time_from_frame_time (GdkFrameTimings *timings,
                                        guint32          frame_time)
{
  /* The timestamp in a wayland frame is a msec time value that in some
   * way reflects the time at which the server started drawing the frame.
   * This is not useful from our perspective.
   *
   * However, for the DRM backend of Weston, on reasonably recent
   * Linux, we know that the time is the
   * clock_gettime (CLOCK_MONOTONIC) value at the vblank, and that
   * backend starts drawing immediately after receiving the vblank
   * notification. If we detect this, and make the assumption that the
   * compositor will finish drawing before the next vblank, we can
   * then determine the presentation time as the frame time we
   * received plus one refresh interval.
   *
   * If a backend is using clock_gettime(CLOCK_MONOTONIC), but not
   * picking values right at the vblank, then the presentation times
   * we compute won't be accurate, but not really worse than then
   * the alternative of not providing presentation times at all.
   *
   * The complexity here is dealing with the fact that we receive
   * only the low 32 bits of the CLOCK_MONOTONIC value in milliseconds.
   */
  gint64 now_monotonic = g_get_monotonic_time ();
  gint64 now_monotonic_msec = now_monotonic / 1000;
  uint32_t now_monotonic_low = (uint32_t)now_monotonic_msec;

  if (frame_time - now_monotonic_low < 1000 ||
      frame_time - now_monotonic_low > (uint32_t)-1000)
    {
      /* Timestamp we received is within one second of the current time.
       */
      gint64 last_frame_time = now_monotonic + (gint64)1000 * (gint32)(frame_time - now_monotonic_low);
      if ((gint32)now_monotonic_low < 0 && (gint32)frame_time > 0)
        last_frame_time += (gint64)1000 * G_GINT64_CONSTANT(0x100000000);
      else if ((gint32)now_monotonic_low > 0 && (gint32)frame_time < 0)
        last_frame_time -= (gint64)1000 * G_GINT64_CONSTANT(0x100000000);

      timings->presentation_time = last_frame_time + timings->refresh_interval;
    }
}

static gboolean
is_realized_shell_surface (GdkWaylandSurface *impl)
{
  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

void
gdk_wayland_surface_get_window_geometry (GdkSurface   *surface,
                                         GdkRectangle *geometry)
{     
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
      
  *geometry = (GdkRectangle) {
    .x = impl->shadow_left,
    .y = impl->shadow_top,
    .width = surface->width - (impl->shadow_left + impl->shadow_right),
    .height = surface->height - (impl->shadow_top + impl->shadow_bottom)
  };
}

static struct wl_region *
wl_region_from_cairo_region (GdkWaylandDisplay *display,
                             cairo_region_t    *region)
{     
  struct wl_region *wl_region;
  int i, n_rects;
  
  wl_region = wl_compositor_create_region (display->compositor);
  if (wl_region == NULL)
    return NULL;

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      wl_region_add (wl_region, rect.x, rect.y, rect.width, rect.height);
    }

  return wl_region;
}

/* }}} */
 /* {{{ Surface implementation */

static void
gdk_wayland_surface_init (GdkWaylandSurface *impl)
{
  impl->scale = GDK_FRACTIONAL_SCALE_INIT_INT (1);
  impl->viewport_dirty = TRUE;
}

void
gdk_wayland_surface_freeze_state (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->state_freeze_count++;
}

void
gdk_wayland_surface_thaw_state (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  g_assert (impl->state_freeze_count > 0);

  impl->state_freeze_count--;

  if (impl->state_freeze_count > 0)
    return;

  if (impl->pending.is_dirty)
    gdk_wayland_surface_configure (surface);
}

static inline void
get_egl_window_size (GdkSurface *surface,
                     int        *width,
                     int        *height)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_DISPLAY_DEBUG_CHECK (display, GL_NO_FRACTIONAL))
    {
      *width = surface->width * gdk_fractional_scale_to_int (&impl->scale);
      *height = surface->height * gdk_fractional_scale_to_int (&impl->scale);

      GDK_DISPLAY_DEBUG (display, OPENGL, "Using integer scale %d for EGL window (%d %d => %d %d)",
                         gdk_fractional_scale_to_int (&impl->scale),
                         surface->width, surface->height,
                         *width, *height);
    }
  else
    {
      *width = gdk_fractional_scale_scale (&impl->scale, surface->width),
      *height = gdk_fractional_scale_scale (&impl->scale, surface->height);

      GDK_DISPLAY_DEBUG (display, OPENGL, "Using fractional scale %g for EGL window (%d %d => %d %d)",
                         gdk_fractional_scale_to_double (&impl->scale),
                         surface->width, surface->height,
                         *width, *height);
    }
}

void
gdk_wayland_surface_update_size (GdkSurface               *surface,
                                 int32_t                   width,
                                 int32_t                   height,
                                 const GdkFractionalScale *scale)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gboolean width_changed, height_changed, scale_changed, scale_factor_changed;

  width_changed = surface->width != width;
  height_changed = surface->height != height;
  scale_changed = !gdk_fractional_scale_equal (&impl->scale, scale);
  scale_factor_changed = gdk_fractional_scale_to_int (&impl->scale) != gdk_fractional_scale_to_int (scale);

  if (!width_changed && !height_changed && !scale_changed)
    return;

  surface->width = width;
  surface->height = height;
  if (scale_changed)
    {
      impl->scale = *scale;
      impl->buffer_scale_dirty = TRUE;
      impl->viewport_dirty = TRUE;
    }
  if (width_changed || height_changed)
    impl->viewport_dirty = TRUE;

  if (impl->display_server.egl_window)
    {
      int w, h;
      get_egl_window_size (surface, &w, &h);
      wl_egl_window_resize (impl->display_server.egl_window, w, h, 0, 0);
    }

  gdk_surface_invalidate_rect (surface, NULL);

  if (width_changed)
    g_object_notify (G_OBJECT (surface), "width");
  if (height_changed)
    g_object_notify (G_OBJECT (surface), "height");
  if (scale_changed)
    g_object_notify (G_OBJECT (surface), "scale");
  if (scale_factor_changed)
    g_object_notify (G_OBJECT (surface), "scale-factor");

  _gdk_surface_update_size (surface);
}

static void
gdk_wayland_surface_clear_frame_callback (GdkWaylandSurface *self)
{
  GdkSurface *surface = GDK_SURFACE (self);

  g_clear_pointer (&self->frame_callback, wl_callback_destroy);

  for (gsize i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
    {
      GdkSubsurface *subsurface = gdk_surface_get_subsurface (surface, i);
      gdk_wayland_subsurface_clear_frame_callback (subsurface);
    }
}

void
gdk_wayland_surface_frame_callback (GdkSurface *surface,
                                    uint32_t    time)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);
  GdkFrameTimings *timings;

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "Wayland frame event", NULL);
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (display_wayland), EVENTS, "frame %p", surface);

  gdk_wayland_surface_clear_frame_callback (impl);

  GDK_WAYLAND_SURFACE_GET_CLASS (impl)->handle_frame (impl);

  if (impl->awaiting_frame_frozen)
    {
      impl->awaiting_frame_frozen = FALSE;
      gdk_surface_thaw_updates (surface);
    }

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  if (timings == NULL)
    return;

  timings->refresh_interval = 16667; /* default to 1/60th of a second */
  if (impl->display_server.outputs)
    {
      /* We pick a random output out of the outputs that the surface touches
       * The rate here is in milli-hertz */
      int refresh_rate =
        gdk_wayland_display_get_output_refresh_rate (display_wayland,
                                                     impl->display_server.outputs->data);
      if (refresh_rate != 0)
        timings->refresh_interval = G_GINT64_CONSTANT(1000000000) / refresh_rate;
    }

  fill_presentation_time_from_frame_time (timings, time);

  timings->complete = TRUE;

  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    _gdk_frame_clock_debug_print_timings (clock, timings);

  if (GDK_PROFILER_IS_RUNNING)
    _gdk_frame_clock_add_timings_to_profiler (clock, timings);
}

static void
frame_callback (void               *data,
                struct wl_callback *callback,
                uint32_t            time)
{
  GdkSurface *surface = data;

  g_assert (GDK_WAYLAND_SURFACE (surface)->frame_callback == callback);
  g_assert (!GDK_SURFACE_DESTROYED (surface));

  gdk_wayland_surface_frame_callback (surface, time);
}

static const struct wl_callback_listener frame_listener = {
  frame_callback
};

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkSurface     *surface)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  if (surface->update_freeze_count > 0)
    return;

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time != 0)
    {
      /* Assume the algorithm used by the DRM backend of Weston - it
       * starts drawing at the next vblank after receiving the commit
       * for this frame, and presentation occurs at the vblank
       * after that.
       */
      timings->predicted_presentation_time = presentation_time + refresh_interval;
    }
  else
    {
      /* As above, but we don't actually know the phase of the vblank,
       * so just assume that we're half way through a refresh cycle.
       */
      timings->predicted_presentation_time = timings->frame_time + refresh_interval / 2 + refresh_interval;
    }

  gdk_surface_apply_state_change (surface);
}

static void
gdk_wayland_surface_request_layout (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->next_layout.surface_geometry_dirty = TRUE;
}

void
gdk_wayland_surface_request_frame (GdkSurface *surface)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);
  GdkFrameClock *clock;

  if (self->frame_callback != NULL)
    return;

  clock = gdk_surface_get_frame_clock (surface);

  self->frame_callback = wl_surface_frame (self->display_server.wl_surface);
  wl_proxy_set_queue ((struct wl_proxy *) self->frame_callback, NULL);
  wl_callback_add_listener (self->frame_callback, &frame_listener, surface);

  if (self->presentation_time == NULL)
    {
      GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
      self->presentation_time = gdk_wayland_presentation_time_new (wayland_display);
    }

  gdk_wayland_presentation_time_track (self->presentation_time,
                                       clock,
                                       self->display_server.wl_surface,
                                       gdk_frame_clock_get_frame_counter (clock));

  for (gsize i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
    {
      GdkSubsurface *subsurface = gdk_surface_get_subsurface (surface, i);
      gdk_wayland_subsurface_request_frame (subsurface);
    }

  self->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
}

gboolean
gdk_wayland_surface_has_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return !!impl->display_server.wl_surface;
}

void
gdk_wayland_surface_commit (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  wl_surface_commit (impl->display_server.wl_surface);
}

void
gdk_wayland_surface_notify_committed (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->has_uncommitted_ack_configure = FALSE;
  impl->has_pending_subsurface_commits = FALSE;
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface    *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->update_freeze_count == 0 &&
      (impl->has_uncommitted_ack_configure ||
       impl->has_pending_subsurface_commits))
    {
      gdk_wayland_surface_commit (surface);
      gdk_wayland_surface_notify_committed (surface);
    }

  if (impl->frame_callback &&
      impl->pending_frame_counter == gdk_frame_clock_get_frame_counter (clock))
    {
      g_assert (!impl->awaiting_frame_frozen);
      impl->awaiting_frame_frozen = TRUE;
      gdk_surface_freeze_updates (surface);
    }
}

void
gdk_wayland_surface_update_scale (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  guint32 scale;
  GSList *l;

  /* We can't set the scale on this surface */
  if (!impl->display_server.wl_surface ||
      wl_surface_get_version (impl->display_server.wl_surface) < WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    return;

  /* scale is tracked by the fractional scale extension */
  if (impl->display_server.fractional_scale)
    return;

  if (!impl->display_server.outputs)
    return;

  scale = 1;
  for (l = impl->display_server.outputs; l != NULL; l = l->next)
    {
      struct wl_output *output = l->data;
      uint32_t output_scale;

      output_scale = gdk_wayland_display_get_output_scale (display_wayland,
                                                           output);
      scale = MAX (scale, output_scale);
    }

  /* Notify app that scale changed */
  gdk_wayland_surface_update_size (surface,
                                   surface->width, surface->height,
                                   &GDK_FRACTIONAL_SCALE_INIT_INT (scale));
}

void
gdk_wayland_surface_attach_image (GdkSurface           *surface,
                                  cairo_surface_t      *cairo_surface,
                                  const cairo_region_t *damage)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  cairo_rectangle_int_t rect;
  uint32_t wl_surface_version;
  int i, n;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (_gdk_wayland_is_shm_surface (cairo_surface));

  wl_surface_version = wl_surface_get_version (impl->display_server.wl_surface);

  /* Attach this new buffer to the surface */
  wl_surface_attach (impl->display_server.wl_surface,
                     _gdk_wayland_shm_surface_get_wl_buffer (cairo_surface),
                     0, 0);

  if ((impl->pending_buffer_offset_x || impl->pending_buffer_offset_y) &&
      wl_surface_version >= WL_SURFACE_OFFSET_SINCE_VERSION)
    wl_surface_offset (impl->display_server.wl_surface,
                       impl->pending_buffer_offset_x,
                       impl->pending_buffer_offset_y);
  impl->pending_buffer_offset_x = 0;
  impl->pending_buffer_offset_y = 0;

  n = cairo_region_num_rectangles (damage);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (damage, i, &rect);
      if (wl_surface_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
        {
          float scale = gdk_surface_get_scale (surface);
          gdk_rectangle_transform_affine (&rect, scale, scale, 0, 0, &rect);
          wl_surface_damage_buffer (impl->display_server.wl_surface, rect.x, rect.y, rect.width, rect.height);
        }
      else
        {
          wl_surface_damage (impl->display_server.wl_surface, rect.x, rect.y, rect.width, rect.height);
        }
    }
}

static gboolean
gdk_wayland_surface_beep (GdkSurface *surface)
{
  gdk_wayland_display_system_bell (gdk_surface_get_display (surface), surface);

  return TRUE;
}

static void
gdk_wayland_surface_dispose (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->event_queue)
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      display_wayland->event_queues =
        g_list_remove (display_wayland->event_queues, impl->event_queue);
      g_clear_pointer (&impl->event_queue, wl_event_queue_destroy);
    }

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->dispose (object);
}

static void
gdk_wayland_surface_finalize (GObject *object)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (object));

  impl = GDK_WAYLAND_SURFACE (object);

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  g_clear_pointer (&impl->input_region, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->finalize (object);
}

static void
gdk_wayland_surface_sync_shadow (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;

  if (!is_realized_shell_surface (impl))
    return;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);
  if (GDK_IS_WAYLAND_TOPLEVEL (impl))
    {
      GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (impl);
      gdk_wayland_toplevel_set_geometry_hints (toplevel, NULL, 0);
    }

  if (gdk_rectangle_equal (&geometry, &impl->last_sent_window_geometry))
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_set_window_geometry (impl->display_server.xdg_surface,
                                       geometry.x,
                                       geometry.y,
                                       geometry.width,
                                       geometry.height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_set_window_geometry (impl->display_server.zxdg_surface_v6,
                                           geometry.x,
                                           geometry.y,
                                           geometry.width,
                                           geometry.height);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->last_sent_window_geometry = geometry;
}

static void
gdk_wayland_surface_sync_opaque_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->opaque_region_dirty)
    return;

  if (impl->opaque_region != NULL)
    {
      if (gdk_surface_get_n_subsurfaces (surface) > 0)
        {
          cairo_region_t *region = cairo_region_copy (impl->opaque_region);
          for (gsize i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
            {
              GdkSubsurface *subsurface = gdk_surface_get_subsurface (surface, i);
              GdkWaylandSubsurface *sub = (GdkWaylandSubsurface *) subsurface;

              if (subsurface->above_parent)
                continue;

              if (sub->texture != NULL)
                cairo_region_subtract_rectangle (region, &sub->dest);
            }

          wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                                   region);
          cairo_region_destroy (region);
        }
      else
        wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                                 impl->opaque_region);
    }

  wl_surface_set_opaque_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->opaque_region_dirty = FALSE;
}

static void
gdk_wayland_surface_sync_input_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->input_region_dirty)
    return;

  if (impl->input_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                             impl->input_region);

  wl_surface_set_input_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->input_region_dirty = FALSE;
}

static void
gdk_wayland_surface_sync_buffer_scale (GdkSurface *surface)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);

  if (!self->display_server.wl_surface)
    return;

  if (!self->buffer_scale_dirty)
    return;

  if (self->display_server.viewport)
    {
      /* The viewport takes care of buffer scale */
    }
  else if (wl_surface_get_version (self->display_server.wl_surface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    {
      wl_surface_set_buffer_scale (self->display_server.wl_surface,
                                   gdk_fractional_scale_to_int (&self->scale));
    }

  self->buffer_scale_dirty = FALSE;
}

static void
gdk_wayland_surface_sync_viewport (GdkSurface *surface)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);

  if (!self->display_server.viewport)
    return;

  if (!self->viewport_dirty)
    return;

  wp_viewport_set_destination (self->display_server.viewport,
                               surface->width,
                               surface->height);

  self->viewport_dirty = FALSE;
}

void
gdk_wayland_surface_sync (GdkSurface *surface)
{
  gdk_wayland_surface_sync_shadow (surface);
  gdk_wayland_surface_sync_opaque_region (surface);
  gdk_wayland_surface_sync_input_region (surface);
  gdk_wayland_surface_sync_buffer_scale (surface);
  gdk_wayland_surface_sync_viewport (surface);
}

gboolean
gdk_wayland_surface_needs_commit (GdkSurface *surface)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);

  return self->has_pending_subsurface_commits ||
         self->opaque_region_dirty ||
         self->input_region_dirty ||
         self->buffer_scale_dirty ||
         self->viewport_dirty;
}

static void
gdk_wayland_surface_fractional_scale_preferred_scale_cb (void *data,
                                                         struct wp_fractional_scale_v1 *fractional_scale,
                                                         uint32_t scale)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (data);
  GdkSurface *surface = GDK_SURFACE (self);
  
  /* Notify app that scale changed */
  gdk_wayland_surface_update_size (surface,
                                   surface->width, surface->height,
                                   &GDK_FRACTIONAL_SCALE_INIT (scale));
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
  gdk_wayland_surface_fractional_scale_preferred_scale_cb,
};

static void
surface_enter (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;

  GDK_DISPLAY_DEBUG(gdk_surface_get_display (surface), EVENTS,
                    "surface enter, surface %p output %p", surface, output);

  impl->display_server.outputs = g_slist_prepend (impl->display_server.outputs, output);

  gdk_wayland_surface_update_scale (surface);

  monitor = gdk_wayland_display_get_monitor_for_output (display, output);
  gdk_surface_enter_monitor (surface, monitor);
}

static void
surface_leave (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkMonitor *monitor;

  GDK_DISPLAY_DEBUG (gdk_surface_get_display (surface), EVENTS,
                     "surface leave, surface %p output %p", surface, output);

  impl->display_server.outputs = g_slist_remove (impl->display_server.outputs, output);

  if (impl->display_server.outputs)
    gdk_wayland_surface_update_scale (surface);

  monitor = gdk_wayland_display_get_monitor_for_output (display, output);
  gdk_surface_leave_monitor (surface, monitor);
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

static void
gdk_wayland_surface_create_wl_surface (GdkSurface *surface)
{
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  struct wl_surface *wl_surface;

  wl_surface = wl_compositor_create_surface (display_wayland->compositor);
  wl_proxy_set_queue ((struct wl_proxy *) wl_surface, self->event_queue);
  wl_surface_add_listener (wl_surface, &surface_listener, self);
  if (display_wayland->fractional_scale)
    {
      self->display_server.fractional_scale =
          wp_fractional_scale_manager_v1_get_fractional_scale (display_wayland->fractional_scale,
                                                               wl_surface);
      wp_fractional_scale_v1_add_listener (self->display_server.fractional_scale,
                                           &fractional_scale_listener, self);
    }
  if (display_wayland->viewporter)
    {
      self->display_server.viewport =
          wp_viewporter_get_viewport (display_wayland->viewporter, wl_surface);
    }

  self->display_server.wl_surface = wl_surface;
}

static void
gdk_wayland_surface_constructed (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *self = GDK_WAYLAND_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

  self->event_queue = wl_display_create_queue (display_wayland->wl_display);
  display_wayland->event_queues = g_list_prepend (display_wayland->event_queues,
                                                  self->event_queue);

  /* More likely to be right than just assuming 1 */
  if (wl_compositor_get_version (display_wayland->compositor) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    {
      GdkMonitor *monitor = g_list_model_get_item (gdk_display_get_monitors (display), 0);
      if (monitor)
        {
          guint32 monitor_scale = gdk_monitor_get_scale_factor (monitor);

          if (monitor_scale != 1)
            {
              self->scale = GDK_FRACTIONAL_SCALE_INIT_INT (monitor_scale);
              self->buffer_scale_dirty = TRUE;
            }

          g_object_unref (monitor);
        }
    }

  gdk_wayland_surface_create_wl_surface (surface);

  g_signal_connect (frame_clock, "before-paint", G_CALLBACK (on_frame_clock_before_paint), surface);
  g_signal_connect (frame_clock, "after-paint", G_CALLBACK (on_frame_clock_after_paint), surface);

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->constructed (object);
}

static void
gdk_wayland_surface_destroy_wl_surface (GdkWaylandSurface *self)
{
  if (self->display_server.egl_window)
    {
      gdk_surface_set_egl_native_window (GDK_SURFACE (self), NULL);
      g_clear_pointer (&self->display_server.egl_window, wl_egl_window_destroy);
    }

  g_clear_pointer (&self->display_server.viewport, wp_viewport_destroy);
  g_clear_pointer (&self->display_server.fractional_scale, wp_fractional_scale_v1_destroy);

  g_clear_pointer (&self->display_server.wl_surface, wl_surface_destroy);

  g_clear_pointer (&self->display_server.outputs, g_slist_free);
}

static void
maybe_notify_mapped (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    gdk_surface_set_is_mapped (surface, TRUE);
}

static void
gdk_wayland_surface_configure (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->initial_configure_received)
    {
      gdk_surface_thaw_updates (surface);
      impl->initial_configure_received = TRUE;
      impl->pending.is_initial_configure = TRUE;
      maybe_notify_mapped (surface);
    }

  impl->has_uncommitted_ack_configure = TRUE;

  GDK_WAYLAND_SURFACE_GET_CLASS (impl)->handle_configure (impl);

  impl->last_configure_serial = impl->pending.serial;

  memset (&impl->pending, 0, sizeof (impl->pending));
}

static void
gdk_wayland_surface_handle_configure (GdkWaylandSurface *impl,
                                      uint32_t           serial)
{
  impl->pending.is_dirty = TRUE;
  impl->pending.serial = serial;

  if (impl->state_freeze_count > 0)
    return;

  gdk_wayland_surface_configure (GDK_SURFACE (impl));
}

static void
xdg_surface_configure (void               *data,
                       struct xdg_surface *xdg_surface,
                       uint32_t            serial)
{
  gdk_wayland_surface_handle_configure (GDK_WAYLAND_SURFACE (data), serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  xdg_surface_configure,
};

static void
zxdg_surface_v6_configure (void                   *data,
                           struct zxdg_surface_v6 *xdg_surface,
                           uint32_t                serial)
{
  gdk_wayland_surface_handle_configure (GDK_WAYLAND_SURFACE (data), serial);
}

static const struct zxdg_surface_v6_listener zxdg_surface_v6_listener = {
  zxdg_surface_v6_configure,
};

void
gdk_wayland_surface_create_xdg_surface_resources (GdkSurface *surface)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      impl->display_server.xdg_surface =
        xdg_wm_base_get_xdg_surface (display->xdg_wm_base,
                                     impl->display_server.wl_surface);
      wl_proxy_set_queue ((struct wl_proxy *) impl->display_server.xdg_surface,
                          impl->event_queue);
      xdg_surface_add_listener (impl->display_server.xdg_surface,
                                &xdg_surface_listener,
                                surface);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      impl->display_server.zxdg_surface_v6 =
        zxdg_shell_v6_get_xdg_surface (display->zxdg_shell_v6,
                                       impl->display_server.wl_surface);
      zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                    &zxdg_surface_v6_listener,
                                    surface);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
unmap_popups_for_surface (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland;
  GList *l;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  for (l = display_wayland->current_popups; l; l = l->next)
    {
       GdkSurface *popup = l->data;

       if (popup->parent == surface)
         {
           g_warning ("Tried to unmap the parent of a popup");
           gdk_surface_hide (popup);

           return;
         }
    }
}

void
gdk_wayland_surface_hide_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->mapped)
    return;

  unmap_popups_for_surface (surface);

  g_clear_pointer (&impl->presentation_time, gdk_wayland_presentation_time_free);
  gdk_wayland_surface_clear_frame_callback (impl);
  if (impl->awaiting_frame_frozen)
    {
      impl->awaiting_frame_frozen = FALSE;
      gdk_surface_thaw_updates (surface);
    }

  GDK_WAYLAND_SURFACE_GET_CLASS (impl)->hide_surface (impl);

  if (impl->display_server.xdg_surface)
    {
      xdg_surface_destroy (impl->display_server.xdg_surface);
      impl->display_server.xdg_surface = NULL;
      if (!impl->initial_configure_received)
        gdk_surface_thaw_updates (surface);
      else
        impl->initial_configure_received = FALSE;
    }
  if (impl->display_server.zxdg_surface_v6)
    {
      g_clear_pointer (&impl->display_server.zxdg_surface_v6, zxdg_surface_v6_destroy);
      if (!impl->initial_configure_received)
        gdk_surface_thaw_updates (surface);
      else
        impl->initial_configure_received = FALSE;
    }

  wl_surface_attach (impl->display_server.wl_surface, NULL, 0, 0);
  wl_surface_commit (impl->display_server.wl_surface);

  impl->has_uncommitted_ack_configure = FALSE;
  impl->has_pending_subsurface_commits = FALSE;

  impl->last_sent_window_geometry = (GdkRectangle) { 0 };
  impl->mapped = FALSE;
}

static void
gdk_wayland_surface_hide (GdkSurface *surface)
{
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (surface->display);
  if (seat)
    {
      if (surface->autohide)
        gdk_seat_ungrab (seat);

      gdk_wayland_seat_clear_touchpoints (GDK_WAYLAND_SEAT (seat), surface);
    }
  gdk_wayland_surface_hide_surface (surface);
  _gdk_surface_clear_update_area (surface);
}

void
gdk_wayland_surface_move_resize (GdkSurface *surface,
                                 int         x,
                                 int         y,
                                 int         width,
                                 int         height)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  surface->x = x;
  surface->y = y;
  gdk_wayland_surface_update_size (surface, width, height, &impl->scale);
}

static void
gdk_wayland_surface_get_geometry (GdkSurface *surface,
                                  int        *x,
                                  int        *y,
                                  int        *width,
                                  int        *height)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      if (x)
        *x = surface->x;
      if (y)
        *y = surface->y;
      if (width)
        *width = surface->width;
      if (height)
        *height = surface->height;
    }
}

static void
gdk_wayland_surface_get_root_coords (GdkSurface *surface,
                                     int         x,
                                     int         y,
                                     int        *root_x,
                                     int        *root_y)
{
  /*
   * Wayland does not have a global coordinate space shared between surfaces. In
   * fact, for regular toplevels, we have no idea where our surfaces are
   * positioned, relatively.
   *
   * However, there are some cases like popups and subsurfaces where we do have
   * some amount of control over the placement of our surface, and we can
   * semi-accurately control the x/y position of these surfaces, if they are
   * relative to another surface.
   *
   * To pretend we have something called a root coordinate space, assume all
   * parent-less surfaces are positioned in (0, 0), and all relative positioned
   * popups and subsurfaces are placed within this fake root coordinate space.
   *
   * For example a 200x200 large toplevel surface will have the position (0, 0).
   * If a popup positioned in the middle of the toplevel will have the fake
   * position (100,100). Furthermore, if a positioned is placed in the middle
   * that popup, will have the fake position (150,150), even though it has the
   * relative position (50,50). These three surfaces would make up one single
   * fake root coordinate space.
   */

  if (root_x)
    *root_x = surface->x + x;

  if (root_y)
    *root_y = surface->y + y;
}

static gboolean
gdk_wayland_surface_get_device_state (GdkSurface       *surface,
                                      GdkDevice        *device,
                                      double           *x,
                                      double           *y,
                                      GdkModifierType  *mask)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  gdk_wayland_device_query_state (device, surface, x, y, mask);

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void
gdk_wayland_surface_set_input_region (GdkSurface     *surface,
                                      cairo_region_t *input_region)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_clear_pointer (&impl->input_region, cairo_region_destroy);

  if (input_region)
    impl->input_region = cairo_region_copy (input_region);

  impl->input_region_dirty = TRUE;
}

static void
gdk_wayland_surface_destroy (GdkSurface *surface,
                             gboolean    foreign_destroy)
{
  GdkFrameClock *frame_clock;

  /* Wayland surfaces can't be externally destroyed; we may possibly
   * eventually want to use this path at display close-down
   */
  g_return_if_fail (!foreign_destroy);

  gdk_wayland_surface_hide_surface (surface);

  if (GDK_IS_TOPLEVEL (surface))
    gdk_wayland_toplevel_destroy (GDK_TOPLEVEL (surface));

  gdk_wayland_surface_destroy_wl_surface (GDK_WAYLAND_SURFACE (surface));

  frame_clock = gdk_surface_get_frame_clock (surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_before_paint, surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_after_paint, surface);
}

static void
gdk_wayland_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("GdkSurface %p unexpectedly destroyed", surface);
      _gdk_surface_destroy (surface, TRUE);
    }

  g_object_unref (surface);
}

static double
gdk_wayland_surface_get_scale (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return gdk_fractional_scale_to_double (&impl->scale);
}

static void
gdk_wayland_surface_set_opaque_region (GdkSurface     *surface,
                                       cairo_region_t *region)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  impl->opaque_region = cairo_region_reference (region);
  impl->opaque_region_dirty = TRUE;
}

static void
gdk_wayland_surface_default_handle_configure (GdkWaylandSurface *surface)
{
}

static void
gdk_wayland_surface_default_handle_frame (GdkWaylandSurface *surface)
{
}

static void
gdk_wayland_surface_default_hide_surface (GdkWaylandSurface *surface)
{
}

static void
gdk_wayland_surface_class_init (GdkWaylandSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = gdk_wayland_surface_constructed;
  object_class->dispose = gdk_wayland_surface_dispose;
  object_class->finalize = gdk_wayland_surface_finalize;

  surface_class->hide = gdk_wayland_surface_hide;
  surface_class->get_geometry = gdk_wayland_surface_get_geometry;
  surface_class->get_root_coords = gdk_wayland_surface_get_root_coords;
  surface_class->get_device_state = gdk_wayland_surface_get_device_state;
  surface_class->set_input_region = gdk_wayland_surface_set_input_region;
  surface_class->destroy = gdk_wayland_surface_destroy;
  surface_class->beep = gdk_wayland_surface_beep;

  surface_class->destroy_notify = gdk_wayland_surface_destroy_notify;
  surface_class->drag_begin = _gdk_wayland_surface_drag_begin;
  surface_class->get_scale = gdk_wayland_surface_get_scale;
  surface_class->set_opaque_region = gdk_wayland_surface_set_opaque_region;
  surface_class->request_layout = gdk_wayland_surface_request_layout;
  surface_class->create_subsurface = gdk_wayland_surface_create_subsurface;

  klass->handle_configure = gdk_wayland_surface_default_handle_configure;
  klass->handle_frame = gdk_wayland_surface_default_handle_frame;
  klass->hide_surface = gdk_wayland_surface_default_hide_surface;
}

/* }}} */
/* {{{ Private Surface API */

struct wl_output *
gdk_wayland_surface_get_wl_output (GdkSurface *surface)
{
  GdkWaylandSurface *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  impl = GDK_WAYLAND_SURFACE (surface);
  /* We pick the head of the list as this is the last entered output */
  if (impl->display_server.outputs)
    return (struct wl_output *) impl->display_server.outputs->data;

  return NULL;
}

void
_gdk_wayland_surface_offset_next_wl_buffer (GdkSurface *surface,
                                            int         x,
                                            int         y)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending_buffer_offset_x = x;
  impl->pending_buffer_offset_y = y;
}

void
gdk_wayland_surface_ensure_wl_egl_window (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->display_server.egl_window == NULL)
    {
      int width, height;

      get_egl_window_size (surface, &width, &height);
      impl->display_server.egl_window =
        wl_egl_window_create (impl->display_server.wl_surface, width, height);
      gdk_surface_set_egl_native_window (surface, impl->display_server.egl_window);
    }
}

/* }}} */
/* {{{ Surface API */

/**
 * gdk_wayland_surface_get_wl_surface: (skip)
 * @surface: (type GdkWaylandSurface): a `GdkSurface`
 *
 * Returns the Wayland `wl_surface` of a `GdkSurface`.
 *
 * Returns: (transfer none): a Wayland `wl_surface`
 */
struct wl_surface *
gdk_wayland_surface_get_wl_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  return GDK_WAYLAND_SURFACE (surface)->display_server.wl_surface;
}

/* }}}} */
/* vim:set foldmethod=marker expandtab: */
