/* GStreamer Wayland video sink
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2011 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2012 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2014 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:element-waylandsink
 * @title: waylandsink
 *
 *  The waylandsink is creating its own window and render the decoded video frames to that.
 *  Setup the Wayland environment as described in
 *  [Wayland](http://wayland.freedesktop.org/building.html) home page.
 *
 *  The current implementation is based on weston compositor.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v videotestsrc ! waylandsink
 * ]| test the video rendering in wayland
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwaylandsink.h"

#include <drm_fourcc.h>
#include <gst/allocators/allocators.h>
#include <gst/video/videooverlay.h>

/* signals */
enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_FULLSCREEN,
  PROP_ROTATE_METHOD,
  PROP_DRM_DEVICE,
  PROP_LAST
};

GST_DEBUG_CATEGORY (gstwayland_debug);
#define GST_CAT_DEFAULT gstwayland_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_WL_VIDEO_FORMATS) ";"
        GST_VIDEO_DMA_DRM_CAPS_MAKE)
    );

static void gst_wayland_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_wayland_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_wayland_sink_finalize (GObject * object);

static GstStateChangeReturn gst_wayland_sink_change_state (GstElement * element,
    GstStateChange transition);
static void gst_wayland_sink_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_wayland_sink_event (GstBaseSink * bsink, GstEvent * event);
static GstCaps *gst_wayland_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_wayland_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static gboolean
gst_wayland_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query);
static GstFlowReturn gst_wayland_sink_show_frame (GstVideoSink * vsink,
    GstBuffer * buffer);

/* VideoOverlay interface */
static void gst_wayland_sink_videooverlay_init (GstVideoOverlayInterface *
    iface);
static void gst_wayland_sink_set_window_handle (GstVideoOverlay * overlay,
    guintptr handle);
static void gst_wayland_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint w, gint h);
static void gst_wayland_sink_expose (GstVideoOverlay * overlay);

#define gst_wayland_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWaylandSink, gst_wayland_sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_wayland_sink_videooverlay_init));
GST_ELEMENT_REGISTER_DEFINE (waylandsink, "waylandsink", GST_RANK_MARGINAL,
    GST_TYPE_WAYLAND_SINK);

static void
gst_wayland_sink_class_init (GstWaylandSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_wayland_sink_set_property;
  gobject_class->get_property = gst_wayland_sink_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_wayland_sink_finalize);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "wayland video sink", "Sink/Video",
      "Output to wayland surface",
      "Sreerenj Balachandran <sreerenj.balachandran@intel.com>, "
      "George Kiagiadakis <george.kiagiadakis@collabora.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_change_state);
  gstelement_class->set_context =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_set_context);

  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_wayland_sink_event);
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_wayland_sink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_wayland_sink_set_caps);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_propose_allocation);

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_show_frame);

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_string ("display", "Wayland Display name", "Wayland "
          "display name to connect to, if not supplied via the GstContext",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FULLSCREEN,
      g_param_spec_boolean ("fullscreen", "Fullscreen",
          "Whether the surface should be made fullscreen ", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * waylandsink:rotate-method:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_ROTATE_METHOD,
      g_param_spec_enum ("rotate-method",
          "rotate method",
          "rotate method",
          GST_TYPE_VIDEO_ORIENTATION_METHOD, GST_VIDEO_ORIENTATION_IDENTITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
   * waylandsink:drm-device:
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_DRM_DEVICE,
      g_param_spec_string ("drm-device", "DRM Device", "Path of the "
          "DRM device to use for dumb buffer allocation",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));


 /**
  * waylandsink:render-rectangle:
  *
  * This helper installs the "render-rectangle" property into the
  * class.
  *
  * Since: 1.22
  */
  gst_video_overlay_install_properties (gobject_class, PROP_LAST);
}

static void
gst_wayland_sink_init (GstWaylandSink * self)
{
  g_mutex_init (&self->display_lock);
  g_mutex_init (&self->render_lock);
}

static void
gst_wayland_sink_set_fullscreen (GstWaylandSink * self, gboolean fullscreen)
{
  if (fullscreen == self->fullscreen)
    return;

  g_mutex_lock (&self->render_lock);
  self->fullscreen = fullscreen;
  gst_wl_window_ensure_fullscreen (self->window, fullscreen);
  g_mutex_unlock (&self->render_lock);
}

static void
gst_wayland_sink_set_rotate_method (GstWaylandSink * self,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  GstVideoOrientationMethod new_method;

  if (method == GST_VIDEO_ORIENTATION_CUSTOM) {
    GST_WARNING_OBJECT (self, "unsupported custom orientation");
    return;
  }

  GST_OBJECT_LOCK (self);
  if (from_tag)
    self->tag_rotate_method = method;
  else
    self->sink_rotate_method = method;

  if (self->sink_rotate_method == GST_VIDEO_ORIENTATION_AUTO)
    new_method = self->tag_rotate_method;
  else
    new_method = self->sink_rotate_method;

  if (new_method != self->current_rotate_method) {
    GST_DEBUG_OBJECT (self, "Changing method from %d to %d",
        self->current_rotate_method, new_method);

    if (self->window) {
      g_mutex_lock (&self->render_lock);
      gst_wl_window_set_rotate_method (self->window, new_method);
      g_mutex_unlock (&self->render_lock);
    }

    self->current_rotate_method = new_method;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_wayland_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->display_name);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_FULLSCREEN:
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->fullscreen);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_ROTATE_METHOD:
      GST_OBJECT_LOCK (self);
      g_value_set_enum (value, self->current_rotate_method);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_DRM_DEVICE:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->drm_device);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wayland_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (object);

  switch (prop_id) {
    case PROP_DISPLAY:
      GST_OBJECT_LOCK (self);
      self->display_name = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_FULLSCREEN:
      GST_OBJECT_LOCK (self);
      gst_wayland_sink_set_fullscreen (self, g_value_get_boolean (value));
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_ROTATE_METHOD:
      gst_wayland_sink_set_rotate_method (self, g_value_get_enum (value),
          FALSE);
      break;
    case PROP_DRM_DEVICE:
      GST_OBJECT_LOCK (self);
      self->drm_device = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      if (!gst_video_overlay_set_property (object, PROP_LAST, prop_id, value))
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wayland_sink_finalize (GObject * object)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (object);

  GST_DEBUG_OBJECT (self, "Finalizing the sink..");

  if (self->last_buffer)
    gst_buffer_unref (self->last_buffer);
  if (self->display)
    g_object_unref (self->display);
  if (self->window)
    g_object_unref (self->window);
  if (self->pool)
    gst_object_unref (self->pool);

  gst_clear_caps (&self->caps);

  g_free (self->display_name);
  g_free (self->drm_device);

  g_mutex_clear (&self->display_lock);
  g_mutex_clear (&self->render_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* must be called with the display_lock */
static void
gst_wayland_sink_set_display_from_context (GstWaylandSink * self,
    GstContext * context)
{
  struct wl_display *display;
  GError *error = NULL;

  display = gst_wl_display_handle_context_get_handle (context);
  self->display = gst_wl_display_new_existing (display, FALSE, &error);

  if (error) {
    GST_ELEMENT_WARNING (self, RESOURCE, OPEN_READ_WRITE,
        ("Could not set display handle"),
        ("Failed to use the external wayland display: '%s'", error->message));
    g_error_free (error);
  }
}

static gboolean
gst_wayland_sink_find_display (GstWaylandSink * self)
{
  GstQuery *query;
  GstMessage *msg;
  GstContext *context = NULL;
  GError *error = NULL;
  gboolean ret = TRUE;

  g_mutex_lock (&self->display_lock);

  if (!self->display) {
    /* first query upstream for the needed display handle */
    query = gst_query_new_context (GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE);
    if (gst_pad_peer_query (GST_VIDEO_SINK_PAD (self), query)) {
      gst_query_parse_context (query, &context);
      gst_wayland_sink_set_display_from_context (self, context);
    }
    gst_query_unref (query);

    if (G_LIKELY (!self->display)) {
      /* now ask the application to set the display handle */
      msg = gst_message_new_need_context (GST_OBJECT_CAST (self),
          GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE);

      g_mutex_unlock (&self->display_lock);
      gst_element_post_message (GST_ELEMENT_CAST (self), msg);
      /* at this point we expect gst_wayland_sink_set_context
       * to get called and fill self->display */
      g_mutex_lock (&self->display_lock);

      if (!self->display) {
        /* if the application didn't set a display, let's create it ourselves */
        GST_OBJECT_LOCK (self);
        self->display = gst_wl_display_new (self->display_name, &error);
        GST_OBJECT_UNLOCK (self);

        if (error) {
          GST_ELEMENT_WARNING (self, RESOURCE, OPEN_READ_WRITE,
              ("Could not initialise Wayland output"),
              ("Failed to create GstWlDisplay: '%s'", error->message));
          g_error_free (error);
          ret = FALSE;
        }
      }
    }
  }

  g_mutex_unlock (&self->display_lock);

  return ret;
}

static GstStateChangeReturn
gst_wayland_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_wayland_sink_find_display (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_buffer_replace (&self->last_buffer, NULL);
      if (self->window) {
        if (gst_wl_window_is_toplevel (self->window)) {
          g_clear_object (&self->window);
        } else {
          /* remove buffer from surface, show nothing */
          gst_wl_window_render (self->window, NULL, NULL);
        }
      }

      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_mutex_lock (&self->display_lock);
      /* If we had a toplevel window, we most likely have our own connection
       * to the display too, and it is a good idea to disconnect and allow
       * potentially the application to embed us with GstVideoOverlay
       * (which requires to re-use the same display connection as the parent
       * surface). If we didn't have a toplevel window, then the display
       * connection that we have is definitely shared with the application
       * and it's better to keep it around (together with the window handle)
       * to avoid requesting them again from the application if/when we are
       * restarted (GstVideoOverlay behaves like that in other sinks)
       */
      if (self->display && !self->window)       /* -> the window was toplevel */
        g_clear_object (&self->display);

      g_mutex_unlock (&self->display_lock);
      g_clear_object (&self->pool);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_wayland_sink_set_context (GstElement * element, GstContext * context)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (element);

  if (gst_context_has_context_type (context,
          GST_WL_DISPLAY_HANDLE_CONTEXT_TYPE)) {
    g_mutex_lock (&self->display_lock);
    if (G_LIKELY (!self->display)) {
      gst_wayland_sink_set_display_from_context (self, context);
    } else {
      GST_WARNING_OBJECT (element, "changing display handle is not supported");
      g_mutex_unlock (&self->display_lock);
      return;
    }
    g_mutex_unlock (&self->display_lock);
  }

  if (GST_ELEMENT_CLASS (parent_class)->set_context)
    GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_wayland_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (bsink);
  GstTagList *taglist;
  GstVideoOrientationMethod method;
  gboolean ret;

  GST_DEBUG_OBJECT (self, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      gst_event_parse_tag (event, &taglist);

      if (gst_video_orientation_from_tag (taglist, &method)) {
        gst_wayland_sink_set_rotate_method (self, method, TRUE);
      }

      break;
    default:
      break;
  }

  ret = GST_BASE_SINK_CLASS (parent_class)->event (bsink, event);

  return ret;
}

static GstCaps *
gst_wayland_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (bsink);;
  GstCaps *caps;

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (self));
  caps = gst_caps_make_writable (caps);

  g_mutex_lock (&self->display_lock);

  if (self->display) {
    GValue shm_list = G_VALUE_INIT, dmabuf_list = G_VALUE_INIT;
    GValue value = G_VALUE_INIT;
    GArray *formats, *modifiers;
    gint i;
    guint fmt;
    GstVideoFormat gfmt;
    guint64 mod;

    g_value_init (&shm_list, GST_TYPE_LIST);
    g_value_init (&dmabuf_list, GST_TYPE_LIST);

    /* Add corresponding shm formats */
    formats = gst_wl_display_get_shm_formats (self->display);
    for (i = 0; i < formats->len; i++) {
      fmt = g_array_index (formats, uint32_t, i);
      gfmt = gst_wl_shm_format_to_video_format (fmt);
      if (gfmt != GST_VIDEO_FORMAT_UNKNOWN) {
        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, gst_video_format_to_string (gfmt));
        gst_value_list_append_and_take_value (&shm_list, &value);
      }
    }

    gst_structure_take_value (gst_caps_get_structure (caps, 0), "format",
        &shm_list);

    /* Add corresponding dmabuf formats */
    formats = gst_wl_display_get_dmabuf_formats (self->display);
    modifiers = gst_wl_display_get_dmabuf_modifiers (self->display);
    for (i = 0; i < formats->len; i++) {
      fmt = g_array_index (formats, uint32_t, i);
      gfmt = gst_wl_dmabuf_format_to_video_format (fmt);
      mod = g_array_index (modifiers, guint64, i);
      if (gfmt != GST_VIDEO_FORMAT_UNKNOWN) {
        g_value_init (&value, G_TYPE_STRING);
        g_value_take_string (&value, gst_wl_dmabuf_format_to_string (fmt, mod));
        gst_value_list_append_and_take_value (&dmabuf_list, &value);
      }
    }

    gst_structure_take_value (gst_caps_get_structure (caps, 1), "drm-format",
        &dmabuf_list);

    GST_DEBUG_OBJECT (self, "display caps: %" GST_PTR_FORMAT, caps);
  }

  g_mutex_unlock (&self->display_lock);

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

static gboolean
gst_wayland_update_pool (GstWaylandSink * self, GstAllocator * allocator)
{
  gsize size = self->video_info.size;
  GstStructure *config;

  /* Pools with outstanding buffer cannot be reconfigured, so we must use
   * a new pool. */
  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_object_unref (self->pool);
  }
  self->pool = gst_wl_video_buffer_pool_new ();
  gst_object_ref_sink (self->pool);

  config = gst_buffer_pool_get_config (self->pool);
  gst_buffer_pool_config_set_params (config, self->caps, size, 2, 0);
  gst_buffer_pool_config_set_allocator (config, allocator, NULL);

  if (!gst_buffer_pool_set_config (self->pool, config))
    return FALSE;

  return gst_buffer_pool_set_active (self->pool, TRUE);
}

static gboolean
gst_wayland_activate_shm_pool (GstWaylandSink * self)
{
  GstAllocator *alloc = NULL;

  if (self->pool && gst_buffer_pool_is_active (self->pool)) {
    GstStructure *config = gst_buffer_pool_get_config (self->pool);
    gboolean is_shm = FALSE;

    if (gst_buffer_pool_config_get_allocator (config, &alloc, NULL) && alloc)
      is_shm = GST_IS_SHM_ALLOCATOR (alloc);

    gst_structure_free (config);

    if (is_shm)
      return TRUE;
  }

  alloc = gst_shm_allocator_get ();
  gst_wayland_update_pool (self, alloc);
  gst_object_unref (alloc);

  return TRUE;
}

static gboolean
gst_wayland_activate_drm_dumb_pool (GstWaylandSink * self)
{
  GstAllocator *alloc;

  if (!self->drm_device)
    return FALSE;

  if (self->pool && gst_buffer_pool_is_active (self->pool)) {
    GstStructure *config = gst_buffer_pool_get_config (self->pool);
    gboolean ret = FALSE;
    gboolean is_drm_dumb = FALSE;

    ret = gst_buffer_pool_config_get_allocator (config, &alloc, NULL);
    gst_structure_free (config);

    if (ret && alloc)
      is_drm_dumb = GST_IS_DRM_DUMB_ALLOCATOR (alloc);

    if (is_drm_dumb)
      return TRUE;
  }

  alloc = gst_drm_dumb_allocator_new_with_device_path (self->drm_device);
  if (!alloc)
    return FALSE;

  gst_wayland_update_pool (self, alloc);
  gst_object_unref (alloc);

  return TRUE;
}

static gboolean
gst_wayland_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (bsink);;
  gboolean use_dmabuf;

  GST_DEBUG_OBJECT (self, "set caps %" GST_PTR_FORMAT, caps);

  if (gst_video_is_dma_drm_caps (caps)) {
    if (!gst_video_info_dma_drm_from_caps (&self->drm_info, caps))
      goto invalid_format;

    if (!gst_video_info_dma_drm_to_video_info (&self->drm_info,
            &self->video_info))
      goto invalid_format;
  } else {
    /* extract info from caps */
    if (!gst_video_info_from_caps (&self->video_info, caps))
      goto invalid_format;

    if (!gst_video_info_dma_drm_from_video_info (&self->drm_info,
            &self->video_info, DRM_FORMAT_MOD_LINEAR))
      gst_video_info_dma_drm_init (&self->drm_info);
  }

  self->video_info_changed = TRUE;
  self->skip_dumb_buffer_copy = FALSE;

  /* free pooled buffer used with previous caps */
  if (self->pool) {
    gst_buffer_pool_set_active (self->pool, FALSE);
    gst_clear_object (&self->pool);
  }

  use_dmabuf = gst_caps_features_contains (gst_caps_get_features (caps, 0),
      GST_CAPS_FEATURE_MEMORY_DMABUF);

  /* validate the format base on the memory type. */
  if (use_dmabuf) {
    if (!gst_wl_display_check_format_for_dmabuf (self->display,
            &self->drm_info))
      goto unsupported_drm_format;
  } else if (!gst_wl_display_check_format_for_shm (self->display,
          &self->video_info)) {
    /* Note: we still support dmabuf in this case, but formats must also be
     * supported on SHM interface to ensure a fallback is possible as we are
     * not guarantied we'll get dmabuf in the buffers. */
    goto unsupported_format;
  }

  /* Will be used to create buffer pools */
  gst_caps_replace (&self->caps, caps);

  return TRUE;

invalid_format:
  {
    GST_ERROR_OBJECT (self,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
unsupported_drm_format:
  {
    GST_ERROR_OBJECT (self, "DRM format %" GST_FOURCC_FORMAT
        " is not available on the display",
        GST_FOURCC_ARGS (self->drm_info.drm_fourcc));
    return FALSE;
  }
unsupported_format:
  {
    GST_ERROR_OBJECT (self, "Format %s is not available on the display",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&self->video_info)));
    return FALSE;
  }
}

static gboolean
gst_wayland_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstCaps *caps;
  GstBufferPool *pool = NULL;
  gboolean need_pool;
  GstAllocator *alloc;
  GstVideoInfoDmaDrm drm_info;
  GstVideoInfo vinfo;
  guint size;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    return FALSE;

  if (gst_video_is_dma_drm_caps (caps)) {
    if (!gst_video_info_dma_drm_from_caps (&drm_info, caps))
      return FALSE;
    size = drm_info.vinfo.size;
  } else {
    /* extract info from caps */
    if (!gst_video_info_from_caps (&vinfo, caps))
      return FALSE;
    size = vinfo.size;
  }

  if (need_pool) {
    GstStructure *config;
    pool = gst_wl_video_buffer_pool_new ();
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
    gst_buffer_pool_config_set_allocator (config,
        gst_shm_allocator_get (), NULL);
    gst_buffer_pool_set_config (pool, config);
  }

  gst_query_add_allocation_pool (query, pool, size, 2, 0);
  if (pool)
    g_object_unref (pool);

  alloc = gst_shm_allocator_get ();
  gst_query_add_allocation_param (query, alloc, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  g_object_unref (alloc);

  return TRUE;
}

/* must be called with the render lock */
static gboolean
render_last_buffer (GstWaylandSink * self, gboolean redraw)
{
  GstWlBuffer *wlbuffer;
  const GstVideoInfo *info = NULL;

  wlbuffer = gst_buffer_get_wl_buffer (self->display, self->last_buffer);

  if (G_UNLIKELY (self->video_info_changed && !redraw)) {
    info = &self->video_info;
    self->video_info_changed = FALSE;
  }
  return gst_wl_window_render (self->window, wlbuffer, info);
}

static void
on_window_closed (GstWlWindow * window, gpointer user_data)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (user_data);

  /* Handle window closure by posting an error on the bus */
  GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
      ("Output window was closed"), (NULL));
}

static GstFlowReturn
gst_wayland_sink_show_frame (GstVideoSink * vsink, GstBuffer * buffer)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (vsink);
  GstBuffer *to_render;
  GstWlBuffer *wlbuffer;
  GstMemory *mem;
  struct wl_buffer *wbuf = NULL;

  GstFlowReturn ret = GST_FLOW_OK;

  g_mutex_lock (&self->render_lock);

  GST_LOG_OBJECT (self, "render buffer %" GST_PTR_FORMAT "", buffer);

  if (G_UNLIKELY (!self->window)) {
    /* ask for window handle. Unlock render_lock while doing that because
     * set_window_handle & friends will lock it in this context */
    g_mutex_unlock (&self->render_lock);
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (self));
    g_mutex_lock (&self->render_lock);

    if (!self->window) {
      /* if we were not provided a window, create one ourselves */
      self->window = gst_wl_window_new_toplevel (self->display,
          &self->video_info, self->fullscreen, &self->render_lock);
      g_signal_connect_object (self->window, "closed",
          G_CALLBACK (on_window_closed), self, 0);
      gst_wl_window_set_rotate_method (self->window,
          self->current_rotate_method);
    }
  }

  /* make sure that the application has called set_render_rectangle() */
  if (G_UNLIKELY (gst_wl_window_get_render_rectangle (self->window)->w == 0))
    goto no_window_size;

  wlbuffer = gst_buffer_get_wl_buffer (self->display, buffer);

  if (G_LIKELY (wlbuffer &&
          gst_wl_buffer_get_display (wlbuffer) == self->display)) {
    GST_LOG_OBJECT (self,
        "buffer %" GST_PTR_FORMAT " has a wl_buffer from our display, "
        "writing directly", buffer);
    to_render = buffer;
    goto render;
  }

  /* update video info from video meta */
  mem = gst_buffer_peek_memory (buffer, 0);

  GST_LOG_OBJECT (self,
      "buffer %" GST_PTR_FORMAT " does not have a wl_buffer from our "
      "display, creating it", buffer);

  if (gst_wl_display_check_format_for_dmabuf (self->display, &self->drm_info)) {
    guint i, nb_dmabuf = 0;

    for (i = 0; i < gst_buffer_n_memory (buffer); i++)
      if (gst_is_dmabuf_memory (gst_buffer_peek_memory (buffer, i)))
        nb_dmabuf++;

    if (nb_dmabuf && (nb_dmabuf == gst_buffer_n_memory (buffer)))
      wbuf = gst_wl_linux_dmabuf_construct_wl_buffer (buffer, self->display,
          &self->drm_info);

    if (!wbuf && !self->skip_dumb_buffer_copy) {
      /* DMABuf did not work, let try and make this a dmabuf, it does not matter
       * if it was a SHM since the compositor needs to copy that anyway, and
       * offloading the compositor from a copy helps maintaining a smoother
       * desktop.
       */
      GstVideoFrame src, dst;

      if (!gst_wayland_activate_drm_dumb_pool (self)) {
        self->skip_dumb_buffer_copy = TRUE;
        goto handle_shm;
      }

      ret = gst_buffer_pool_acquire_buffer (self->pool, &to_render, NULL);
      if (ret != GST_FLOW_OK)
        goto no_buffer;

      wlbuffer = gst_buffer_get_wl_buffer (self->display, to_render);

      /* attach a wl_buffer if there isn't one yet */
      if (G_UNLIKELY (!wlbuffer)) {
        wbuf = gst_wl_linux_dmabuf_construct_wl_buffer (to_render,
            self->display, &self->drm_info);

        if (G_UNLIKELY (!wbuf)) {
          GST_WARNING_OBJECT (self, "failed to import DRM Dumb dmabuf");
          gst_clear_buffer (&to_render);
          self->skip_dumb_buffer_copy = TRUE;
          goto handle_shm;
        }

        wlbuffer = gst_buffer_add_wl_buffer (to_render, wbuf, self->display);
      }

      if (!gst_video_frame_map (&dst, &self->video_info, to_render,
              GST_MAP_WRITE))
        goto dst_map_failed;

      if (!gst_video_frame_map (&src, &self->video_info, buffer, GST_MAP_READ)) {
        gst_video_frame_unmap (&dst);
        goto src_map_failed;
      }

      gst_video_frame_copy (&dst, &src);

      gst_video_frame_unmap (&src);
      gst_video_frame_unmap (&dst);

      goto render;
    }
  }

handle_shm:
  if (!wbuf && gst_wl_display_check_format_for_shm (self->display,
          &self->video_info)) {
    if (gst_buffer_n_memory (buffer) == 1 && gst_is_fd_memory (mem))
      wbuf = gst_wl_shm_memory_construct_wl_buffer (mem, self->display,
          &self->video_info);

    /* If nothing worked, copy into our internal pool */
    if (!wbuf) {
      GstVideoFrame src, dst;

      /* we don't know how to create a wl_buffer directly from the provided
       * memory, so we have to copy the data to shm memory that we know how
       * to handle... */

      GST_LOG_OBJECT (self,
          "buffer %" GST_PTR_FORMAT " cannot have a wl_buffer, "
          "copying to wl_shm memory", buffer);

      /* ensure the internal pool is configured for SHM */
      if (!gst_wayland_activate_shm_pool (self))
        goto activate_failed;

      ret = gst_buffer_pool_acquire_buffer (self->pool, &to_render, NULL);
      if (ret != GST_FLOW_OK)
        goto no_buffer;

      wlbuffer = gst_buffer_get_wl_buffer (self->display, to_render);

      /* attach a wl_buffer if there isn't one yet */
      if (G_UNLIKELY (!wlbuffer)) {
        mem = gst_buffer_peek_memory (to_render, 0);
        wbuf = gst_wl_shm_memory_construct_wl_buffer (mem, self->display,
            &self->video_info);

        if (G_UNLIKELY (!wbuf))
          goto no_wl_buffer_shm;

        wlbuffer = gst_buffer_add_wl_buffer (to_render, wbuf, self->display);
      }

      if (!gst_video_frame_map (&dst, &self->video_info, to_render,
              GST_MAP_WRITE))
        goto dst_map_failed;

      if (!gst_video_frame_map (&src, &self->video_info, buffer, GST_MAP_READ)) {
        gst_video_frame_unmap (&dst);
        goto src_map_failed;
      }

      gst_video_frame_copy (&dst, &src);

      gst_video_frame_unmap (&src);
      gst_video_frame_unmap (&dst);

      goto render;
    }
  }

  if (!wbuf)
    goto no_wl_buffer;

  wlbuffer = gst_buffer_add_wl_buffer (buffer, wbuf, self->display);
  to_render = buffer;

render:
  /* drop double rendering */
  if (G_UNLIKELY (wlbuffer ==
          gst_buffer_get_wl_buffer (self->display, self->last_buffer))) {
    GST_LOG_OBJECT (self, "Buffer already being rendered");
    goto done;
  }

  gst_buffer_replace (&self->last_buffer, to_render);
  if (!render_last_buffer (self, FALSE))
    ret = GST_BASE_SINK_FLOW_DROPPED;

  if (buffer != to_render)
    gst_buffer_unref (to_render);
  goto done;

no_window_size:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Window has no size set"),
        ("Make sure you set the size after calling set_window_handle"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
no_buffer:
  {
    GST_WARNING_OBJECT (self, "could not create buffer");
    goto done;
  }
no_wl_buffer_shm:
  {
    GST_ERROR_OBJECT (self, "could not create wl_buffer out of wl_shm memory");
    ret = GST_FLOW_ERROR;
    goto done;
  }
no_wl_buffer:
  {
    GST_ERROR_OBJECT (self,
        "buffer %" GST_PTR_FORMAT " cannot have a wl_buffer", buffer);
    ret = GST_FLOW_ERROR;
    goto done;
  }
activate_failed:
  {
    GST_ERROR_OBJECT (self, "failed to activate bufferpool.");
    ret = GST_FLOW_ERROR;
    goto done;
  }
src_map_failed:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, READ,
        ("Video memory can not be read from userspace."), (NULL));
    ret = GST_FLOW_ERROR;
    goto done;
  }
dst_map_failed:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
        ("Video memory can not be written from userspace."), (NULL));
    ret = GST_FLOW_ERROR;
    goto done;
  }
done:
  {
    g_mutex_unlock (&self->render_lock);
    return ret;
  }
}

static void
gst_wayland_sink_videooverlay_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_wayland_sink_set_window_handle;
  iface->set_render_rectangle = gst_wayland_sink_set_render_rectangle;
  iface->expose = gst_wayland_sink_expose;
}

static void
gst_wayland_sink_set_window_handle (GstVideoOverlay * overlay, guintptr handle)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (overlay);
  struct wl_surface *surface = (struct wl_surface *) handle;

  g_return_if_fail (self != NULL);

  if (self->window != NULL) {
    GST_WARNING_OBJECT (self, "changing window handle is not supported");
    return;
  }

  g_mutex_lock (&self->render_lock);

  GST_DEBUG_OBJECT (self, "Setting window handle %" GST_PTR_FORMAT,
      (void *) handle);

  g_clear_object (&self->window);

  if (handle) {
    if (G_LIKELY (gst_wayland_sink_find_display (self))) {
      /* we cannot use our own display with an external window handle */
      if (G_UNLIKELY (gst_wl_display_has_own_display (self->display))) {
        GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ_WRITE,
            ("Application did not provide a wayland display handle"),
            ("waylandsink cannot use an externally-supplied surface without "
                "an externally-supplied display handle. Consider providing a "
                "display handle from your application with GstContext"));
      } else {
        self->window = gst_wl_window_new_in_surface (self->display, surface,
            &self->render_lock);
        gst_wl_window_set_rotate_method (self->window,
            self->current_rotate_method);
      }
    } else {
      GST_ERROR_OBJECT (self, "Failed to find display handle, "
          "ignoring window handle");
    }
  }

  g_mutex_unlock (&self->render_lock);
}

static void
gst_wayland_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint w, gint h)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (overlay);

  g_return_if_fail (self != NULL);

  g_mutex_lock (&self->render_lock);
  if (!self->window) {
    g_mutex_unlock (&self->render_lock);
    GST_WARNING_OBJECT (self,
        "set_render_rectangle called without window, ignoring");
    return;
  }

  GST_DEBUG_OBJECT (self, "window geometry changed to (%d, %d) %d x %d",
      x, y, w, h);
  gst_wl_window_set_render_rectangle (self->window, x, y, w, h);

  g_mutex_unlock (&self->render_lock);
}

static void
gst_wayland_sink_expose (GstVideoOverlay * overlay)
{
  GstWaylandSink *self = GST_WAYLAND_SINK (overlay);

  g_return_if_fail (self != NULL);

  GST_DEBUG_OBJECT (self, "expose");

  g_mutex_lock (&self->render_lock);
  if (self->last_buffer) {
    GST_DEBUG_OBJECT (self, "redrawing last buffer");
    render_last_buffer (self, TRUE);
  }
  g_mutex_unlock (&self->render_lock);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gstwayland_debug, "waylandsink", 0,
      " wayland video sink");

  return GST_ELEMENT_REGISTER (waylandsink, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    waylandsink,
    "Wayland Video Sink", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
