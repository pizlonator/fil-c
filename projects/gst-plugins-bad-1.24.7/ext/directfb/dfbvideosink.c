/* GStreamer DirectFB plugin
 * Copyright (C) 2005 Julien MOUTTE <julien@moutte.net>
 * Copyright (C) 2013 Kazunori Kobayashi <kkobayas@igel.co.jp>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-dfbvideosink
 * @title: dfbvideosink
 *
 * DfbVideoSink renders video frames using the
 * [DirectFB](http://www.directfb.org/) library.
 * Rendering can happen in two different modes :
 *
 * * Standalone: this mode will take complete control of the monitor forcing
 *   DirectFB to fullscreen layout.
 *
 *   This is convenient to test using the  gst-launch-1.0 command line tool or
 *   other simple applications. It is possible to interrupt playback while
 *   being in this mode by pressing the Escape key.
 *   This mode handles navigation events for every input device supported by
 *   the DirectFB library, it will look for available video modes in the fb.modes
 *   file and try to switch the framebuffer video mode to the most suitable one.
 *   Depending on hardware acceleration capabilities the element will handle
 *   scaling or not.
 *
 *   If no acceleration is available it will do clipping or centering of the
 *   video frames respecting the original aspect ratio.
 *
 * * Embedded: this mode will render video frames in a
 *   #GstDfbVideoSink:surface provided by the
 *   application developer. This is a more advanced usage of the element and
 *   it is required to integrate video playback in existing
 *   DirectFB applications.
 *
 *   When using this mode the element just renders to the
 *   #GstDfbVideoSink:surface provided by the
 *   application, that means it won't handle navigation events and won't resize
 *   the #GstDfbVideoSink:surface to fit video
 *   frames geometry. Application has to implement the necessary code to grab
 *   information about the negotiated geometry and resize there
 *   #GstDfbVideoSink:surface accordingly.
 *
 * For both modes the element implements a buffer pool allocation system to
 * optimize memory allocation time and handle reverse negotiation. Indeed if
 * you insert an element like videoscale in the pipeline the video sink will
 * negotiate with it to try get a scaled video for either the fullscreen layout
 * or the application provided external #GstDfbVideoSink:surface.
 *
 * ## Example application
 *
 * <include xmlns="http://www.w3.org/2003/XInclude" href="element-dfb-example.xml" />
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v videotestsrc ! dfbvideosink hue=20000 saturation=40000 brightness=25000
 * ]| test the colorbalance interface implementation in dfbvideosink
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>

/* Object header */
#include "dfbvideosink.h"

#include <string.h>
#include <stdlib.h>

/* Debugging category */
GST_DEBUG_CATEGORY_STATIC (dfbvideosink_debug);
#define GST_CAT_DEFAULT dfbvideosink_debug

/* Default template */
static GstStaticPadTemplate gst_dfbvideosink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

/* Signals and args */
enum
{
  ARG_0,
  ARG_SURFACE,
  ARG_CONTRAST,
  ARG_BRIGHTNESS,
  ARG_HUE,
  ARG_SATURATION,
  ARG_PIXEL_ASPECT_RATIO,
  ARG_VSYNC,
  ARG_LAYER_MODE
};

#define DEFAULT_LAYER_MODE LAYER_MODE_EXCLUSIVE

static DFBSurfacePixelFormat gst_dfbvideosink_get_format_from_caps (GstCaps *
    caps);
static void gst_dfbvideosink_update_colorbalance (GstDfbVideoSink *
    dfbvideosink);
static void gst_dfbvideosink_navigation_init (GstNavigationInterface * iface);
static void gst_dfbvideosink_colorbalance_init (GstColorBalanceInterface
    * iface);
static const char *gst_dfbvideosink_get_format_name (DFBSurfacePixelFormat
    format);

#define gst_dfbvideosink_parent_class parent_class

static GType
gst_dfbvideosink_layer_mode_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {0, "NONE", "none"},
    {DLSCL_EXCLUSIVE, "DLSCL_EXCLUSIVE", "exclusive"},
    {DLSCL_ADMINISTRATIVE, "DLSCL_ADMINISTRATIVE", "administrative"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstDfbVideoSinkLayerMode", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

GType
gst_meta_dfbsurface_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstMetaDfbSurfaceAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_meta_dfbsurface_init (GstMetaDfbSurface * meta, gpointer params,
    GstBuffer * buf)
{
  meta->surface = NULL;
  meta->width = meta->height = 0;
  meta->locked = FALSE;
  meta->pixel_format = 0;
  meta->dfbvideosink = NULL;

  return TRUE;
}

/* our metadata */
const GstMetaInfo *
gst_meta_dfbsurface_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (gst_meta_dfbsurface_api_get_type (),
        "GstMetaDfbSurface", sizeof (GstMetaDfbSurface),
        (GstMetaInitFunction) gst_meta_dfbsurface_init,
        (GstMetaFreeFunction) NULL,
        (GstMetaTransformFunction) NULL);
    g_once_init_leave (&meta_info, meta);
  }
  return meta_info;
}

G_DEFINE_TYPE (GstDfbBufferPool, gst_dfb_buffer_pool, GST_TYPE_BUFFER_POOL);

static gboolean
gst_dfb_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstDfbBufferPool *dfbpool = GST_DFB_BUFFER_POOL_CAST (pool);
  GstCaps *caps;
  DFBSurfacePixelFormat pixel_format = DSPF_UNKNOWN;
  gint width, height;
  DFBResult ret;
  DFBSurfaceDescription s_dsc;
  IDirectFBSurface *surface;
  gpointer data;
  gint pitch;
  guint size;
  guint min_buffers;
  guint max_buffers;
  GstVideoInfo info;

  if (!dfbpool->dfbvideosink->setup) {
    GST_WARNING_OBJECT (pool, "DirectFB hasn't been initialized yet.");
    return FALSE;
  }

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, &min_buffers,
          &max_buffers)) {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }

  pixel_format = gst_dfbvideosink_get_format_from_caps (caps);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (pool, "failed getting video info from caps %"
        GST_PTR_FORMAT, caps);
    return FALSE;
  }

  width = GST_VIDEO_INFO_WIDTH (&info);
  height = GST_VIDEO_INFO_HEIGHT (&info);

  /* temporarily create a surface to get the pitch */
  s_dsc.flags = DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_HEIGHT;
  s_dsc.pixelformat = pixel_format;
  s_dsc.width = width;
  s_dsc.height = height;

  ret = dfbpool->dfbvideosink->dfb->CreateSurface (dfbpool->dfbvideosink->dfb,
      &s_dsc, &surface);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (pool, "failed creating surface with format %s",
        gst_dfbvideosink_get_format_name (pixel_format));
    return FALSE;
  }

  ret = surface->Lock (surface, DSLF_READ, &data, &pitch);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (pool, "failed locking the surface");
    surface->Release (surface);
    return FALSE;
  }
  surface->Unlock (surface);
  surface->Release (surface);

  switch (GST_VIDEO_INFO_FORMAT (&info)) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
      size = pitch * height * 3 / 2;
      break;
    default:
      size = pitch * height;
      break;
  }

  gst_buffer_pool_config_set_params (config, caps, size, min_buffers,
      max_buffers);

  dfbpool->caps = gst_caps_ref (caps);

  return GST_BUFFER_POOL_CLASS (gst_dfb_buffer_pool_parent_class)->set_config
      (pool, config);
}

static void
gst_dfb_buffer_pool_free_buffer (GstBufferPool * bpool, GstBuffer * surface)
{
  GstMetaDfbSurface *meta;

  meta = GST_META_DFBSURFACE_GET (surface);

  /* Release our internal surface */
  if (meta->surface) {
    if (meta->locked) {
      meta->surface->Unlock (meta->surface);
      meta->locked = FALSE;
    }
    meta->surface->Release (meta->surface);
  }

  if (meta->dfbvideosink)
    /* Release the ref to our sink */
    gst_object_unref (meta->dfbvideosink);

  GST_BUFFER_POOL_CLASS (gst_dfb_buffer_pool_parent_class)->free_buffer (bpool,
      surface);
}

static GstFlowReturn
gst_dfb_buffer_pool_alloc_buffer (GstBufferPool * bpool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstDfbBufferPool *dfbpool = GST_DFB_BUFFER_POOL_CAST (bpool);
  GstBuffer *surface;
  GstMetaDfbSurface *meta;
  GstStructure *structure;
  DFBResult ret;
  DFBSurfaceDescription s_dsc;
  gpointer data;
  gint pitch;
  GstFlowReturn result = GST_FLOW_ERROR;
  gsize alloc_size;
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0 };
  gint stride[GST_VIDEO_MAX_PLANES] = { 0 };
  gsize max_size;
  gsize plane_size[GST_VIDEO_MAX_PLANES] = { 0 };
  guint n_planes;
  const gchar *str;
  GstVideoFormat format;
  gint i;

  surface = gst_buffer_new ();
  meta = GST_META_DFBSURFACE_ADD (surface);

  /* Keep a ref to our sink */
  meta->dfbvideosink = gst_object_ref (dfbpool->dfbvideosink);
  /* Surface is not locked yet */
  meta->locked = FALSE;

  structure = gst_caps_get_structure (dfbpool->caps, 0);

  if (!gst_structure_get_int (structure, "width", &meta->width) ||
      !gst_structure_get_int (structure, "height", &meta->height)) {
    GST_WARNING_OBJECT (bpool, "failed getting geometry from caps %"
        GST_PTR_FORMAT, dfbpool->caps);
    goto fallback;
  }

  /* Pixel format from caps */
  meta->pixel_format = gst_dfbvideosink_get_format_from_caps (dfbpool->caps);
  if (meta->pixel_format == DSPF_UNKNOWN) {
    goto fallback;
  }

  if (!dfbpool->dfbvideosink->dfb) {
    GST_DEBUG_OBJECT (bpool, "no DirectFB context to create a surface");
    goto fallback;
  }

  /* Creating an internal surface which will be used as GstBuffer, we used
     the detected pixel format and video dimensions */

  s_dsc.flags = DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_HEIGHT;

  s_dsc.pixelformat = meta->pixel_format;
  s_dsc.width = meta->width;
  s_dsc.height = meta->height;

  ret =
      dfbpool->dfbvideosink->dfb->CreateSurface (dfbpool->dfbvideosink->dfb,
      &s_dsc, &meta->surface);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (bpool, "failed creating a DirectFB surface");
    meta->surface = NULL;
    goto fallback;
  }

  /* Clearing surface */
  meta->surface->Clear (meta->surface, 0x00, 0x00, 0x00, 0xFF);

  /* Locking the surface to acquire the memory pointer */
  meta->surface->Lock (meta->surface, DSLF_WRITE, &data, &pitch);
  meta->locked = TRUE;

  GST_DEBUG_OBJECT (bpool, "creating a %dx%d surface (%p) with %s "
      "pixel format, line pitch %d", meta->width, meta->height, surface,
      gst_dfbvideosink_get_format_name (meta->pixel_format), pitch);

  structure = gst_caps_get_structure (dfbpool->caps, 0);
  str = gst_structure_get_string (structure, "format");
  if (str == NULL) {
    GST_WARNING ("failed grabbing fourcc from caps %" GST_PTR_FORMAT,
        dfbpool->caps);
    return GST_FLOW_ERROR;
  }

  format = gst_video_format_from_string (str);
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      offset[1] = pitch * meta->height;
      offset[2] = offset[1] + pitch / 2 * meta->height / 2;
      stride[0] = pitch;
      stride[1] = stride[2] = pitch / 2;

      plane_size[0] = offset[1];
      plane_size[1] = plane_size[2] = plane_size[0] / 4;
      max_size = plane_size[0] * 3 / 2;
      n_planes = 3;
      break;
    case GST_VIDEO_FORMAT_NV12:
      offset[1] = pitch * meta->height;
      stride[0] = stride[1] = pitch;

      plane_size[0] = offset[1];
      plane_size[1] = pitch * meta->height / 2;
      max_size = plane_size[0] * 3 / 2;
      n_planes = 2;
      break;
    default:
      stride[0] = pitch;
      plane_size[0] = max_size = pitch * meta->height;
      n_planes = 1;
      break;
  }

  for (i = 0; i < n_planes; i++) {
    gst_buffer_append_memory (surface,
        gst_memory_new_wrapped (0, data, max_size, offset[i], plane_size[i],
            NULL, NULL));
  }

  gst_buffer_add_video_meta_full (surface, GST_VIDEO_FRAME_FLAG_NONE,
      format, meta->width, meta->height, n_planes, offset, stride);

  result = GST_FLOW_OK;

  goto beach;

fallback:

  /* We allocate a standard buffer ourselves to store it in our buffer pool,
     this is an optimisation for memory allocation */
  alloc_size = meta->width * meta->height;
  surface = gst_buffer_new_allocate (NULL, alloc_size, NULL);
  if (surface == NULL) {
    GST_WARNING_OBJECT (bpool, "failed allocating a gstbuffer");
    goto beach;
  }

  if (meta->surface) {
    if (meta->locked) {
      meta->surface->Unlock (meta->surface);
      meta->locked = FALSE;
    }
    meta->surface->Release (meta->surface);
    meta->surface = NULL;
  }
  GST_DEBUG_OBJECT (bpool, "allocating a buffer (%p) of %u bytes",
      surface, (guint) alloc_size);

  result = GST_FLOW_OK;

beach:
  if (result != GST_FLOW_OK) {
    gst_dfb_buffer_pool_free_buffer (bpool, surface);
    *buffer = NULL;
  } else
    *buffer = surface;

  return result;
}

static GstBufferPool *
gst_dfb_buffer_pool_new (GstDfbVideoSink * dfbvideosink)
{
  GstDfbBufferPool *pool;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), NULL);

  pool = g_object_new (GST_TYPE_DFB_BUFFER_POOL, NULL);
  g_object_ref_sink (pool);
  pool->dfbvideosink = gst_object_ref (dfbvideosink);

  GST_LOG_OBJECT (pool, "new dfb buffer pool %p", pool);

  return GST_BUFFER_POOL_CAST (pool);
}

static void
gst_dfb_buffer_pool_finalize (GObject * object)
{
  GstDfbBufferPool *pool = GST_DFB_BUFFER_POOL_CAST (object);

  if (pool->caps)
    gst_caps_unref (pool->caps);
  gst_object_unref (pool->dfbvideosink);

  G_OBJECT_CLASS (gst_dfb_buffer_pool_parent_class)->finalize (object);
}

static void
gst_dfb_buffer_pool_init (GstDfbBufferPool * pool)
{
  /* No processing */
}

static void
gst_dfb_buffer_pool_class_init (GstDfbBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = gst_dfb_buffer_pool_finalize;

  gstbufferpool_class->alloc_buffer = gst_dfb_buffer_pool_alloc_buffer;
  gstbufferpool_class->set_config = gst_dfb_buffer_pool_set_config;
  gstbufferpool_class->free_buffer = gst_dfb_buffer_pool_free_buffer;
}

G_DEFINE_TYPE_WITH_CODE (GstDfbVideoSink, gst_dfbvideosink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_dfbvideosink_navigation_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_dfbvideosink_colorbalance_init);
    GST_DEBUG_CATEGORY_INIT (dfbvideosink_debug, "dfbvideosink", 0,
        "DirectFB video sink element");
    );
GST_ELEMENT_REGISTER_DEFINE (dfbvideosink, "dfbvideosink", GST_RANK_MARGINAL,
    GST_TYPE_DFBVIDEOSINK);
#ifndef GST_DISABLE_GST_DEBUG
static const char *
gst_dfbvideosink_get_format_name (DFBSurfacePixelFormat format)
{
  switch (format) {
    case DSPF_ARGB1555:
      return "ARGB1555";
    case DSPF_RGB16:
      return "RGB16";
    case DSPF_RGB24:
      return "RGB24";
    case DSPF_RGB32:
      return "RGB32";
    case DSPF_ARGB:
      return "ARGB";
    case DSPF_A8:
      return "A8";
    case DSPF_YUY2:
      return "YUY2";
    case DSPF_RGB332:
      return "RGB33";
    case DSPF_UYVY:
      return "UYVY";
    case DSPF_I420:
      return "I420";
    case DSPF_YV12:
      return "YV12";
    case DSPF_LUT8:
      return "LUT8";
    case DSPF_ALUT44:
      return "ALUT44";
    case DSPF_AiRGB:
      return "AiRGB";
    case DSPF_A1:
      return "A1";
    case DSPF_NV12:
      return "NV12";
    case DSPF_NV16:
      return "NV16";
    case DSPF_ARGB2554:
      return "ARGB2554";
    case DSPF_ARGB4444:
      return "ARGB4444";
    case DSPF_NV21:
      return "NV21";
    default:
      return "UNKNOWN";
  }
}
#endif /* GST_DISABLE_GST_DEBUG */

static gpointer
gst_dfbvideosink_event_thread (GstDfbVideoSink * dfbvideosink)
{
  DFBResult ret;

  while (dfbvideosink->running) {
    /* Wait for an event with a 50 ms timeout */
    dfbvideosink->event_buffer->WaitForEventWithTimeout (dfbvideosink->
        event_buffer, 0, 50);

    /* Do we have an event ? */
    ret = dfbvideosink->event_buffer->HasEvent (dfbvideosink->event_buffer);

    if (ret == DFB_OK) {
      DFBEvent event;

      GST_DEBUG_OBJECT (dfbvideosink, "we have an event");

      ret = dfbvideosink->event_buffer->GetEvent (dfbvideosink->event_buffer,
          &event);
      if (ret != DFB_OK) {      /* Error */
        GST_WARNING_OBJECT (dfbvideosink, "failed when getting event from "
            "event buffer");
      } else {                  /* Handle event */
        if (event.input.type == DIET_KEYPRESS) {
          switch (event.input.key_symbol) {
            case DIKS_ESCAPE:
            {
              GST_ELEMENT_ERROR (dfbvideosink, RESOURCE, OPEN_WRITE,
                  ("Video output device is gone."),
                  ("We were running fullscreen and user "
                      "pressed the ESC key, stopping playback."));
            }
            default:
              GST_DEBUG_OBJECT (dfbvideosink, "key press event %c !",
                  event.input.key_symbol);
              gst_dfbvideosink_navigation_send_event
                  (GST_NAVIGATION (dfbvideosink),
                  gst_navigation_event_new_key_press ("prout",
                      GST_NAVIGATION_MODIFIER_NONE));
          }
        } else if (event.input.type == DIET_BUTTONPRESS) {
          gint x, y;

          dfbvideosink->layer->GetCursorPosition (dfbvideosink->layer, &x, &y);

          GST_DEBUG_OBJECT (dfbvideosink, "button %d pressed at %dx%d",
              event.input.button, x, y);

          gst_dfbvideosink_navigation_send_event
              (GST_NAVIGATION (dfbvideosink),
              gst_navigation_event_new_mouse_button_press (event.input.button,
                  x, y, GST_NAVIGATION_MODIFIER_NONE));
        } else if (event.input.type == DIET_BUTTONRELEASE) {
          gint x, y;

          dfbvideosink->layer->GetCursorPosition (dfbvideosink->layer, &x, &y);

          GST_DEBUG_OBJECT (dfbvideosink, "button %d released at %dx%d",
              event.input.button, x, y);

          gst_dfbvideosink_navigation_send_event
              (GST_NAVIGATION (dfbvideosink),
              gst_navigation_event_new_mouse_button_release (event.input.button,
                  x, y, GST_NAVIGATION_MODIFIER_NONE));
        } else if (event.input.type == DIET_AXISMOTION) {
          gint x, y;

          dfbvideosink->layer->GetCursorPosition (dfbvideosink->layer, &x, &y);
          gst_dfbvideosink_navigation_send_event
              (GST_NAVIGATION (dfbvideosink),
              gst_navigation_event_new_mouse_move (x, y,
                  GST_NAVIGATION_MODIFIER_NONE));
        } else {
          GST_WARNING_OBJECT (dfbvideosink, "unhandled event type %d",
              event.input.type);
        }
      }
    }
  }
  return NULL;
}

static DFBEnumerationResult
gst_dfbvideosink_enum_layers (DFBDisplayLayerID id,
    DFBDisplayLayerDescription desc, void *data)
{
  GstDfbVideoSink *dfbvideosink = NULL;
  IDirectFBDisplayLayer *layer = NULL;
  DFBDisplayLayerConfig dlc;
  DFBResult ret;
  gboolean backbuffer = FALSE;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (data), DFENUM_CANCEL);

  dfbvideosink = GST_DFBVIDEOSINK (data);

  GST_DEBUG_OBJECT (dfbvideosink, "inspecting display layer %d with name: %s",
      id, desc.name);

  if ((desc.type & DLTF_VIDEO) && (desc.caps & DLCAPS_SURFACE)) {
    GST_DEBUG_OBJECT (dfbvideosink,
        "this layer can handle live video and has a surface");
  } else {
    if (desc.caps & DLCAPS_SURFACE) {
      GST_DEBUG_OBJECT (dfbvideosink,
          "this layer can not handle live video but has a surface");
    } else {
      GST_DEBUG_OBJECT (dfbvideosink, "no we can't use that layer, really...");
      goto beach;
    }
  }

  ret = dfbvideosink->dfb->GetDisplayLayer (dfbvideosink->dfb, id, &layer);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed getting display layer %s",
        desc.name);
    goto beach;
  }

  ret = layer->GetConfiguration (layer, &dlc);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink,
        "failed getting display layer configuration");
    goto beach;
  }

  if ((dlc.flags & DLCONF_BUFFERMODE) && (dlc.buffermode & DLBM_FRONTONLY)) {
    GST_DEBUG_OBJECT (dfbvideosink, "no backbuffer");
  }
  if ((dlc.flags & DLCONF_BUFFERMODE) && (dlc.buffermode & DLBM_BACKVIDEO)) {
    GST_DEBUG_OBJECT (dfbvideosink, "backbuffer is in video memory");
    backbuffer = TRUE;
  }
  if ((dlc.flags & DLCONF_BUFFERMODE) && (dlc.buffermode & DLBM_BACKSYSTEM)) {
    GST_DEBUG_OBJECT (dfbvideosink, "backbuffer is in system memory");
    backbuffer = TRUE;
  }
  if ((dlc.flags & DLCONF_BUFFERMODE) && (dlc.buffermode & DLBM_TRIPLE)) {
    GST_DEBUG_OBJECT (dfbvideosink, "triple buffering");
    backbuffer = TRUE;
  }

  /* If the primary is suitable we prefer using it */
  if (dfbvideosink->layer_id != DLID_PRIMARY) {
    GST_DEBUG_OBJECT (dfbvideosink, "selecting layer named %s", desc.name);
    dfbvideosink->layer_id = id;
    dfbvideosink->backbuffer = backbuffer;
  } else {
    GST_DEBUG_OBJECT (dfbvideosink, "layer %s is suitable but the primary "
        "is currently selected and we prefer that one", desc.name);
  }

beach:
  if (layer) {
    layer->Release (layer);
  }
  return DFENUM_OK;
}

static DFBEnumerationResult
gst_dfbvideosink_enum_vmodes (gint width, gint height, gint bpp, void *data)
{
  GstDfbVideoSink *dfbvideosink = NULL;
  GstDfbVMode *vmode = NULL;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (data), DFENUM_CANCEL);

  dfbvideosink = GST_DFBVIDEOSINK (data);

  GST_DEBUG_OBJECT (dfbvideosink, "adding video mode %dx%d at %d bpp", width,
      height, bpp);
  vmode = g_new0 (GstDfbVMode, 1);
  vmode->width = width;
  vmode->height = height;
  vmode->bpp = bpp;

  /* We need to know the maximum video geometry we can accept for the caps */
  if (width > dfbvideosink->out_width) {
    dfbvideosink->out_width = width;
  }
  if (height > dfbvideosink->out_height) {
    dfbvideosink->out_height = height;
  }

  dfbvideosink->vmodes = g_slist_append (dfbvideosink->vmodes, vmode);

  return DFENUM_OK;
}

static DFBEnumerationResult
gst_dfbvideosink_enum_devices (DFBInputDeviceID id,
    DFBInputDeviceDescription desc, void *data)
{
  GstDfbVideoSink *dfbvideosink = NULL;
  IDirectFBInputDevice *device = NULL;
  DFBResult ret;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (data), DFENUM_CANCEL);

  dfbvideosink = GST_DFBVIDEOSINK (data);

  GST_DEBUG_OBJECT (dfbvideosink, "detected input device %s from vendor %s",
      desc.name, desc.vendor);

  /* Get that input device */
  ret = dfbvideosink->dfb->GetInputDevice (dfbvideosink->dfb, id, &device);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed when getting input device id %d",
        id);
    goto beach;
  }

  ret = device->AttachEventBuffer (device, dfbvideosink->event_buffer);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed when attaching input device "
        "%d to our event buffer", id);
  }

beach:
  if (device) {
    device->Release (device);
  }
  return DFENUM_OK;
}

static gboolean
gst_dfbvideosink_setup (GstDfbVideoSink * dfbvideosink)
{
  DFBResult ret;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), FALSE);

  dfbvideosink->video_width = 0;
  dfbvideosink->video_height = 0;
  dfbvideosink->out_width = 0;
  dfbvideosink->out_height = 0;
  dfbvideosink->fps_d = 0;
  dfbvideosink->fps_n = 0;
  dfbvideosink->hw_scaling = FALSE;
  dfbvideosink->backbuffer = FALSE;
  dfbvideosink->pixel_format = DSPF_UNKNOWN;

  /* If we do it all by ourself we create the DirectFB context, get the
     primary layer and use a fullscreen configuration */
  if (!dfbvideosink->ext_surface) {
    GST_DEBUG_OBJECT (dfbvideosink, "no external surface, taking over "
        "DirectFB fullscreen");
    if (!dfbvideosink->dfb) {
      DFBGraphicsDeviceDescription hw_caps;
      char *argv[] = { (char *) "-", (char *) "--dfb:quiet",
        (char *) "--dfb:no-sighandler", NULL
      };
      int argc = 3;
      char **args;

      GST_DEBUG_OBJECT (dfbvideosink, "initializing DirectFB");

      args = argv;
      ret = DirectFBInit (&argc, &args);

      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "DirectFB initialization failed");
        goto beach;
      }

      ret = DirectFBCreate (&(dfbvideosink->dfb));

      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "failed creating the DirectFB "
            "main object");
        goto beach;
      }

      /* Get Hardware capabilities */
      ret = dfbvideosink->dfb->GetDeviceDescription (dfbvideosink->dfb,
          &hw_caps);

      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "failed grabbing the hardware "
            "capabilities");
        goto beach;
      }

      GST_DEBUG_OBJECT (dfbvideosink, "video card %s from vendor %s detected "
          "with %d bytes of video memory", hw_caps.name, hw_caps.vendor,
          hw_caps.video_memory);

      if (hw_caps.acceleration_mask & DFXL_BLIT) {
        GST_DEBUG_OBJECT (dfbvideosink, "Blit is accelerated");
      }
      if (hw_caps.acceleration_mask & DFXL_STRETCHBLIT) {
        GST_DEBUG_OBJECT (dfbvideosink, "StretchBlit is accelerated");
        dfbvideosink->hw_scaling = TRUE;
      } else {
        GST_DEBUG_OBJECT (dfbvideosink, "StretchBlit is not accelerated");
        dfbvideosink->hw_scaling = FALSE;
      }

      dfbvideosink->layer_id = -1;

      /* Inspect all the Display layers */
      dfbvideosink->dfb->EnumDisplayLayers (dfbvideosink->dfb,
          gst_dfbvideosink_enum_layers, dfbvideosink);
      /* Inspect all Video modes */
      dfbvideosink->dfb->EnumVideoModes (dfbvideosink->dfb,
          gst_dfbvideosink_enum_vmodes, dfbvideosink);

      /* Create an event buffer for input */
      dfbvideosink->dfb->CreateEventBuffer (dfbvideosink->dfb,
          &dfbvideosink->event_buffer);

      /* Inspect all Input devices */
      dfbvideosink->dfb->EnumInputDevices (dfbvideosink->dfb,
          gst_dfbvideosink_enum_devices, dfbvideosink);
      /* Create a thread to handle those events */
      dfbvideosink->event_thread = g_thread_new ("dfbvsink-events",
          (GThreadFunc) gst_dfbvideosink_event_thread, dfbvideosink);
    }
    if (!dfbvideosink->layer) {
      GList *channels_list = NULL;
      DFBDisplayLayerDescription dl_desc;

      /* Get the best Display Layer */
      ret = dfbvideosink->dfb->GetDisplayLayer (dfbvideosink->dfb,
          dfbvideosink->layer_id, &dfbvideosink->layer);
      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "failed getting display layer");
        goto beach;
      }

      if (dfbvideosink->layer_mode == LAYER_MODE_EXCLUSIVE ||
          dfbvideosink->layer_mode == LAYER_MODE_ADMINISTRATIVE)
        ret = dfbvideosink->layer->SetCooperativeLevel (dfbvideosink->layer,
            dfbvideosink->layer_mode);
      else {
        GST_ERROR_OBJECT (dfbvideosink, "invalid layer cooperative level");
        goto beach;
      }

      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "failed setting display layer to "
            "fullscreen mode");
        goto beach;
      }

      dfbvideosink->layer->GetDescription (dfbvideosink->layer, &dl_desc);

      /* Check that this layer is able to do colorbalance settings */
      if (dl_desc.caps & DLCAPS_BRIGHTNESS) {
        channels_list = g_list_append (channels_list, (char *) "BRIGHTNESS");
      }
      if (dl_desc.caps & DLCAPS_CONTRAST) {
        channels_list = g_list_append (channels_list, (char *) "CONTRAST");
      }
      if (dl_desc.caps & DLCAPS_HUE) {
        channels_list = g_list_append (channels_list, (char *) "HUE");
      }
      if (dl_desc.caps & DLCAPS_SATURATION) {
        channels_list = g_list_append (channels_list, (char *) "SATURATION");
      }

      if (channels_list) {
        GList *walk = channels_list;

        /* Generate Color balance channel list */
        while (walk) {
          GstColorBalanceChannel *channel = NULL;

          GST_DEBUG_OBJECT (dfbvideosink, "adding %s as a colorbalance channel",
              (const char *) walk->data);

          channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
          channel->label = g_strdup (walk->data);
          channel->min_value = 0x0000;
          channel->max_value = 0xFFFF;

          dfbvideosink->cb_channels = g_list_append (dfbvideosink->cb_channels,
              channel);

          walk = g_list_next (walk);
        }

        /* If the colorbalance settings have not been touched we get current
           values as defaults and update our internal variables */
        if (!dfbvideosink->cb_changed) {
          DFBColorAdjustment cb_adjust;

          ret = dfbvideosink->layer->GetColorAdjustment (dfbvideosink->layer,
              &cb_adjust);

          if (ret != DFB_OK) {
            GST_WARNING_OBJECT (dfbvideosink, "failed when getting color "
                "adjustment from layer");
          }

          if (cb_adjust.flags & DCAF_BRIGHTNESS) {
            dfbvideosink->brightness = cb_adjust.brightness;
          } else {
            dfbvideosink->brightness = 0x8000;
          }
          if (cb_adjust.flags & DCAF_CONTRAST) {
            dfbvideosink->contrast = cb_adjust.contrast;
          } else {
            dfbvideosink->contrast = 0x8000;
          }
          if (cb_adjust.flags & DCAF_HUE) {
            dfbvideosink->hue = cb_adjust.hue;
          } else {
            dfbvideosink->hue = 0x8000;
          }
          if (cb_adjust.flags & DCAF_SATURATION) {
            dfbvideosink->saturation = cb_adjust.saturation;
          } else {
            dfbvideosink->saturation = 0x8000;
          }
          GST_DEBUG_OBJECT (dfbvideosink, "brightness %d, contrast %d, "
              "hue %d, saturation %d", dfbvideosink->brightness,
              dfbvideosink->contrast, dfbvideosink->hue,
              dfbvideosink->saturation);
        }

        g_list_free (channels_list);

        gst_dfbvideosink_update_colorbalance (dfbvideosink);
      }

      dfbvideosink->layer->SetBackgroundColor (dfbvideosink->layer,
          0x00, 0x00, 0x00, 0xFF);

#if (DIRECTFB_VER >= GST_DFBVIDEOSINK_VER (1,6,0))
      if (dfbvideosink->layer_mode == LAYER_MODE_ADMINISTRATIVE)
#endif
        dfbvideosink->layer->EnableCursor (dfbvideosink->layer, TRUE);

      GST_DEBUG_OBJECT (dfbvideosink, "getting primary surface");
      dfbvideosink->layer->GetSurface (dfbvideosink->layer,
          &dfbvideosink->primary);

      dfbvideosink->primary->SetBlittingFlags (dfbvideosink->primary,
          DSBLIT_NOFX);
    }

    dfbvideosink->primary->GetPixelFormat (dfbvideosink->primary,
        &dfbvideosink->pixel_format);
  } else {
    DFBSurfaceCapabilities s_caps;

    GST_DEBUG_OBJECT (dfbvideosink, "getting pixel format from foreign "
        "surface %p", dfbvideosink->ext_surface);
    dfbvideosink->ext_surface->GetPixelFormat (dfbvideosink->ext_surface,
        &dfbvideosink->pixel_format);
    dfbvideosink->ext_surface->GetSize (dfbvideosink->ext_surface,
        &dfbvideosink->out_width, &dfbvideosink->out_height);
    dfbvideosink->ext_surface->GetCapabilities (dfbvideosink->ext_surface,
        &s_caps);
    if ((s_caps & DSCAPS_DOUBLE) || (s_caps & DSCAPS_TRIPLE)) {
      dfbvideosink->backbuffer = TRUE;
    } else {
      dfbvideosink->backbuffer = FALSE;
    }
    GST_DEBUG_OBJECT (dfbvideosink, "external surface is %dx%d and uses %s "
        "pixel format", dfbvideosink->out_width, dfbvideosink->out_height,
        gst_dfbvideosink_get_format_name (dfbvideosink->pixel_format));
  }

  dfbvideosink->setup = TRUE;

beach:
  return dfbvideosink->setup;
}

static void
gst_dfbvideosink_cleanup (GstDfbVideoSink * dfbvideosink)
{
  g_return_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink));

  GST_DEBUG_OBJECT (dfbvideosink, "cleaning up DirectFB environment");

  /* Wait for our event thread */
  if (dfbvideosink->event_thread) {
    g_thread_join (dfbvideosink->event_thread);
    dfbvideosink->event_thread = NULL;
  }

  if (dfbvideosink->event_buffer) {
    dfbvideosink->event_buffer->Release (dfbvideosink->event_buffer);
    dfbvideosink->event_buffer = NULL;
  }

  if (dfbvideosink->vmodes) {
    GSList *walk = dfbvideosink->vmodes;

    while (walk) {
      g_free (walk->data);
      walk = g_slist_next (walk);
    }
    g_slist_free (dfbvideosink->vmodes);
    dfbvideosink->vmodes = NULL;
  }

  if (dfbvideosink->cb_channels) {
    GList *walk = dfbvideosink->cb_channels;

    while (walk) {
      GstColorBalanceChannel *channel = walk->data;

      g_object_unref (channel);
      walk = g_list_next (walk);
    }
    g_list_free (dfbvideosink->cb_channels);
    dfbvideosink->cb_channels = NULL;
  }

  if (dfbvideosink->pool) {
    gst_object_unref (dfbvideosink->pool);
    dfbvideosink->pool = NULL;
  }

  if (dfbvideosink->primary) {
    dfbvideosink->primary->Release (dfbvideosink->primary);
    dfbvideosink->primary = NULL;
  }

  if (dfbvideosink->layer) {
#if (DIRECTFB_VER >= GST_DFBVIDEOSINK_VER (1,6,0))
    if (dfbvideosink->layer_mode == LAYER_MODE_ADMINISTRATIVE)
#endif
      dfbvideosink->layer->EnableCursor (dfbvideosink->layer, FALSE);
    dfbvideosink->layer->Release (dfbvideosink->layer);
    dfbvideosink->layer = NULL;
  }

  if (dfbvideosink->dfb) {
    dfbvideosink->dfb->Release (dfbvideosink->dfb);
    dfbvideosink->dfb = NULL;
  }

  dfbvideosink->setup = FALSE;
}

static DFBSurfacePixelFormat
gst_dfbvideosink_get_format_from_caps (GstCaps * caps)
{
  GstStructure *structure;
  DFBSurfacePixelFormat pixel_format = DSPF_UNKNOWN;
  const gchar *str;
  GstVideoFormat format;

  g_return_val_if_fail (GST_IS_CAPS (caps), DSPF_UNKNOWN);

  structure = gst_caps_get_structure (caps, 0);
  str = gst_structure_get_string (structure, "format");
  if (str == NULL) {
    GST_WARNING ("failed grabbing fourcc from caps %" GST_PTR_FORMAT, caps);
    return DSPF_UNKNOWN;
  }

  format = gst_video_format_from_string (str);
  switch (format) {
    case GST_VIDEO_FORMAT_RGB16:
      pixel_format = DSPF_RGB16;
      break;
    case GST_VIDEO_FORMAT_RGB:
      pixel_format = DSPF_RGB24;
      break;
    case GST_VIDEO_FORMAT_xRGB:
      pixel_format = DSPF_RGB32;
      break;
    case GST_VIDEO_FORMAT_ARGB:
      pixel_format = DSPF_ARGB;
      break;
    case GST_VIDEO_FORMAT_I420:
      pixel_format = DSPF_I420;
      break;
    case GST_VIDEO_FORMAT_YV12:
      pixel_format = DSPF_YV12;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      pixel_format = DSPF_YUY2;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      pixel_format = DSPF_UYVY;
      break;
    case GST_VIDEO_FORMAT_NV12:
      pixel_format = DSPF_NV12;
      break;
    default:
      GST_WARNING ("unhandled pixel format %s", str);
      return DSPF_UNKNOWN;
  }

  return pixel_format;
}

static GstCaps *
gst_dfbvideosink_get_caps_from_format (DFBSurfacePixelFormat format)
{
  const char *fourcc;

  g_return_val_if_fail (format != DSPF_UNKNOWN, NULL);

  switch (format) {
    case DSPF_RGB16:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_RGB16);
      break;
    case DSPF_RGB24:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_RGB);
      break;
    case DSPF_RGB32:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_xRGB);
      break;
    case DSPF_ARGB:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_ARGB);
      break;
    case DSPF_YUY2:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_YUY2);
      break;
    case DSPF_UYVY:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_UYVY);
      break;
    case DSPF_I420:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_I420);
      break;
    case DSPF_YV12:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_YV12);
      break;
    case DSPF_NV12:
      fourcc = gst_video_format_to_string (GST_VIDEO_FORMAT_NV12);
      break;
    default:
      GST_WARNING ("unknown pixel format %s",
          gst_dfbvideosink_get_format_name (format));
      return NULL;
  }

  return gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, fourcc,
      NULL);
}

static gboolean
gst_dfbvideosink_can_blit_from_format (GstDfbVideoSink * dfbvideosink,
    DFBSurfacePixelFormat format, gboolean accelerated)
{
  gboolean res = FALSE;
  DFBResult ret;
  IDirectFBSurface *surface = NULL;
  DFBSurfaceDescription s_dsc;
  DFBAccelerationMask mask;
  DFBDisplayLayerConfig dlc, prev_dlc;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), FALSE);

  /* Create a surface of desired format */
  s_dsc.flags = DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_HEIGHT;
  s_dsc.pixelformat = format;
  s_dsc.width = 10;
  s_dsc.height = 10;

  ret = dfbvideosink->dfb->CreateSurface (dfbvideosink->dfb, &s_dsc, &surface);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed creating surface with format %s",
        gst_dfbvideosink_get_format_name (format));
    goto beach;
  }

  /* Backup layer configuration */
  ret = dfbvideosink->layer->GetConfiguration (dfbvideosink->layer, &prev_dlc);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed when getting current layer "
        "configuration");
    goto beach;
  }

  /* Test configuration of the layer to this pixel format */
  dlc.flags = DLCONF_PIXELFORMAT;
  dlc.pixelformat = format;

  ret = dfbvideosink->layer->TestConfiguration (dfbvideosink->layer, &dlc,
      NULL);
  if (ret != DFB_OK) {
    GST_DEBUG_OBJECT (dfbvideosink, "our layer refuses to operate in pixel "
        "format %s", gst_dfbvideosink_get_format_name (format));
    goto beach;
  }

  ret = dfbvideosink->layer->SetConfiguration (dfbvideosink->layer, &dlc);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "our layer refuses to operate in pixel "
        "format, though this format was successfully tested earlied %s",
        gst_dfbvideosink_get_format_name (format));
    goto beach;
  }

  ret = dfbvideosink->primary->GetAccelerationMask (dfbvideosink->primary,
      surface, &mask);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed getting acceleration mask");
    goto beach;
  }

  /* Blitting from this format to our primary is accelerated */
  if ((mask & DFXL_BLIT) && accelerated) {
    GST_DEBUG_OBJECT (dfbvideosink, "blitting from format %s to our primary "
        "is accelerated", gst_dfbvideosink_get_format_name (format));
    res = TRUE;
  } else if (!accelerated) {
    GST_DEBUG_OBJECT (dfbvideosink, "blitting from format %s to our primary "
        "is not accelerated", gst_dfbvideosink_get_format_name (format));
    res = TRUE;
  }

  /* Restore original layer configuration */
  ret = dfbvideosink->layer->SetConfiguration (dfbvideosink->layer, &prev_dlc);
  if (ret != DFB_OK) {
    GST_WARNING_OBJECT (dfbvideosink, "failed when restoring layer "
        "configuration");
    goto beach;
  }

beach:
  if (surface) {
    surface->Release (surface);
  }
  return res;
}

static gboolean
gst_dfbvideosink_get_best_vmode (GstDfbVideoSink * dfbvideosink, gint v_width,
    gint v_height, GstDfbVMode * best_vmode)
{
  GSList *walk = NULL;
  gboolean ret = FALSE;
  gint width, height, bpp;
  GstDfbVMode *vmode = NULL;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), FALSE);

  if (!dfbvideosink->vmodes) {
    goto beach;
  }

  walk = dfbvideosink->vmodes;

  vmode = (GstDfbVMode *) walk->data;

  /* First mode */
  width = vmode->width;
  height = vmode->height;
  bpp = vmode->bpp;

  while (walk) {
    gint wgap, hgap, best_wgap, best_hgap;

    vmode = (GstDfbVMode *) walk->data;

    /* What are the gaps */
    wgap = abs (vmode->width - v_width);
    hgap = abs (vmode->height - v_height);
    best_wgap = abs (width - v_width);
    best_hgap = abs (height - v_height);

    /* If this mode is better we ll use that */
    if (wgap + hgap < best_wgap + best_hgap) {
      width = vmode->width;
      height = vmode->height;
      bpp = vmode->bpp;
    }

    walk = g_slist_next (walk);
  }

  GST_DEBUG_OBJECT (dfbvideosink, "found video mode %dx%d for input at %dx%d",
      width, height, v_width, v_height);

  best_vmode->width = width;
  best_vmode->height = height;
  best_vmode->bpp = bpp;

  ret = TRUE;

beach:
  return ret;
}

static GstCaps *
gst_dfbvideosink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstDfbVideoSink *dfbvideosink;
  GstCaps *caps = NULL;
  GstCaps *returned_caps;
  gint i;

  dfbvideosink = GST_DFBVIDEOSINK (bsink);

  if (!dfbvideosink->setup) {
    GstCaps *tcaps =
        gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (dfbvideosink));
    caps = gst_caps_copy (tcaps);
    gst_caps_unref (tcaps);
    GST_DEBUG_OBJECT (dfbvideosink, "getcaps called and we are not setup yet, "
        "returning template %" GST_PTR_FORMAT, caps);
    goto beach;
  } else {
    GST_DEBUG_OBJECT (dfbvideosink, "getcaps called, checking our internal "
        "format");
    if (dfbvideosink->ext_surface) {
      /* We are not rendering to our own surface, returning this surface's
       *  pixel format */
      caps = gst_dfbvideosink_get_caps_from_format (dfbvideosink->pixel_format);
    } else {
      /* Try some formats */
      gboolean accelerated = TRUE;
      caps = gst_caps_new_empty ();

      do {
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_RGB16,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_RGB16));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_RGB24,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_RGB24));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_RGB32,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_RGB32));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_ARGB,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_ARGB));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_NV12,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_NV12));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_YUY2,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_YUY2));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_UYVY,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_UYVY));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_I420,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_I420));
        }
        if (gst_dfbvideosink_can_blit_from_format (dfbvideosink, DSPF_YV12,
                accelerated)) {
          gst_caps_append (caps,
              gst_dfbvideosink_get_caps_from_format (DSPF_YV12));
        }
        accelerated = !accelerated;
      } while (accelerated == FALSE);
    }
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);

    gst_structure_set (structure,
        "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

    if (!dfbvideosink->hw_scaling && dfbvideosink->par) {
      int nom, den;

      nom = gst_value_get_fraction_numerator (dfbvideosink->par);
      den = gst_value_get_fraction_denominator (dfbvideosink->par);
      gst_structure_set (structure, "pixel-aspect-ratio",
          GST_TYPE_FRACTION, nom, den, NULL);
    }
  }

beach:
  if (filter) {
    returned_caps = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
  } else
    returned_caps = caps;

  GST_DEBUG_OBJECT (dfbvideosink, "returning our caps %" GST_PTR_FORMAT,
      returned_caps);

  return returned_caps;
}

static gboolean
gst_dfbvideosink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstDfbVideoSink *dfbvideosink;
  GstStructure *structure;
  gboolean res, result = FALSE;
  gint video_width, video_height;
  const GValue *framerate;
  DFBSurfacePixelFormat pixel_format = DSPF_UNKNOWN;

  dfbvideosink = GST_DFBVIDEOSINK (bsink);

  structure = gst_caps_get_structure (caps, 0);
  res = gst_structure_get_int (structure, "width", &video_width);
  res &= gst_structure_get_int (structure, "height", &video_height);
  framerate = gst_structure_get_value (structure, "framerate");
  res &= (framerate != NULL);
  if (!res) {
    goto beach;
  }

  dfbvideosink->fps_n = gst_value_get_fraction_numerator (framerate);
  dfbvideosink->fps_d = gst_value_get_fraction_denominator (framerate);

  pixel_format = gst_dfbvideosink_get_format_from_caps (caps);

  GST_DEBUG_OBJECT (dfbvideosink, "setcaps called with %" GST_PTR_FORMAT, caps);
  GST_DEBUG_OBJECT (dfbvideosink, "our format is: %dx%d %s video at %d/%d fps",
      video_width, video_height,
      gst_dfbvideosink_get_format_name (pixel_format), dfbvideosink->fps_n,
      dfbvideosink->fps_d);

  if (dfbvideosink->hw_scaling && dfbvideosink->par) {
    gint video_par_n, video_par_d;      /* video's PAR */
    gint display_par_n, display_par_d;  /* display's PAR */
    gint num, den;
    GValue display_ratio = { 0, };      /* display w/h ratio */
    const GValue *caps_par;

    /* get aspect ratio from caps if it's present, and
     * convert video width and height to a display width and height
     * using wd / hd = wv / hv * PARv / PARd
     * the ratio wd / hd will be stored in display_ratio */
    g_value_init (&display_ratio, GST_TYPE_FRACTION);

    /* get video's PAR */
    caps_par = gst_structure_get_value (structure, "pixel-aspect-ratio");
    if (caps_par) {
      video_par_n = gst_value_get_fraction_numerator (caps_par);
      video_par_d = gst_value_get_fraction_denominator (caps_par);
    } else {
      video_par_n = 1;
      video_par_d = 1;
    }
    /* get display's PAR */
    if (dfbvideosink->par) {
      display_par_n = gst_value_get_fraction_numerator (dfbvideosink->par);
      display_par_d = gst_value_get_fraction_denominator (dfbvideosink->par);
    } else {
      display_par_n = 1;
      display_par_d = 1;
    }

    gst_value_set_fraction (&display_ratio,
        video_width * video_par_n * display_par_d,
        video_height * video_par_d * display_par_n);

    num = gst_value_get_fraction_numerator (&display_ratio);
    den = gst_value_get_fraction_denominator (&display_ratio);
    GST_DEBUG_OBJECT (dfbvideosink,
        "video width/height: %dx%d, calculated display ratio: %d/%d",
        video_width, video_height, num, den);

    /* now find a width x height that respects this display ratio.
     * prefer those that have one of w/h the same as the incoming video
     * using wd / hd = num / den */

    /* start with same height, because of interlaced video */
    /* check hd / den is an integer scale factor, and scale wd with the PAR */
    if (video_height % den == 0) {
      GST_DEBUG_OBJECT (dfbvideosink, "keeping video height");
      GST_VIDEO_SINK_WIDTH (dfbvideosink) = video_height * num / den;
      GST_VIDEO_SINK_HEIGHT (dfbvideosink) = video_height;
    } else if (video_width % num == 0) {
      GST_DEBUG_OBJECT (dfbvideosink, "keeping video width");
      GST_VIDEO_SINK_WIDTH (dfbvideosink) = video_width;
      GST_VIDEO_SINK_HEIGHT (dfbvideosink) = video_width * den / num;
    } else {
      GST_DEBUG_OBJECT (dfbvideosink, "approximating while keeping height");
      GST_VIDEO_SINK_WIDTH (dfbvideosink) = video_height * num / den;
      GST_VIDEO_SINK_HEIGHT (dfbvideosink) = video_height;
    }
    GST_DEBUG_OBJECT (dfbvideosink, "scaling to %dx%d",
        GST_VIDEO_SINK_WIDTH (dfbvideosink),
        GST_VIDEO_SINK_HEIGHT (dfbvideosink));
  } else {
    if (dfbvideosink->par) {
      const GValue *par;

      par = gst_structure_get_value (structure, "pixel-aspect-ratio");
      if (par) {
        if (gst_value_compare (par, dfbvideosink->par) != GST_VALUE_EQUAL) {
          goto wrong_aspect;
        }
      }
    }
    GST_VIDEO_SINK_WIDTH (dfbvideosink) = video_width;
    GST_VIDEO_SINK_HEIGHT (dfbvideosink) = video_height;
  }

  /* Try to adapt the video mode to the video geometry */
  if (dfbvideosink->dfb) {
    DFBResult ret;
    GstDfbVMode vmode;
    DFBDisplayLayerConfig lc;

    GST_DEBUG_OBJECT (dfbvideosink, "trying to adapt the video mode to video "
        "geometry");

    /* Set video mode and layer configuration appropriately */
    if (gst_dfbvideosink_get_best_vmode (dfbvideosink,
            GST_VIDEO_SINK_WIDTH (dfbvideosink),
            GST_VIDEO_SINK_HEIGHT (dfbvideosink), &vmode)) {
      gint width, height, bpp;

      width = vmode.width;
      height = vmode.height;
      bpp = vmode.bpp;

      GST_DEBUG_OBJECT (dfbvideosink, "setting video mode to %dx%d at %d bpp",
          width, height, bpp);

      ret = dfbvideosink->dfb->SetVideoMode (dfbvideosink->dfb, width,
          height, bpp);
      if (ret != DFB_OK) {
        GST_WARNING_OBJECT (dfbvideosink, "failed setting video mode %dx%d "
            "at %d bpp", width, height, bpp);
      }
    }

    lc.flags = DLCONF_PIXELFORMAT;
    lc.pixelformat = pixel_format;

    ret = dfbvideosink->layer->SetConfiguration (dfbvideosink->layer, &lc);
    if (ret != DFB_OK) {
      GST_WARNING_OBJECT (dfbvideosink, "failed setting layer pixelformat "
          "to %s", gst_dfbvideosink_get_format_name (pixel_format));
    } else {
      dfbvideosink->layer->GetConfiguration (dfbvideosink->layer, &lc);
      dfbvideosink->out_width = lc.width;
      dfbvideosink->out_height = lc.height;
      dfbvideosink->pixel_format = lc.pixelformat;
      GST_DEBUG_OBJECT (dfbvideosink, "layer %d now configured to %dx%d %s",
          dfbvideosink->layer_id, lc.width, lc.height,
          gst_dfbvideosink_get_format_name (lc.pixelformat));
    }
  }

  if (pixel_format != dfbvideosink->pixel_format) {
    GST_WARNING_OBJECT (dfbvideosink, "setcaps sent us a different pixel "
        "format %s", gst_dfbvideosink_get_format_name (pixel_format));
    goto beach;
  }

  dfbvideosink->video_width = video_width;
  dfbvideosink->video_height = video_height;

  if (dfbvideosink->pool) {
    if (gst_buffer_pool_is_active (dfbvideosink->pool))
      gst_buffer_pool_set_active (dfbvideosink->pool, FALSE);
    gst_object_unref (dfbvideosink->pool);
  }

  /* create a new buffer pool of DirectFB surface */
  dfbvideosink->pool = gst_dfb_buffer_pool_new (dfbvideosink);

  structure = gst_buffer_pool_get_config (dfbvideosink->pool);
  gst_buffer_pool_config_set_params (structure, caps, 0, 0, 0);
  if (!gst_buffer_pool_set_config (dfbvideosink->pool, structure)) {
    GST_WARNING_OBJECT (dfbvideosink,
        "failed to set buffer pool configuration");
    goto beach;
  }
  if (!gst_buffer_pool_set_active (dfbvideosink->pool, TRUE)) {
    GST_WARNING_OBJECT (dfbvideosink, "failed to activate buffer pool");
    goto beach;
  }

  result = TRUE;

beach:
  return result;

/* ERRORS */
wrong_aspect:
  {
    GST_INFO_OBJECT (dfbvideosink, "pixel aspect ratio does not match");
    return FALSE;
  }
}

static GstStateChangeReturn
gst_dfbvideosink_change_state (GstElement * element, GstStateChange transition)
{
  GstDfbVideoSink *dfbvideosink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  dfbvideosink = GST_DFBVIDEOSINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      dfbvideosink->running = TRUE;
      if (!dfbvideosink->setup) {
        if (!gst_dfbvideosink_setup (dfbvideosink)) {
          GST_DEBUG_OBJECT (dfbvideosink, "setup failed when changing state "
              "from NULL to READY");
          GST_ELEMENT_ERROR (dfbvideosink, RESOURCE, OPEN_WRITE,
              (NULL), ("Failed initializing DirectFB system"));
          return GST_STATE_CHANGE_FAILURE;
        }
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Blank surface if we have one */
      if (dfbvideosink->ext_surface) {
        dfbvideosink->ext_surface->Clear (dfbvideosink->ext_surface,
            0x00, 0x00, 0x00, 0xFF);
      }
      if (dfbvideosink->primary) {
        dfbvideosink->primary->Clear (dfbvideosink->primary, 0x00, 0x00,
            0x00, 0xFF);
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      dfbvideosink->fps_d = 0;
      dfbvideosink->fps_n = 0;
      dfbvideosink->video_width = 0;
      dfbvideosink->video_height = 0;
      if (dfbvideosink->pool)
        gst_buffer_pool_set_active (dfbvideosink->pool, FALSE);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      dfbvideosink->running = FALSE;
      if (dfbvideosink->setup) {
        gst_dfbvideosink_cleanup (dfbvideosink);
      }
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_dfbvideosink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstDfbVideoSink *dfbvideosink;

  dfbvideosink = GST_DFBVIDEOSINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (dfbvideosink->fps_n > 0) {
        *end =
            *start + (GST_SECOND * dfbvideosink->fps_d) / dfbvideosink->fps_n;
      }
    }
  }
}

static GstFlowReturn
gst_dfbvideosink_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
  GstDfbVideoSink *dfbvideosink = NULL;
  DFBResult res;
  GstVideoRectangle dst = { 0, };
  GstVideoRectangle src = { 0, };
  GstVideoRectangle result;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean mem_cpy = TRUE;
  GstMetaDfbSurface *meta;

  dfbvideosink = GST_DFBVIDEOSINK (bsink);

  if (!dfbvideosink->setup) {
    ret = GST_FLOW_EOS;
    goto beach;
  }

  meta = GST_META_DFBSURFACE_GET (buf);

  /* Is that a buffer we allocated ourselves ? */
  if (meta != NULL) {
    /* Does it have a surface ? */
    if (meta->surface) {
      mem_cpy = FALSE;
      GST_DEBUG_OBJECT (dfbvideosink, "we have a buffer (%p) we allocated "
          "ourselves and it has a surface, no memcpy then", buf);
    } else {
      /* No surface, that's a malloc */
      GST_DEBUG_OBJECT (dfbvideosink, "we have a buffer (%p) we allocated "
          "ourselves but it does not hold a surface", buf);
    }
  } else {
    /* Not our baby */
    GST_DEBUG_OBJECT (dfbvideosink, "we have a buffer (%p) we did not allocate",
        buf);
  }

  if (mem_cpy) {
    IDirectFBSurface *dest = NULL, *surface = NULL;
    guint8 *data;
    gint dest_pitch, line;
    GstStructure *structure;
    GstCaps *caps;
    gint plane;
    GstVideoInfo src_info;
    GstVideoFrame src_frame;
    const gchar *str;
    GstVideoFormat format;
    guint offset[GST_VIDEO_MAX_PLANES] = { 0 };
    guint stride[GST_VIDEO_MAX_PLANES] = { 0 };

    /* As we are not blitting no acceleration is possible. If the surface is
     * too small we do clipping, if it's too big we center. Theoretically as
     * we are using propose_allocation, there's a chance that we have been
     * able to do reverse caps negotiation */

    if (dfbvideosink->ext_surface) {
      surface = dfbvideosink->ext_surface;
      GST_DEBUG_OBJECT (dfbvideosink, "memcpy to an external surface "
          "subsurface (vsync %d)", dfbvideosink->vsync);
    } else {
      surface = dfbvideosink->primary;
      GST_DEBUG_OBJECT (dfbvideosink, "memcpy to a primary subsurface "
          "(vsync %d)", dfbvideosink->vsync);
    }

    /* Get the video frame geometry from the buffer caps */
    caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (bsink));
    structure = gst_caps_get_structure (caps, 0);
    if (structure) {
      gst_structure_get_int (structure, "width", &src.w);
      gst_structure_get_int (structure, "height", &src.h);
    } else {
      src.w = dfbvideosink->video_width;
      src.h = dfbvideosink->video_height;
    }
    gst_caps_unref (caps);
    surface->GetSize (surface, &dst.w, &dst.h);

    /* Center / Clip */
    gst_video_sink_center_rect (src, dst, &result, FALSE);

    res =
        surface->GetSubSurface (surface, (DFBRectangle *) (void *) &result,
        &dest);
    if (res != DFB_OK) {
      GST_WARNING_OBJECT (dfbvideosink, "failed when getting a sub surface");
      ret = GST_FLOW_EOS;
      goto beach;
    }

    /* If we are not using Flip we wait for VSYNC before blit */
    if (!dfbvideosink->backbuffer && dfbvideosink->vsync) {
      dfbvideosink->layer->WaitForSync (dfbvideosink->layer);
    }

    res = dest->Lock (dest, DSLF_WRITE, (void *) &data, &dest_pitch);
    if (res != DFB_OK) {
      GST_WARNING_OBJECT (dfbvideosink, "failed locking the external "
          "subsurface for writing");
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    caps = gst_pad_get_current_caps (GST_BASE_SINK_PAD (bsink));
    if (!gst_video_info_from_caps (&src_info, caps)) {
      GST_WARNING_OBJECT (dfbvideosink, "failed getting video info");
      gst_caps_unref (caps);
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    str = gst_structure_get_string (structure, "format");
    if (str == NULL) {
      GST_WARNING ("failed grabbing fourcc from caps %" GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);
      ret = GST_FLOW_ERROR;
      goto beach;
    }
    format = gst_video_format_from_string (str);

    gst_caps_unref (caps);

    if (!gst_video_frame_map (&src_frame, &src_info, buf, GST_MAP_READ)) {
      GST_WARNING_OBJECT (dfbvideosink, "failed mapping frame");
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    switch (format) {
      case GST_VIDEO_FORMAT_I420:
      case GST_VIDEO_FORMAT_YV12:
        offset[1] = dest_pitch * ((dfbvideosink->out_height - result.y) +
            result.y / 4);
        offset[2] = offset[1] + dest_pitch * dfbvideosink->out_height / 4;
        stride[0] = dest_pitch;
        stride[1] = stride[2] = dest_pitch / 2;
        break;
      case GST_VIDEO_FORMAT_NV12:
        offset[1] = dest_pitch * (dfbvideosink->out_height - result.y / 2);
        stride[0] = stride[1] = dest_pitch;
        break;
      default:
        stride[0] = dest_pitch;
        break;
    }

    line = 0;
    for (plane = 0; plane < src_info.finfo->n_planes; plane++) {
      guint plane_h;
      guint plane_line;
      guint8 *w_buf;
      guint size;

      w_buf = data + offset[plane];

      plane_h = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, plane);
      size = MIN (src_info.stride[plane], stride[plane]);

      /* Write each line respecting subsurface pitch */
      for (plane_line = 0; line < result.h || plane_line < plane_h;
          line++, plane_line++) {
        /* We do clipping */
        memcpy (w_buf, (gchar *) src_frame.data[plane] +
            (plane_line * src_info.stride[plane]), size);
        w_buf += stride[plane];
      }
    }

    gst_video_frame_unmap (&src_frame);

    dest->Unlock (dest);

    dest->Release (dest);

    if (dfbvideosink->backbuffer) {
      if (dfbvideosink->vsync) {
        surface->Flip (surface, NULL, DSFLIP_ONSYNC);
      } else {
        surface->Flip (surface, NULL, DSFLIP_NONE);
      }
    }
  } else {
    /* Else we will [Stretch]Blit to our primary */
    GST_DEBUG_OBJECT (dfbvideosink, "blitting to a primary surface (vsync %d)",
        dfbvideosink->vsync);

    src.w = GST_VIDEO_SINK_WIDTH (dfbvideosink);
    src.h = GST_VIDEO_SINK_HEIGHT (dfbvideosink);

    dfbvideosink->primary->GetSize (dfbvideosink->primary, &dst.w, &dst.h);

    /* Unlocking surface before blit */
    if (meta->locked) {
      meta->surface->Unlock (meta->surface);
      meta->locked = FALSE;
    }

    gst_video_sink_center_rect (src, dst, &result, dfbvideosink->hw_scaling);

    /* If we are not using Flip we wait for VSYNC before blit */
    if (!dfbvideosink->backbuffer && dfbvideosink->vsync) {
      dfbvideosink->layer->WaitForSync (dfbvideosink->layer);
    }

    if (dfbvideosink->hw_scaling) {
      dfbvideosink->primary->StretchBlit (dfbvideosink->primary,
          meta->surface, NULL, (DFBRectangle *) (void *) &result);
    } else {
      DFBRectangle clip;

      clip.x = clip.y = 0;
      clip.w = result.w;
      clip.h = result.h;
      dfbvideosink->primary->Blit (dfbvideosink->primary, meta->surface,
          &clip, result.x, result.y);
    }

    if (dfbvideosink->backbuffer) {
      if (dfbvideosink->vsync) {
        dfbvideosink->primary->Flip (dfbvideosink->primary, NULL,
            DSFLIP_ONSYNC);
      } else {
        dfbvideosink->primary->Flip (dfbvideosink->primary, NULL, DSFLIP_NONE);
      }
    }
  }

beach:
  return ret;
}

static void
gst_dfbvideosink_navigation_send_event (GstNavigation * navigation,
    GstEvent * event)
{
  GstDfbVideoSink *dfbvideosink = GST_DFBVIDEOSINK (navigation);
  GstVideoRectangle dst = { 0, };
  GstVideoRectangle src = { 0, };
  GstVideoRectangle result;
  double x, y, old_x, old_y;
  GstPad *pad = NULL;

  src.w = GST_VIDEO_SINK_WIDTH (dfbvideosink);
  src.h = GST_VIDEO_SINK_HEIGHT (dfbvideosink);
  dst.w = dfbvideosink->out_width;
  dst.h = dfbvideosink->out_height;
  gst_video_sink_center_rect (src, dst, &result, dfbvideosink->hw_scaling);

  event = gst_event_make_writable (event);

  /* Our coordinates can be wrong here if we centered the video */

  /* Converting pointer coordinates to the non scaled geometry */
  if (gst_navigation_event_get_coordinates (event, &old_x, &old_y)) {
    x = old_x;
    y = old_y;

    if (x >= result.x && x <= (result.x + result.w)) {
      x -= result.x;
      x *= dfbvideosink->video_width;
      x /= result.w;
    } else {
      x = 0;
    }
    if (y >= result.y && y <= (result.y + result.h)) {
      y -= result.y;
      y *= dfbvideosink->video_height;
      y /= result.h;
    } else {
      y = 0;
    }

    GST_DEBUG_OBJECT (dfbvideosink, "translated navigation event x "
        "coordinate from %f to %f", old_x, x);
    GST_DEBUG_OBJECT (dfbvideosink, "translated navigation event y "
        "coordinate from %fd to %fd", old_y, y);
    gst_navigation_event_set_coordinates (event, x, y);
  }

  pad = gst_pad_get_peer (GST_VIDEO_SINK_PAD (dfbvideosink));

  if (GST_IS_PAD (pad)) {
    if (!gst_pad_send_event (pad, gst_event_ref (event))) {
      /* If upstream didn't handle the event we'll post a message with it
       * for the application in case it wants to do something with it */
      gst_element_post_message (GST_ELEMENT_CAST (dfbvideosink),
          gst_navigation_message_new_event (GST_OBJECT_CAST (dfbvideosink),
              event));
    }
    gst_event_unref (event);
    gst_object_unref (pad);
  }
}

static void
gst_dfbvideosink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event_simple = gst_dfbvideosink_navigation_send_event;
}

static void
gst_dfbvideosink_update_colorbalance (GstDfbVideoSink * dfbvideosink)
{
  g_return_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink));

  if (dfbvideosink->layer) {
    DFBColorAdjustment cb_adjust;

    cb_adjust.flags = DCAF_NONE;

    if (dfbvideosink->brightness >= 0) {
      cb_adjust.flags |= DCAF_BRIGHTNESS;
    }
    if (dfbvideosink->contrast >= 0) {
      cb_adjust.flags |= DCAF_CONTRAST;
    }
    if (dfbvideosink->hue >= 0) {
      cb_adjust.flags |= DCAF_HUE;
    }
    if (dfbvideosink->saturation >= 0) {
      cb_adjust.flags |= DCAF_SATURATION;
    }

    cb_adjust.brightness = dfbvideosink->brightness;
    cb_adjust.contrast = dfbvideosink->contrast;
    cb_adjust.hue = dfbvideosink->hue;
    cb_adjust.saturation = dfbvideosink->saturation;

    GST_DEBUG_OBJECT (dfbvideosink, "updating colorbalance: flags %d "
        "brightness %d contrast %d hue %d saturation %d", cb_adjust.flags,
        cb_adjust.brightness, cb_adjust.contrast, cb_adjust.hue,
        cb_adjust.saturation);
    dfbvideosink->layer->SetColorAdjustment (dfbvideosink->layer, &cb_adjust);
  }
}

static const GList *
gst_dfbvideosink_colorbalance_list_channels (GstColorBalance * balance)
{
  GstDfbVideoSink *dfbvideosink = GST_DFBVIDEOSINK (balance);

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), NULL);

  return dfbvideosink->cb_channels;
}

static void
gst_dfbvideosink_colorbalance_set_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel, gint value)
{
  GstDfbVideoSink *dfbvideosink = GST_DFBVIDEOSINK (balance);

  g_return_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink));
  g_return_if_fail (channel->label != NULL);

  dfbvideosink->cb_changed = TRUE;

  if (g_ascii_strcasecmp (channel->label, "HUE") == 0) {
    dfbvideosink->hue = value;
  } else if (g_ascii_strcasecmp (channel->label, "SATURATION") == 0) {
    dfbvideosink->saturation = value;
  } else if (g_ascii_strcasecmp (channel->label, "CONTRAST") == 0) {
    dfbvideosink->contrast = value;
  } else if (g_ascii_strcasecmp (channel->label, "BRIGHTNESS") == 0) {
    dfbvideosink->brightness = value;
  } else {
    GST_WARNING_OBJECT (dfbvideosink, "got an unknown channel %s",
        channel->label);
    return;
  }

  gst_dfbvideosink_update_colorbalance (dfbvideosink);
}

static gint
gst_dfbvideosink_colorbalance_get_value (GstColorBalance * balance,
    GstColorBalanceChannel * channel)
{
  GstDfbVideoSink *dfbvideosink = GST_DFBVIDEOSINK (balance);
  gint value = 0;

  g_return_val_if_fail (GST_IS_DFBVIDEOSINK (dfbvideosink), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (g_ascii_strcasecmp (channel->label, "HUE") == 0) {
    value = dfbvideosink->hue;
  } else if (g_ascii_strcasecmp (channel->label, "SATURATION") == 0) {
    value = dfbvideosink->saturation;
  } else if (g_ascii_strcasecmp (channel->label, "CONTRAST") == 0) {
    value = dfbvideosink->contrast;
  } else if (g_ascii_strcasecmp (channel->label, "BRIGHTNESS") == 0) {
    value = dfbvideosink->brightness;
  } else {
    GST_WARNING_OBJECT (dfbvideosink, "got an unknown channel %s",
        channel->label);
  }

  return value;
}

static GstColorBalanceType
gst_dfbvideosink_colorbalance_get_balance_type (GstColorBalance * balance)
{
  return GST_COLOR_BALANCE_HARDWARE;
}

static void
gst_dfbvideosink_colorbalance_init (GstColorBalanceInterface * iface)
{
  iface->list_channels = gst_dfbvideosink_colorbalance_list_channels;
  iface->set_value = gst_dfbvideosink_colorbalance_set_value;
  iface->get_value = gst_dfbvideosink_colorbalance_get_value;
  iface->get_balance_type = gst_dfbvideosink_colorbalance_get_balance_type;
}

/* Properties */

static void
gst_dfbvideosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDfbVideoSink *dfbvideosink;

  g_return_if_fail (GST_IS_DFBVIDEOSINK (object));
  dfbvideosink = GST_DFBVIDEOSINK (object);

  switch (prop_id) {
    case ARG_SURFACE:
      dfbvideosink->ext_surface = g_value_get_pointer (value);
      break;
    case ARG_HUE:
      dfbvideosink->hue = g_value_get_int (value);
      dfbvideosink->cb_changed = TRUE;
      gst_dfbvideosink_update_colorbalance (dfbvideosink);
      break;
    case ARG_CONTRAST:
      dfbvideosink->contrast = g_value_get_int (value);
      dfbvideosink->cb_changed = TRUE;
      gst_dfbvideosink_update_colorbalance (dfbvideosink);
      break;
    case ARG_BRIGHTNESS:
      dfbvideosink->brightness = g_value_get_int (value);
      dfbvideosink->cb_changed = TRUE;
      gst_dfbvideosink_update_colorbalance (dfbvideosink);
      break;
    case ARG_SATURATION:
      dfbvideosink->saturation = g_value_get_int (value);
      dfbvideosink->cb_changed = TRUE;
      gst_dfbvideosink_update_colorbalance (dfbvideosink);
      break;
    case ARG_PIXEL_ASPECT_RATIO:
      g_free (dfbvideosink->par);
      dfbvideosink->par = g_new0 (GValue, 1);
      g_value_init (dfbvideosink->par, GST_TYPE_FRACTION);
      if (!g_value_transform (value, dfbvideosink->par)) {
        GST_WARNING_OBJECT (dfbvideosink, "Could not transform string to "
            "aspect ratio");
        gst_value_set_fraction (dfbvideosink->par, 1, 1);
      }
      GST_DEBUG_OBJECT (dfbvideosink, "set PAR to %d/%d",
          gst_value_get_fraction_numerator (dfbvideosink->par),
          gst_value_get_fraction_denominator (dfbvideosink->par));
      break;
    case ARG_VSYNC:
      dfbvideosink->vsync = g_value_get_boolean (value);
      break;
    case ARG_LAYER_MODE:
      dfbvideosink->layer_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dfbvideosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDfbVideoSink *dfbvideosink;

  g_return_if_fail (GST_IS_DFBVIDEOSINK (object));
  dfbvideosink = GST_DFBVIDEOSINK (object);

  switch (prop_id) {
    case ARG_HUE:
      g_value_set_int (value, dfbvideosink->hue);
      break;
    case ARG_CONTRAST:
      g_value_set_int (value, dfbvideosink->contrast);
      break;
    case ARG_BRIGHTNESS:
      g_value_set_int (value, dfbvideosink->brightness);
      break;
    case ARG_SATURATION:
      g_value_set_int (value, dfbvideosink->saturation);
      break;
    case ARG_PIXEL_ASPECT_RATIO:
      if (dfbvideosink->par)
        g_value_transform (dfbvideosink->par, value);
      break;
    case ARG_VSYNC:
      g_value_set_boolean (value, dfbvideosink->vsync);
      break;
    case ARG_LAYER_MODE:
      g_value_set_enum (value, dfbvideosink->layer_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dfbvideosink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstDfbVideoSink *dfbvideosink;
  GstBufferPool *pool;
  GstCaps *caps;
  gboolean need_pool;
  guint size = 0;

  dfbvideosink = GST_DFBVIDEOSINK (bsink);

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (!caps) {
    GST_WARNING_OBJECT (dfbvideosink, "Missing caps in allocation query.");
    return FALSE;
  }

  /* FIXME re-using buffer pool breaks renegotiation */
  if ((pool = dfbvideosink->pool))
    gst_object_ref (pool);

  if (pool != NULL) {
    GstCaps *pcaps;
    GstStructure *config;

    /* we had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    GST_DEBUG_OBJECT (dfbvideosink,
        "buffer pool configuration caps %" GST_PTR_FORMAT, pcaps);
    if (!gst_caps_is_equal (caps, pcaps)) {
      gst_structure_free (config);
      gst_object_unref (pool);
      GST_WARNING_OBJECT (dfbvideosink, "pool has different caps");
      return FALSE;
    }
    gst_structure_free (config);
  } else {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps)) {
      GST_WARNING_OBJECT (dfbvideosink,
          "Invalid video caps in allocation query");
      return FALSE;
    }

    size = info.size;
  }

  gst_query_add_allocation_pool (query, pool, size, 1, 0);

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  if (pool)
    gst_object_unref (pool);

  return TRUE;
}

/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */
static void
gst_dfbvideosink_finalize (GObject * object)
{
  GstDfbVideoSink *dfbvideosink;

  dfbvideosink = GST_DFBVIDEOSINK (object);

  if (dfbvideosink->par) {
    g_free (dfbvideosink->par);
    dfbvideosink->par = NULL;
  }
  if (dfbvideosink->setup) {
    gst_dfbvideosink_cleanup (dfbvideosink);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dfbvideosink_init (GstDfbVideoSink * dfbvideosink)
{
  dfbvideosink->pool = NULL;

  dfbvideosink->video_height = dfbvideosink->out_height = 0;
  dfbvideosink->video_width = dfbvideosink->out_width = 0;
  dfbvideosink->fps_d = 0;
  dfbvideosink->fps_n = 0;

  dfbvideosink->dfb = NULL;
  dfbvideosink->vmodes = NULL;
  dfbvideosink->layer_id = -1;
  dfbvideosink->layer = NULL;
  dfbvideosink->primary = NULL;
  dfbvideosink->event_buffer = NULL;
  dfbvideosink->event_thread = NULL;

  dfbvideosink->ext_surface = NULL;

  dfbvideosink->pixel_format = DSPF_UNKNOWN;

  dfbvideosink->hw_scaling = FALSE;
  dfbvideosink->backbuffer = FALSE;
  dfbvideosink->vsync = TRUE;
  dfbvideosink->setup = FALSE;
  dfbvideosink->running = FALSE;

  dfbvideosink->cb_channels = NULL;
  dfbvideosink->brightness = -1;
  dfbvideosink->contrast = -1;
  dfbvideosink->hue = -1;
  dfbvideosink->saturation = -1;

  dfbvideosink->par = NULL;

  dfbvideosink->layer_mode = DEFAULT_LAYER_MODE;
}

static void
gst_dfbvideosink_class_init (GstDfbVideoSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_dfbvideosink_finalize;
  gobject_class->set_property = gst_dfbvideosink_set_property;
  gobject_class->get_property = gst_dfbvideosink_get_property;

  g_object_class_install_property (gobject_class, ARG_SURFACE,
      g_param_spec_pointer ("surface", "Surface",
          "The target surface for video",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_CONTRAST,
      g_param_spec_int ("contrast", "Contrast", "The contrast of the video",
          0x0000, 0xFFFF, 0x8000, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_BRIGHTNESS,
      g_param_spec_int ("brightness", "Brightness",
          "The brightness of the video", 0x0000, 0xFFFF, 0x8000,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_HUE,
      g_param_spec_int ("hue", "Hue", "The hue of the video", 0x0000, 0xFFFF,
          0x8000, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_SATURATION,
      g_param_spec_int ("saturation", "Saturation",
          "The saturation of the video", 0x0000, 0xFFFF, 0x8000,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_VSYNC,
      g_param_spec_boolean ("vsync", "Vertical synchronisation",
          "Wait for next vertical sync to draw frames", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_LAYER_MODE,
      g_param_spec_enum ("layer-mode",
          "The layer cooperative level (administrative or exclusive)",
          "The cooperative level handling the access permission (set this to "
          "'administrative' when the cursor is required)",
          gst_dfbvideosink_layer_mode_get_type (), DEFAULT_LAYER_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (gst_dfbvideosink_layer_mode_get_type (), 0);

  gst_element_class_set_static_metadata (gstelement_class,
      "DirectFB video sink", "Sink/Video", "A DirectFB based videosink",
      "Julien Moutte <julien@moutte.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dfbvideosink_sink_template_factory);

  gstelement_class->change_state = gst_dfbvideosink_change_state;

  gstbasesink_class->get_caps = gst_dfbvideosink_getcaps;
  gstbasesink_class->set_caps = gst_dfbvideosink_setcaps;
  gstbasesink_class->get_times = gst_dfbvideosink_get_times;
  gstbasesink_class->preroll = gst_dfbvideosink_show_frame;
  gstbasesink_class->render = gst_dfbvideosink_show_frame;
  gstbasesink_class->propose_allocation = gst_dfbvideosink_propose_allocation;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (dfbvideosink, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    directfb,
    "DirectFB video output plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
