/* GStreamer
 * Copyright (C) <2007> Thijs Vermeir <thijsvermeir@gmail.com>
 * Copyright (C) <2006> Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>
 * Copyright (C) <2004> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrfbsrc.h"
#include "gstrfb-utils.h"

#include <gst/video/video.h>
#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#define DEFAULT_PROP_HOST             "127.0.0.1"
#define DEFAULT_PROP_PORT             5900
#define DEFAULT_PROP_URI              "rfb://"DEFAULT_PROP_HOST":"G_STRINGIFY(DEFAULT_PROP_PORT)

enum
{
  PROP_0,
  PROP_URI,
  PROP_HOST,
  PROP_PORT,
  PROP_VERSION,
  PROP_PASSWORD,
  PROP_OFFSET_X,
  PROP_OFFSET_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_INCREMENTAL,
  PROP_USE_COPYRECT,
  PROP_SHARED,
  PROP_VIEWONLY
};

GST_DEBUG_CATEGORY_STATIC (rfbsrc_debug);
GST_DEBUG_CATEGORY (rfbdecoder_debug);
#define GST_CAT_DEFAULT rfbsrc_debug

static GstStaticPadTemplate gst_rfb_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGB")
        "; " GST_VIDEO_CAPS_MAKE ("BGR")
        "; " GST_VIDEO_CAPS_MAKE ("RGBx")
        "; " GST_VIDEO_CAPS_MAKE ("BGRx")
        "; " GST_VIDEO_CAPS_MAKE ("xRGB")
        "; " GST_VIDEO_CAPS_MAKE ("xBGR")));

static void gst_rfb_src_finalize (GObject * object);
static void gst_rfb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rfb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rfb_src_negotiate (GstBaseSrc * bsrc);
static gboolean gst_rfb_src_stop (GstBaseSrc * bsrc);
static gboolean gst_rfb_src_event (GstBaseSrc * bsrc, GstEvent * event);
static gboolean gst_rfb_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_rfb_src_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query);
static GstFlowReturn gst_rfb_src_fill (GstPushSrc * psrc, GstBuffer * outbuf);

static void gst_rfb_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static gboolean
gst_rfb_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error);

#define gst_rfb_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRfbSrc, gst_rfb_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rfb_src_uri_handler_init));
GST_ELEMENT_REGISTER_DEFINE (rfbsrc, "rfbsrc", GST_RANK_NONE, GST_TYPE_RFB_SRC);

static void
gst_rfb_src_class_init (GstRfbSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstElementClass *gstelement_class;
  GstPushSrcClass *gstpushsrc_class;


  GST_DEBUG_CATEGORY_INIT (rfbsrc_debug, "rfbsrc", 0, "rfb src element");
  GST_DEBUG_CATEGORY_INIT (rfbdecoder_debug, "rfbdecoder", 0, "rfb decoder");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->finalize = gst_rfb_src_finalize;
  gobject_class->set_property = gst_rfb_src_set_property;
  gobject_class->get_property = gst_rfb_src_get_property;

  /**
   * GstRfbSrc:uri:
   *
   * uri to an RFB from. All GStreamer parameters can be
   * encoded in the URI, this URI format is RFC compliant.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI in the form of rfb://host:port?query", DEFAULT_PROP_URI,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host to connect to", "Host to connect to",
          DEFAULT_PROP_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "Port",
          1, 65535, DEFAULT_PROP_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VERSION,
      g_param_spec_string ("version", "RFB protocol version",
          "RFB protocol version", "3.3",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PASSWORD,
      g_param_spec_string ("password", "Password for authentication",
          "Password for authentication", "",
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSET_X,
      g_param_spec_int ("offset-x", "x offset for screen scrapping",
          "x offset for screen scrapping", 0, 65535, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OFFSET_Y,
      g_param_spec_int ("offset-y", "y offset for screen scrapping",
          "y offset for screen scrapping", 0, 65535, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "width of screen", "width of screen", 0, 65535,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height of screen", "height of screen", 0,
          65535, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INCREMENTAL,
      g_param_spec_boolean ("incremental", "Incremental updates",
          "Incremental updates", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_COPYRECT,
      g_param_spec_boolean ("use-copyrect", "Use copyrect encoding",
          "Use copyrect encoding", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SHARED,
      g_param_spec_boolean ("shared", "Share desktop with other clients",
          "Share desktop with other clients", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_VIEWONLY,
      g_param_spec_boolean ("view-only", "Only view the desktop",
          "only view the desktop", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesrc_class->negotiate = GST_DEBUG_FUNCPTR (gst_rfb_src_negotiate);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_rfb_src_stop);
  gstbasesrc_class->event = GST_DEBUG_FUNCPTR (gst_rfb_src_event);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_rfb_src_unlock);
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_rfb_src_fill);
  gstbasesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_rfb_src_decide_allocation);

  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_rfb_src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Rfb source",
      "Source/Video",
      "Creates a rfb video stream",
      "David A. Schleef <ds@schleef.org>, "
      "Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>, "
      "Thijs Vermeir <thijsvermeir@gmail.com>");
}

static void
gst_rfb_src_init (GstRfbSrc * src)
{
  GstBaseSrc *bsrc = GST_BASE_SRC (src);

  gst_pad_use_fixed_caps (GST_BASE_SRC_PAD (bsrc));
  gst_base_src_set_live (bsrc, TRUE);
  gst_base_src_set_format (bsrc, GST_FORMAT_TIME);

  src->uri = gst_uri_from_string (DEFAULT_PROP_URI);
  src->host = g_strdup (DEFAULT_PROP_HOST);
  src->port = DEFAULT_PROP_PORT;
  src->version_major = 3;
  src->version_minor = 3;

  src->incremental_update = TRUE;

  src->view_only = FALSE;

  src->decoder = rfb_decoder_new ();
}

static void
gst_rfb_src_finalize (GObject * object)
{
  GstRfbSrc *src = GST_RFB_SRC (object);

  if (src->uri)
    gst_uri_unref (src->uri);

  g_free (src->host);

  if (src->decoder) {
    rfb_decoder_free (src->decoder);
    src->decoder = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_rfb_property_set_version (GstRfbSrc * src, gchar * value)
{
  gchar *major;
  gchar *minor;

  g_return_if_fail (src != NULL);
  g_return_if_fail (value != NULL);

  major = g_strdup (value);
  minor = g_strrstr (value, ".");

  g_return_if_fail (minor != NULL);

  *minor++ = 0;

  g_return_if_fail (g_ascii_isdigit (*major) == TRUE);
  g_return_if_fail (g_ascii_isdigit (*minor) == TRUE);

  src->version_major = g_ascii_digit_value (*major);
  src->version_minor = g_ascii_digit_value (*minor);

  GST_DEBUG ("Version major : %d", src->version_major);
  GST_DEBUG ("Version minor : %d", src->version_minor);

  g_free (major);
  g_free (value);
}

static gchar *
gst_rfb_property_get_version (GstRfbSrc * src)
{
  return g_strdup_printf ("%d.%d", src->version_major, src->version_minor);
}

static void
gst_rfb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRfbSrc *src = GST_RFB_SRC (object);

  switch (prop_id) {
    case PROP_URI:{
      const gchar *str_uri = g_value_get_string (value);

      gst_rfb_src_uri_set_uri ((GstURIHandler *) src, str_uri, NULL);
      break;
    }
    case PROP_HOST:
      src->host = g_value_dup_string (value);;
      break;
    case PROP_PORT:
      src->port = g_value_get_int (value);
      break;
    case PROP_VERSION:
      gst_rfb_property_set_version (src, g_value_dup_string (value));
      break;
    case PROP_PASSWORD:
      g_free (src->decoder->password);
      src->decoder->password = g_value_dup_string (value);
      break;
    case PROP_OFFSET_X:
      src->decoder->offset_x = g_value_get_int (value);
      break;
    case PROP_OFFSET_Y:
      src->decoder->offset_y = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      src->decoder->rect_width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      src->decoder->rect_height = g_value_get_int (value);
      break;
    case PROP_INCREMENTAL:
      src->incremental_update = g_value_get_boolean (value);
      break;
    case PROP_USE_COPYRECT:
      src->decoder->use_copyrect = g_value_get_boolean (value);
      break;
    case PROP_SHARED:
      src->decoder->shared_flag = g_value_get_boolean (value);
      break;
    case PROP_VIEWONLY:
      src->view_only = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_rfb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRfbSrc *src = GST_RFB_SRC (object);
  gchar *version;

  switch (prop_id) {
    case PROP_URI:
      GST_OBJECT_LOCK (object);
      if (src->uri)
        g_value_take_string (value, gst_uri_to_string (src->uri));
      else
        g_value_set_string (value, NULL);
      GST_OBJECT_UNLOCK (object);
      break;
    case PROP_HOST:
      g_value_set_string (value, src->host);
      break;
    case PROP_PORT:
      g_value_set_int (value, src->port);
      break;
    case PROP_VERSION:
      version = gst_rfb_property_get_version (src);
      g_value_set_string (value, version);
      g_free (version);
      break;
    case PROP_OFFSET_X:
      g_value_set_int (value, src->decoder->offset_x);
      break;
    case PROP_OFFSET_Y:
      g_value_set_int (value, src->decoder->offset_y);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->decoder->rect_width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, src->decoder->rect_height);
      break;
    case PROP_INCREMENTAL:
      g_value_set_boolean (value, src->incremental_update);
      break;
    case PROP_USE_COPYRECT:
      g_value_set_boolean (value, src->decoder->use_copyrect);
      break;
    case PROP_SHARED:
      g_value_set_boolean (value, src->decoder->shared_flag);
      break;
    case PROP_VIEWONLY:
      g_value_set_boolean (value, src->view_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_rfb_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstBufferPool *pool = NULL;
  guint size, min = 1, max = 0;
  GstStructure *config;
  GstCaps *caps;
  GstVideoInfo info;
  gboolean ret;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps || !gst_video_info_from_caps (&info, caps))
    return FALSE;

  while (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    /* TODO We restrict to the exact size as we don't support strides or
     * special padding */
    if (size == info.size)
      break;

    gst_query_remove_nth_allocation_pool (query, 0);
    gst_object_unref (pool);
    pool = NULL;
  }

  if (pool == NULL) {
    /* we did not get a pool, make one ourselves then */
    pool = gst_video_buffer_pool_new ();
    size = info.size;
    min = 1;
    max = 0;

    if (gst_query_get_n_allocation_pools (query) > 0)
      gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
    else
      gst_query_add_allocation_pool (query, pool, size, min, max);
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);

  ret = gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return ret;
}

static gboolean
gst_rfb_src_negotiate (GstBaseSrc * bsrc)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);
  RfbDecoder *decoder;
  GstCaps *caps;
  GstVideoInfo vinfo;
  GstVideoFormat vformat;
  guint32 red_mask, green_mask, blue_mask;
  gchar *stream_id = NULL;
  GstEvent *stream_start = NULL;

  decoder = src->decoder;

  if (decoder->inited)
    return TRUE;

  GST_DEBUG_OBJECT (src, "connecting to host %s on port %d",
      src->host, src->port);
  if (!rfb_decoder_connect_tcp (decoder, src->host, src->port)) {
    if (decoder->error != NULL) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          ("Could not connect to VNC server %s on port %d: %s", src->host,
              src->port, decoder->error->message), (NULL));
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,
          ("Could not connect to VNC server %s on port %d", src->host,
              src->port), (NULL));
    }
    return FALSE;
  }

  while (!decoder->inited) {
    if (!rfb_decoder_iterate (decoder)) {
      if (decoder->error != NULL) {
        GST_ELEMENT_ERROR (src, RESOURCE, READ,
            ("Failed to setup VNC connection to host %s on port %d: %s",
                src->host, src->port, decoder->error->message), (NULL));
      } else {
        GST_ELEMENT_ERROR (src, RESOURCE, READ,
            ("Failed to setup VNC connection to host %s on port %d", src->host,
                src->port), (NULL));
      }
      return FALSE;
    }
  }

  stream_id = gst_pad_create_stream_id_printf (GST_BASE_SRC_PAD (bsrc),
      GST_ELEMENT (src), "%s:%d", src->host, src->port);
  stream_start = gst_event_new_stream_start (stream_id);
  g_free (stream_id);
  gst_pad_push_event (GST_BASE_SRC_PAD (bsrc), stream_start);

  decoder->rect_width =
      (decoder->rect_width ? decoder->rect_width : decoder->width);
  decoder->rect_height =
      (decoder->rect_height ? decoder->rect_height : decoder->height);

  decoder->decoder_private = src;

  /* calculate some many used values */
  decoder->bytespp = decoder->bpp / 8;
  decoder->line_size = decoder->rect_width * decoder->bytespp;

  GST_DEBUG_OBJECT (src, "setting caps width to %d and height to %d",
      decoder->rect_width, decoder->rect_height);

  red_mask = decoder->red_max << decoder->red_shift;
  green_mask = decoder->green_max << decoder->green_shift;
  blue_mask = decoder->blue_max << decoder->blue_shift;

  vformat = gst_video_format_from_masks (decoder->depth, decoder->bpp,
      decoder->big_endian ? G_BIG_ENDIAN : G_LITTLE_ENDIAN,
      red_mask, green_mask, blue_mask, 0);

  gst_video_info_init (&vinfo);

  gst_video_info_set_format (&vinfo, vformat, decoder->rect_width,
      decoder->rect_height);

  decoder->frame = g_malloc (vinfo.size);
  if (decoder->use_copyrect)
    decoder->prev_frame = g_malloc (vinfo.size);

  caps = gst_video_info_to_caps (&vinfo);

  gst_base_src_set_caps (bsrc, caps);

  gst_caps_unref (caps);

  return TRUE;
}

static gboolean
gst_rfb_src_stop (GstBaseSrc * bsrc)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);

  rfb_decoder_disconnect (src->decoder);

  if (src->decoder->frame) {
    g_free (src->decoder->frame);
    src->decoder->frame = NULL;
  }

  if (src->decoder->prev_frame) {
    g_free (src->decoder->prev_frame);
    src->decoder->prev_frame = NULL;
  }

  return TRUE;
}

static GstFlowReturn
gst_rfb_src_fill (GstPushSrc * psrc, GstBuffer * outbuf)
{
  GstRfbSrc *src = GST_RFB_SRC (psrc);
  RfbDecoder *decoder = src->decoder;
  GstMapInfo info;

  rfb_decoder_send_update_request (decoder, src->incremental_update,
      decoder->offset_x, decoder->offset_y, decoder->rect_width,
      decoder->rect_height);

  while (decoder->state != NULL) {
    if (!rfb_decoder_iterate (decoder)) {
      if (decoder->error != NULL) {
        GST_ELEMENT_ERROR (src, RESOURCE, READ,
            ("Error on VNC connection to host %s on port %d: %s",
                src->host, src->port, decoder->error->message), (NULL));
      } else {
        GST_ELEMENT_ERROR (src, RESOURCE, READ,
            ("Error on setup VNC connection to host %s on port %d", src->host,
                src->port), (NULL));
      }
      return GST_FLOW_ERROR;
    }
  }

  if (!gst_buffer_map (outbuf, &info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE,
        ("Could not map the output frame"), (NULL));
    return GST_FLOW_ERROR;
  }

  memcpy (info.data, decoder->frame, info.size);

  GST_BUFFER_PTS (outbuf) =
      gst_clock_get_time (GST_ELEMENT_CLOCK (src)) -
      GST_ELEMENT_CAST (src)->base_time;

  gst_buffer_unmap (outbuf, &info);

  return GST_FLOW_OK;
}

static gboolean
gst_rfb_src_event (GstBaseSrc * bsrc, GstEvent * event)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);
  gdouble x, y;
  gint button;
  GstNavigationEventType event_type;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:

      /* if in view_only mode ignore the navigation event */
      if (src->view_only)
        break;

      event_type = gst_navigation_event_get_type (event);
      switch (event_type) {
#ifdef HAVE_X11
        case GST_NAVIGATION_EVENT_KEY_PRESS:
        case GST_NAVIGATION_EVENT_KEY_RELEASE:{
          const gchar *key;
          KeySym key_sym;

          gst_navigation_event_parse_key_event (event, &key);
          key_sym = XStringToKeysym (key);

          if (key_sym != NoSymbol)
            rfb_decoder_send_key_event (src->decoder, key_sym,
                event_type == GST_NAVIGATION_EVENT_KEY_PRESS);
          break;
        }
#endif
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:{
          gst_navigation_event_parse_mouse_button_event (event,
              &button, &x, &y);
          x += src->decoder->offset_x;
          y += src->decoder->offset_y;
          src->button_mask |= (1 << (button - 1));
          GST_LOG_OBJECT (src, "sending mouse-button-press event "
              "button_mask=%d, x=%d, y=%d",
              src->button_mask, (gint) x, (gint) y);
          rfb_decoder_send_pointer_event (src->decoder, src->button_mask,
              (gint) x, (gint) y);
          break;
        }
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:{
          gst_navigation_event_parse_mouse_button_event (event,
              &button, &x, &y);
          x += src->decoder->offset_x;
          y += src->decoder->offset_y;
          src->button_mask &= ~(1 << (button - 1));
          GST_LOG_OBJECT (src, "sending mouse-button-release event "
              "button_mask=%d, x=%d, y=%d",
              src->button_mask, (gint) x, (gint) y);
          rfb_decoder_send_pointer_event (src->decoder, src->button_mask,
              (gint) x, (gint) y);
          break;
        }
        case GST_NAVIGATION_EVENT_MOUSE_MOVE:{
          gst_navigation_event_parse_mouse_move_event (event, &x, &y);
          x += src->decoder->offset_x;
          y += src->decoder->offset_y;
          GST_LOG_OBJECT (src, "sending mouse-move event "
              "button_mask=%d, x=%d, y=%d",
              src->button_mask, (gint) x, (gint) y);
          rfb_decoder_send_pointer_event (src->decoder, src->button_mask,
              (gint) x, (gint) y);
          break;
        }
        default:
          break;
      }
      break;
    default:
      break;
  }

  return TRUE;
}

static gboolean
gst_rfb_src_unlock (GstBaseSrc * bsrc)
{
  GstRfbSrc *src = GST_RFB_SRC (bsrc);
  g_cancellable_cancel (src->decoder->cancellable);
  return TRUE;
}

static GstURIType
gst_rfb_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rfb_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rfb", NULL };

  return protocols;
}

static gchar *
gst_rfb_src_uri_get_uri (GstURIHandler * handler)
{
  GstRfbSrc *src = (GstRfbSrc *) handler;
  gchar *str_uri = NULL;

  GST_OBJECT_LOCK (src);
  str_uri = gst_uri_to_string (src->uri);
  GST_OBJECT_UNLOCK (src);

  return str_uri;
}

static gboolean
gst_rfb_src_uri_set_uri (GstURIHandler * handler, const gchar * str_uri,
    GError ** error)
{
  GstRfbSrc *src = (GstRfbSrc *) handler;
  GstUri *uri = NULL;
  const gchar *userinfo;

  g_return_val_if_fail (str_uri != NULL, FALSE);

  if (GST_STATE (src) >= GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        _("Changing the URI on rfbsrc when it is running is not supported"));
    GST_ERROR_OBJECT (src,
        "Changing the URI on rfbsrc when it is running is not supported");
    return FALSE;
  }

  if (!(uri = gst_uri_from_string (str_uri))) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        _("Invalid URI: %s"), str_uri);
    GST_ERROR_OBJECT (src, "Invalid URI: %s", str_uri);
    return FALSE;
  }

  if (g_strcmp0 (gst_uri_get_scheme (uri), "rfb") != 0) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        _("Invalid scheme in uri (needs to be rfb): %s"), str_uri);
    GST_ERROR_OBJECT (src, "Invalid scheme in uri (needs to be rfb): %s",
        str_uri);
    gst_uri_unref (uri);
    return FALSE;
  }

  /* Recursive set to src, do not use the same lock in all property
   * setters. */
  g_object_set (src, "host", gst_uri_get_host (uri), NULL);
  g_object_set (src, "port", gst_uri_get_port (uri), NULL);

  userinfo = gst_uri_get_userinfo (uri);
  if (userinfo) {
    gchar *pass;
    gchar **split = g_strsplit (userinfo, ":", 2);

    if (!split || !split[0] || !split[1]) {
      g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
          _("Failed to parse username:password data"));
      GST_ERROR_OBJECT (src, "Failed to parse username:password data");
      g_strfreev (split);
      gst_uri_unref (uri);
      return FALSE;
    }

    if (g_strrstr (split[1], ":") != NULL)
      GST_WARNING_OBJECT (src,
          "userinfo %s contains more than one ':', will "
          "assume that the first ':' delineates user:pass. You should escape "
          "the user and pass before adding to the URI.", userinfo);

    pass = g_uri_unescape_string (split[1], NULL);
    g_strfreev (split);

    g_object_set (src, "password", pass, NULL);
    g_free (pass);
  }

  /* Only save URI once it is accepted */
  GST_OBJECT_LOCK (src);
  if (src->uri)
    gst_uri_unref (src->uri);
  src->uri = gst_uri_ref (uri);
  GST_OBJECT_UNLOCK (src);

  gst_rfb_utils_set_properties_from_uri_query (G_OBJECT (src), uri);

  gst_uri_unref (uri);

  return TRUE;
}

static void
gst_rfb_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rfb_src_uri_get_type;
  iface->get_protocols = gst_rfb_src_uri_get_protocols;
  iface->get_uri = gst_rfb_src_uri_get_uri;
  iface->set_uri = gst_rfb_src_uri_set_uri;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (rfbsrc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rfbsrc,
    "Connects to a VNC server and decodes RFB stream",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
