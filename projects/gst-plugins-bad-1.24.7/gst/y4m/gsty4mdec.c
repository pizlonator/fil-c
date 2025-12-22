/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
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
 * SECTION:element-y4mdec
 * @title: gsty4mdec
 *
 * The gsty4mdec element decodes uncompressed video in YUV4MPEG format.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=file.y4m ! y4mdec ! xvimagesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gsty4mdec.h"

#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 32768

GST_DEBUG_CATEGORY (y4mdec_debug);
#define GST_CAT_DEFAULT y4mdec_debug

/* prototypes */


static void gst_y4m_dec_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_y4m_dec_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_y4m_dec_dispose (GObject * object);
static void gst_y4m_dec_finalize (GObject * object);

static GstFlowReturn gst_y4m_dec_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_y4m_dec_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static gboolean gst_y4m_dec_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_y4m_dec_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static GstStateChangeReturn
gst_y4m_dec_change_state (GstElement * element, GstStateChange transition);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_y4m_dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-yuv4mpeg, y4mversion=2")
    );

static GstStaticPadTemplate gst_y4m_dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ \
        I420,Y41B,Y42B,Y444,                 \
        I420_10LE,I422_10LE,Y444_10LE,       \
        I420_12LE,I422_12LE,Y444_12LE,       \
        Y444_16LE,GRAY8,GRAY16_LE            \
    }")));

/* class initialization */
#define gst_y4m_dec_parent_class parent_class
G_DEFINE_TYPE (GstY4mDec, gst_y4m_dec, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (y4mdec, "y4mdec", GST_RANK_SECONDARY,
    gst_y4m_dec_get_type (), GST_DEBUG_CATEGORY_INIT (y4mdec_debug, "y4mdec", 0,
        "y4mdec element"));
static void
gst_y4m_dec_class_init (GstY4mDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_y4m_dec_set_property;
  gobject_class->get_property = gst_y4m_dec_get_property;
  gobject_class->dispose = gst_y4m_dec_dispose;
  gobject_class->finalize = gst_y4m_dec_finalize;

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_y4m_dec_change_state);

  gst_element_class_add_static_pad_template (element_class,
      &gst_y4m_dec_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_y4m_dec_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "YUV4MPEG demuxer/decoder", "Codec/Demuxer",
      "Demuxes/decodes YUV4MPEG streams", "David Schleef <ds@schleef.org>");
}

static void
gst_y4m_dec_init (GstY4mDec * y4mdec)
{
  y4mdec->adapter = gst_adapter_new ();

  y4mdec->sinkpad =
      gst_pad_new_from_static_template (&gst_y4m_dec_sink_template, "sink");
  gst_pad_set_event_function (y4mdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_y4m_dec_sink_event));
  gst_pad_set_chain_function (y4mdec->sinkpad,
      GST_DEBUG_FUNCPTR (gst_y4m_dec_chain));
  gst_element_add_pad (GST_ELEMENT (y4mdec), y4mdec->sinkpad);

  y4mdec->srcpad = gst_pad_new_from_static_template (&gst_y4m_dec_src_template,
      "src");
  gst_pad_set_event_function (y4mdec->srcpad,
      GST_DEBUG_FUNCPTR (gst_y4m_dec_src_event));
  gst_pad_set_query_function (y4mdec->srcpad,
      GST_DEBUG_FUNCPTR (gst_y4m_dec_src_query));
  gst_pad_use_fixed_caps (y4mdec->srcpad);
  gst_element_add_pad (GST_ELEMENT (y4mdec), y4mdec->srcpad);

}

void
gst_y4m_dec_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  g_return_if_fail (GST_IS_Y4M_DEC (object));

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_y4m_dec_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  g_return_if_fail (GST_IS_Y4M_DEC (object));

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_y4m_dec_dispose (GObject * object)
{
  GstY4mDec *y4mdec;

  g_return_if_fail (GST_IS_Y4M_DEC (object));
  y4mdec = GST_Y4M_DEC (object);

  /* clean up as possible.  may be called multiple times */
  if (y4mdec->adapter) {
    g_object_unref (y4mdec->adapter);
    y4mdec->adapter = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_y4m_dec_finalize (GObject * object)
{
  g_return_if_fail (GST_IS_Y4M_DEC (object));

  /* clean up object here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_y4m_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstY4mDec *y4mdec;
  GstStateChangeReturn ret;

  g_return_val_if_fail (GST_IS_Y4M_DEC (element), GST_STATE_CHANGE_FAILURE);

  y4mdec = GST_Y4M_DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (y4mdec->pool) {
        gst_buffer_pool_set_active (y4mdec->pool, FALSE);
        gst_object_unref (y4mdec->pool);
      }
      y4mdec->pool = NULL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static GstClockTime
gst_y4m_dec_frames_to_timestamp (GstY4mDec * y4mdec, gint64 frame_index)
{
  if (frame_index == -1)
    return -1;

  return gst_util_uint64_scale (frame_index, GST_SECOND * y4mdec->info.fps_d,
      y4mdec->info.fps_n);
}

static gint64
gst_y4m_dec_timestamp_to_frames (GstY4mDec * y4mdec, GstClockTime timestamp)
{
  if (timestamp == -1)
    return -1;

  return gst_util_uint64_scale (timestamp, y4mdec->info.fps_n,
      GST_SECOND * y4mdec->info.fps_d);
}

static gint64
gst_y4m_dec_bytes_to_frames (GstY4mDec * y4mdec, gint64 bytes)
{
  if (bytes == -1)
    return -1;

  if (bytes < y4mdec->header_size)
    return 0;
  return (bytes - y4mdec->header_size) / (y4mdec->info.size + 6);
}

static guint64
gst_y4m_dec_frames_to_bytes (GstY4mDec * y4mdec, gint64 frame_index)
{
  if (frame_index == -1)
    return -1;

  return y4mdec->header_size + (y4mdec->info.size + 6) * frame_index;
}

static GstClockTime
gst_y4m_dec_bytes_to_timestamp (GstY4mDec * y4mdec, gint64 bytes)
{
  if (bytes == -1)
    return -1;

  return gst_y4m_dec_frames_to_timestamp (y4mdec,
      gst_y4m_dec_bytes_to_frames (y4mdec, bytes));
}

static GstVideoFormat
parse_colorspace (const char *param)
{
  char *end;
  guint iformat = g_ascii_strtoull (param, &end, 10);

  if (*end == '\0') {
    switch (iformat) {
      case 420:
        return GST_VIDEO_FORMAT_I420;
      case 411:
        return GST_VIDEO_FORMAT_Y41B;
      case 422:
        return GST_VIDEO_FORMAT_Y42B;
      case 444:
        return GST_VIDEO_FORMAT_Y444;
    }
  }

  /*
   * Parse non-standard (i.e., unknown to mjpegtools) streams that are
   * generated by FFmpeg:
   * https://wiki.multimedia.cx/index.php/YUV4MPEG2
   * https://github.com/FFmpeg/FFmpeg/blob/eee3b7e2/libavformat/yuv4mpegenc.c#L74-L166
   * Will assume little-endian because this is an on-disk serialization format.
   */

  // TODO: Differentiate between:
  // * C420jpeg: biaxially-displaced chroma planes
  // * C420paldv: coincident R and vertically-displaced B
  // * C420mpeg2: vertically-displaced chroma planes
  if (iformat == 420 && (g_strcmp0 (end, "jpeg") == 0 ||
          g_strcmp0 (end, "paldv") == 0 || g_strcmp0 (end, "mpeg2") == 0))
    return GST_VIDEO_FORMAT_I420;

  if (iformat == 0 && strncmp (end, "mono", 4) == 0) {
    char *type = end + 4;
    if (*type == '\0')
      return GST_VIDEO_FORMAT_GRAY8;
    if (g_strcmp0 (type, "16") == 0)
      return GST_VIDEO_FORMAT_GRAY16_LE;
  }

  if (*end == 'p') {
    guint depth = g_ascii_strtoull (end + 1, NULL, 10);
    if (depth == 10) {
      switch (iformat) {
        case 420:
          return GST_VIDEO_FORMAT_I420_10LE;
        case 422:
          return GST_VIDEO_FORMAT_I422_10LE;
        case 444:
          return GST_VIDEO_FORMAT_Y444_10LE;
      }
    } else if (depth == 12) {
      switch (iformat) {
        case 420:
          return GST_VIDEO_FORMAT_I420_12LE;
        case 422:
          return GST_VIDEO_FORMAT_I422_12LE;
        case 444:
          return GST_VIDEO_FORMAT_Y444_12LE;
      }
    } else if (depth == 16 && iformat == 444) {
      return GST_VIDEO_FORMAT_Y444_16LE;
    }
  }

  GST_WARNING ("%s is not a supported format", param);
  return GST_VIDEO_FORMAT_UNKNOWN;
}

static gboolean
parse_ratio (const char *param, gulong * n, gulong * d)
{
  char *end;
  *n = g_ascii_strtoull (param, &end, 10);
  if (end == param)
    return FALSE;
  param = end;
  if (param[0] != ':')
    return FALSE;
  param++;
  *d = g_ascii_strtoull (param, &end, 10);
  if (end == param)
    return FALSE;
  return TRUE;
}

static gboolean
gst_y4m_dec_parse_header (GstY4mDec * y4mdec, char *header)
{
  guint len;
  char **params;
  guint interlaced_char = 0;
  gulong fps_n = 0, fps_d = 0;
  gulong par_n = 0, par_d = 0;
  gulong width = 0, height = 0;
  GstVideoFormat format = GST_VIDEO_FORMAT_I420;

  if (memcmp (header, "YUV4MPEG2 ", 10) != 0) {
    GST_ERROR_OBJECT (y4mdec, "y4m start code not found");
    return FALSE;
  }

  header += 10;
  if (!g_str_is_ascii (header)) {
    GST_ERROR_OBJECT (y4mdec, "Invalid non-ASCII y4m header: %s", header);
    return FALSE;
  }

  GST_INFO_OBJECT (y4mdec, "Found header: %s", header);
  params = g_strsplit (header, " ", -1);
  len = g_strv_length (params);

  for (int i = 0; i < len; i++) {
    const char *param = params[i];
    char param_type = *param;
    const char *param_value = param + 1;
    switch (param_type) {
      case 'C':
        format = parse_colorspace (param_value);
        if (format == GST_VIDEO_FORMAT_UNKNOWN) {
          GST_ERROR_OBJECT (y4mdec, "Failed to parse colorspace: %s", param);
          return FALSE;
        }
        GST_INFO_OBJECT (y4mdec, "Parsed format as %s",
            gst_video_format_to_string (format));
        continue;
      case 'W':
        if ((width = g_ascii_strtoull (param_value, NULL, 10)) == 0) {
          GST_ERROR_OBJECT (y4mdec, "Failed to parse width: %s", param);
          return FALSE;
        }
        continue;
      case 'H':
        if ((height = g_ascii_strtoull (param_value, NULL, 10)) == 0) {
          GST_ERROR_OBJECT (y4mdec, "Failed to parse height: %s", param);
          return FALSE;
        }
        continue;
      case 'I':
        if ((interlaced_char = param_value[0]) == 0) {
          GST_ERROR_OBJECT (y4mdec, "Expecting interlaced flag: %s", param);
          return FALSE;
        }
        continue;
      case 'F':
        if (!parse_ratio (param_value, &fps_n, &fps_d)) {
          GST_ERROR_OBJECT (y4mdec, "Failed to parse framerate: %s", param);
          return FALSE;
        }
        continue;
      case 'A':
        if (!parse_ratio (param_value, &par_n, &par_d)) {
          GST_ERROR_OBJECT (y4mdec, "Failed to parse PAR: %s", param);
          return FALSE;
        }
        continue;
    }
    GST_WARNING_OBJECT (y4mdec, "Unknown y4m param field '%s', ignoring",
        param);
  }
  g_strfreev (params);

  if (width > MAX_SIZE || height > MAX_SIZE) {
    GST_ERROR_OBJECT (y4mdec, "Dimensions %lux%lu out of range", width, height);
    return FALSE;
  }

  gst_video_info_init (&y4mdec->info);
  gst_video_info_set_format (&y4mdec->out_info, format, width, height);
  y4mdec->info = y4mdec->out_info;

  switch (y4mdec->info.finfo->format) {
    case GST_VIDEO_FORMAT_I420:
      y4mdec->info.offset[0] = 0;
      y4mdec->info.stride[0] = width;
      y4mdec->info.offset[1] = y4mdec->info.stride[0] * height;
      y4mdec->info.stride[1] = GST_ROUND_UP_2 (width) / 2;
      y4mdec->info.offset[2] =
          y4mdec->info.offset[1] +
          y4mdec->info.stride[1] * (GST_ROUND_UP_2 (height) / 2);
      y4mdec->info.stride[2] = GST_ROUND_UP_2 (width) / 2;
      y4mdec->info.size =
          y4mdec->info.offset[2] +
          y4mdec->info.stride[2] * (GST_ROUND_UP_2 (height) / 2);
      break;
    case GST_VIDEO_FORMAT_Y42B:
      y4mdec->info.offset[0] = 0;
      y4mdec->info.stride[0] = width;
      y4mdec->info.offset[1] = y4mdec->info.stride[0] * height;
      y4mdec->info.stride[1] = GST_ROUND_UP_2 (width) / 2;
      y4mdec->info.offset[2] =
          y4mdec->info.offset[1] + y4mdec->info.stride[1] * height;
      y4mdec->info.stride[2] = GST_ROUND_UP_2 (width) / 2;
      y4mdec->info.size =
          y4mdec->info.offset[2] + y4mdec->info.stride[2] * height;
      break;
    case GST_VIDEO_FORMAT_Y444:
      y4mdec->info.offset[0] = 0;
      y4mdec->info.stride[0] = width;
      y4mdec->info.offset[1] = y4mdec->info.stride[0] * height;
      y4mdec->info.stride[1] = width;
      y4mdec->info.offset[2] =
          y4mdec->info.offset[1] + y4mdec->info.stride[1] * height;
      y4mdec->info.stride[2] = width;
      y4mdec->info.size =
          y4mdec->info.offset[2] + y4mdec->info.stride[2] * height;
      break;
    default:
      break;
  }

  switch (interlaced_char) {
    case 0:
    case '?':
    case 'p':
      y4mdec->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
      break;
    case 't':
    case 'b':
      y4mdec->info.interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      break;
    default:
      GST_ERROR_OBJECT (y4mdec, "Unknown interlaced char '%c'",
          interlaced_char);
      return FALSE;
      break;
  }

  if (fps_n == 0)
    fps_n = 1;
  if (fps_d == 0)
    fps_d = 1;
  if (par_n == 0)
    par_n = 1;
  if (par_d == 0)
    par_d = 1;

  y4mdec->info.fps_n = fps_n;
  y4mdec->info.fps_d = fps_d;
  y4mdec->info.par_n = par_n;
  y4mdec->info.par_d = par_d;

  return TRUE;
}

static GstFlowReturn
gst_y4m_dec_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstY4mDec *y4mdec;
  int n_avail;
  GstFlowReturn flow_ret = GST_FLOW_OK;
#define MAX_HEADER_LENGTH 80
  char header[MAX_HEADER_LENGTH];
  int i;
  int len;

  y4mdec = GST_Y4M_DEC (parent);

  GST_DEBUG_OBJECT (y4mdec, "chain");

  if (GST_BUFFER_IS_DISCONT (buffer)) {
    GST_DEBUG ("got discont");
    gst_adapter_clear (y4mdec->adapter);
  }

  gst_adapter_push (y4mdec->adapter, buffer);
  n_avail = gst_adapter_available (y4mdec->adapter);

  if (!y4mdec->have_header) {
    gboolean ret;
    GstCaps *caps;
    GstQuery *query;

    if (n_avail < MAX_HEADER_LENGTH)
      return GST_FLOW_OK;

    gst_adapter_copy (y4mdec->adapter, (guint8 *) header, 0, MAX_HEADER_LENGTH);

    header[MAX_HEADER_LENGTH - 1] = 0;
    for (i = 0; i < MAX_HEADER_LENGTH; i++) {
      if (header[i] == 0x0a)
        header[i] = 0;
    }

    ret = gst_y4m_dec_parse_header (y4mdec, header);
    if (!ret) {
      GST_ELEMENT_ERROR (y4mdec, STREAM, DECODE,
          ("Failed to parse YUV4MPEG header"), (NULL));
      return GST_FLOW_ERROR;
    }

    y4mdec->header_size = strlen (header) + 1;
    gst_adapter_flush (y4mdec->adapter, y4mdec->header_size);

    caps = gst_video_info_to_caps (&y4mdec->info);
    ret = gst_pad_set_caps (y4mdec->srcpad, caps);

    query = gst_query_new_allocation (caps, FALSE);
    y4mdec->video_meta = FALSE;

    if (y4mdec->pool) {
      gst_buffer_pool_set_active (y4mdec->pool, FALSE);
      gst_object_unref (y4mdec->pool);
    }
    y4mdec->pool = NULL;

    if (gst_pad_peer_query (y4mdec->srcpad, query)) {
      y4mdec->video_meta =
          gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

      /* We only need a pool if we need to do stride conversion for downstream */
      if (!y4mdec->video_meta && memcmp (&y4mdec->info, &y4mdec->out_info,
              sizeof (y4mdec->info)) != 0) {
        GstBufferPool *pool = NULL;
        GstAllocator *allocator = NULL;
        GstAllocationParams params;
        GstStructure *config;
        guint size, min, max;

        if (gst_query_get_n_allocation_params (query) > 0) {
          gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
        } else {
          allocator = NULL;
          gst_allocation_params_init (&params);
        }

        if (gst_query_get_n_allocation_pools (query) > 0) {
          gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min,
              &max);
          size = MAX (size, y4mdec->out_info.size);
        } else {
          pool = NULL;
          size = y4mdec->out_info.size;
          min = max = 0;
        }

        if (pool == NULL) {
          pool = gst_video_buffer_pool_new ();
        }

        config = gst_buffer_pool_get_config (pool);
        gst_buffer_pool_config_set_params (config, caps, size, min, max);
        gst_buffer_pool_config_set_allocator (config, allocator, &params);
        gst_buffer_pool_set_config (pool, config);

        if (allocator)
          gst_object_unref (allocator);

        y4mdec->pool = pool;
      }
    } else if (memcmp (&y4mdec->info, &y4mdec->out_info,
            sizeof (y4mdec->info)) != 0) {
      GstBufferPool *pool;
      GstStructure *config;

      /* No pool, create our own if we need to do stride conversion */
      pool = gst_video_buffer_pool_new ();
      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_set_params (config, caps, y4mdec->out_info.size, 0,
          0);
      gst_buffer_pool_set_config (pool, config);
      y4mdec->pool = pool;
    }
    if (y4mdec->pool) {
      gst_buffer_pool_set_active (y4mdec->pool, TRUE);
    }
    gst_query_unref (query);
    gst_caps_unref (caps);
    if (!ret) {
      GST_DEBUG_OBJECT (y4mdec, "Couldn't set caps on src pad");
      return GST_FLOW_ERROR;
    }

    y4mdec->have_header = TRUE;
  }

  if (y4mdec->have_new_segment) {
    GstEvent *event;
    GstClockTime start = gst_y4m_dec_bytes_to_timestamp (y4mdec,
        y4mdec->segment.start);
    GstClockTime stop = gst_y4m_dec_bytes_to_timestamp (y4mdec,
        y4mdec->segment.stop);
    GstClockTime time = gst_y4m_dec_bytes_to_timestamp (y4mdec,
        y4mdec->segment.time);
    GstSegment seg;

    gst_segment_init (&seg, GST_FORMAT_TIME);
    seg.start = start;
    seg.stop = stop;
    seg.time = time;
    event = gst_event_new_segment (&seg);

    gst_pad_push_event (y4mdec->srcpad, event);
    //gst_event_unref (event);

    y4mdec->have_new_segment = FALSE;
    y4mdec->frame_index = gst_y4m_dec_bytes_to_frames (y4mdec,
        y4mdec->segment.time);
    GST_DEBUG ("new frame_index %d", y4mdec->frame_index);

  }

  while (1) {
    n_avail = gst_adapter_available (y4mdec->adapter);
    if (n_avail < MAX_HEADER_LENGTH)
      break;

    gst_adapter_copy (y4mdec->adapter, (guint8 *) header, 0, MAX_HEADER_LENGTH);
    header[MAX_HEADER_LENGTH - 1] = 0;
    for (i = 0; i < MAX_HEADER_LENGTH; i++) {
      if (header[i] == 0x0a)
        header[i] = 0;
    }
    if (memcmp (header, "FRAME", 5) != 0) {
      GST_ELEMENT_ERROR (y4mdec, STREAM, DECODE,
          ("Failed to parse YUV4MPEG frame"), (NULL));
      flow_ret = GST_FLOW_ERROR;
      break;
    }

    len = strlen (header);
    if (n_avail < y4mdec->info.size + len + 1) {
      /* not enough data */
      GST_TRACE ("not enough data for frame %d < %" G_GSIZE_FORMAT,
          n_avail, y4mdec->info.size + len + 1);
      break;
    }

    gst_adapter_flush (y4mdec->adapter, len + 1);

    buffer = gst_adapter_take_buffer (y4mdec->adapter, y4mdec->info.size);

    GST_BUFFER_TIMESTAMP (buffer) =
        gst_y4m_dec_frames_to_timestamp (y4mdec, y4mdec->frame_index);
    GST_BUFFER_DURATION (buffer) =
        gst_y4m_dec_frames_to_timestamp (y4mdec, y4mdec->frame_index + 1) -
        GST_BUFFER_TIMESTAMP (buffer);

    y4mdec->frame_index++;

    if (y4mdec->video_meta) {
      gst_buffer_add_video_meta_full (buffer, 0, y4mdec->info.finfo->format,
          y4mdec->info.width, y4mdec->info.height, y4mdec->info.finfo->n_planes,
          y4mdec->info.offset, y4mdec->info.stride);
    } else if (memcmp (&y4mdec->info, &y4mdec->out_info,
            sizeof (y4mdec->info)) != 0) {
      GstBuffer *outbuf;
      GstVideoFrame iframe, oframe;
      gint i, j;
      gint w, h, istride, ostride;
      guint8 *src, *dest;

      /* Allocate a new buffer and do stride conversion */
      g_assert (y4mdec->pool != NULL);

      flow_ret = gst_buffer_pool_acquire_buffer (y4mdec->pool, &outbuf, NULL);
      if (flow_ret != GST_FLOW_OK) {
        gst_buffer_unref (buffer);
        break;
      }

      gst_video_frame_map (&iframe, &y4mdec->info, buffer, GST_MAP_READ);
      gst_video_frame_map (&oframe, &y4mdec->out_info, outbuf, GST_MAP_WRITE);

      for (i = 0; i < 3; i++) {
        w = GST_VIDEO_FRAME_COMP_WIDTH (&iframe, i);
        h = GST_VIDEO_FRAME_COMP_HEIGHT (&iframe, i);
        istride = GST_VIDEO_FRAME_COMP_STRIDE (&iframe, i);
        ostride = GST_VIDEO_FRAME_COMP_STRIDE (&oframe, i);
        src = GST_VIDEO_FRAME_COMP_DATA (&iframe, i);
        dest = GST_VIDEO_FRAME_COMP_DATA (&oframe, i);

        for (j = 0; j < h; j++) {
          memcpy (dest, src, w);

          dest += ostride;
          src += istride;
        }
      }

      gst_video_frame_unmap (&iframe);
      gst_video_frame_unmap (&oframe);
      gst_buffer_copy_into (outbuf, buffer, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
      gst_buffer_unref (buffer);
      buffer = outbuf;
    }

    flow_ret = gst_pad_push (y4mdec->srcpad, buffer);
    if (flow_ret != GST_FLOW_OK)
      break;
  }

  GST_DEBUG ("returning %d", flow_ret);

  return flow_ret;
}

static gboolean
gst_y4m_dec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstY4mDec *y4mdec;

  y4mdec = GST_Y4M_DEC (parent);

  GST_DEBUG_OBJECT (y4mdec, "event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      res = gst_pad_push_event (y4mdec->srcpad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      res = gst_pad_push_event (y4mdec->srcpad, event);
      break;
    case GST_EVENT_SEGMENT:
    {
      GstSegment seg;

      gst_event_copy_segment (event, &seg);

      GST_DEBUG ("segment: %" GST_SEGMENT_FORMAT, &seg);

      if (seg.format == GST_FORMAT_BYTES) {
        y4mdec->segment = seg;
        y4mdec->have_new_segment = TRUE;
      }

      res = TRUE;
      /* not sure why it's not forwarded, but let's unref it so it
         doesn't leak, remove the unref if it gets forwarded again */
      gst_event_unref (event);
      //res = gst_pad_push_event (y4mdec->srcpad, event);
    }
      break;
    case GST_EVENT_EOS:
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static gboolean
gst_y4m_dec_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstY4mDec *y4mdec;

  y4mdec = GST_Y4M_DEC (parent);

  GST_DEBUG_OBJECT (y4mdec, "event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gint64 framenum;
      guint64 byte;

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type,
          &start, &stop_type, &stop);

      if (format != GST_FORMAT_TIME) {
        res = FALSE;
        break;
      }

      framenum = gst_y4m_dec_timestamp_to_frames (y4mdec, start);
      GST_DEBUG ("seeking to frame %" G_GINT64_FORMAT, framenum);
      if (framenum == -1) {
        res = FALSE;
        break;
      }

      byte = gst_y4m_dec_frames_to_bytes (y4mdec, framenum);
      GST_DEBUG ("offset %" G_GUINT64_FORMAT, (guint64) byte);
      if (byte == -1) {
        res = FALSE;
        break;
      }

      gst_event_unref (event);
      event = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags,
          start_type, byte, stop_type, -1);

      res = gst_pad_push_event (y4mdec->sinkpad, event);
    }
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static gboolean
gst_y4m_dec_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstY4mDec *y4mdec = GST_Y4M_DEC (parent);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      GstQuery *peer_query;

      GST_DEBUG ("duration query");

      gst_query_parse_duration (query, &format, NULL);

      if (format != GST_FORMAT_TIME) {
        res = FALSE;
        GST_DEBUG_OBJECT (y4mdec, "not handling duration query in format %d",
            format);
        break;
      }

      peer_query = gst_query_new_duration (GST_FORMAT_BYTES);

      res = gst_pad_peer_query (y4mdec->sinkpad, peer_query);
      if (res) {
        gint64 duration;
        int n_frames;

        gst_query_parse_duration (peer_query, &format, &duration);

        n_frames = gst_y4m_dec_bytes_to_frames (y4mdec, duration);
        GST_DEBUG ("duration in frames %d", n_frames);

        duration = gst_y4m_dec_frames_to_timestamp (y4mdec, n_frames);
        GST_DEBUG ("duration in time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (duration));

        gst_query_set_duration (query, GST_FORMAT_TIME, duration);
        res = TRUE;
      }
      gst_query_unref (peer_query);
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (y4mdec, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    y4mdec,
    "Demuxes/decodes YUV4MPEG streams",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
