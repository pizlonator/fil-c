/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2011 Igalia S.L.
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
 * SECTION:element-debugspy
 * @title: debugspy
 *
 * A spy element that can provide information on buffers going through it, with
 * bus messages.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -m videotestsrc ! debugspy ! fakesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstdebugutilsbadelements.h"
#include "gstdebugspy.h"

GST_DEBUG_CATEGORY_STATIC (gst_debug_spy_debug);
#define GST_CAT_DEFAULT gst_debug_spy_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_CHECKSUM_TYPE
};

/* create a GType for GChecksumType */
#define GST_DEBUG_SPY_CHECKSUM_TYPE (gst_debug_spy_checksum_get_type())
static GType
gst_debug_spy_checksum_get_type (void)
{
  static GType checksum_type = 0;

  static const GEnumValue checksum_values[] = {
    {G_CHECKSUM_MD5, "Use the MD5 hashing algorithm", "md5"},
    {G_CHECKSUM_SHA1, "Use the SHA-1 hashing algorithm", "sha1"},
    {G_CHECKSUM_SHA256, "Use the SHA-256 hashing algorithm", "sha256"},
    {0, NULL, NULL}
  };

  if (!checksum_type)
    checksum_type = g_enum_register_static ("GChecksumType", checksum_values);

  return checksum_type;
}

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

G_DEFINE_TYPE (GstDebugSpy, gst_debug_spy, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (debugspy, "debugspy",
    GST_RANK_NONE, gst_debug_spy_get_type ());

static void gst_debug_spy_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_debug_spy_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_debug_spy_transform_ip (GstBaseTransform * transform,
    GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the debugspy's class */
static void
gst_debug_spy_class_init (GstDebugSpyClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *base_transform_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  base_transform_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = gst_debug_spy_set_property;
  gobject_class->get_property = gst_debug_spy_get_property;

  base_transform_class->passthrough_on_same_caps = TRUE;
  base_transform_class->transform_ip = gst_debug_spy_transform_ip;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHECKSUM_TYPE,
      g_param_spec_enum ("checksum-type", "Checksum TYpe",
          "Checksum algorithm to use", GST_DEBUG_SPY_CHECKSUM_TYPE,
          G_CHECKSUM_SHA1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "DebugSpy",
      "Filter/Analyzer/Debug",
      "DebugSpy provides information on buffers with bus messages",
      "Guillaume Emont <gemont@igalia.com>");

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  GST_DEBUG_CATEGORY_INIT (gst_debug_spy_debug, "debugspy", 0, "debugspy");
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_debug_spy_init (GstDebugSpy * debugspy)
{
  debugspy->silent = FALSE;
  debugspy->checksum_type = G_CHECKSUM_SHA1;
}

static void
gst_debug_spy_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDebugSpy *debugspy = GST_DEBUGSPY (object);

  switch (prop_id) {
    case PROP_SILENT:
      debugspy->silent = g_value_get_boolean (value);
      break;
    case PROP_CHECKSUM_TYPE:
      debugspy->checksum_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_debug_spy_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDebugSpy *debugspy = GST_DEBUGSPY (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, debugspy->silent);
      break;
    case PROP_CHECKSUM_TYPE:
      g_value_set_enum (value, debugspy->checksum_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static GstFlowReturn
gst_debug_spy_transform_ip (GstBaseTransform * transform, GstBuffer * buf)
{
  GstDebugSpy *debugspy = GST_DEBUGSPY (transform);

  if (debugspy->silent == FALSE) {
    gchar *checksum;
    GstMessage *message;
    GstStructure *message_structure;
    GstMapInfo map;
    GstCaps *caps;

    gst_buffer_map (buf, &map, GST_MAP_READ);
    checksum = g_compute_checksum_for_data (debugspy->checksum_type,
        map.data, map.size);

    caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SRC_PAD (transform));
    message_structure = gst_structure_new ("buffer",
        "checksum", G_TYPE_STRING, checksum,
        "timestamp", GST_TYPE_CLOCK_TIME, GST_BUFFER_TIMESTAMP (buf),
        "duration", GST_TYPE_CLOCK_TIME, GST_BUFFER_DURATION (buf),
        "offset", G_TYPE_UINT64, GST_BUFFER_OFFSET (buf),
        "offset_end", G_TYPE_UINT64, GST_BUFFER_OFFSET_END (buf),
        "size", G_TYPE_UINT, map.size, "caps", GST_TYPE_CAPS, caps, NULL);
    if (caps)
      gst_caps_unref (caps);

    g_free (checksum);
    gst_buffer_unmap (buf, &map);

    message =
        gst_message_new_element (GST_OBJECT (transform), message_structure);

    gst_element_post_message (GST_ELEMENT (transform), message);

  }

  return GST_FLOW_OK;
}
