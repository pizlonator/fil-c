/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdwritetimeoverlay.h"
#include <mutex>
#include <strsafe.h>

GST_DEBUG_CATEGORY_STATIC (dwrite_time_overlay_debug);
#define GST_CAT_DEFAULT dwrite_time_overlay_debug

typedef enum
{
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_TIME,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_STREAM_TIME,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_RUNNING_TIME,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_TIME_CODE,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_ELAPSED_RUNNING_TIME,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_REFERENCE_TIMESTAMP,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_COUNT,
  GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_OFFSET,
} GstDWriteTimeOverlayTimeLine;

#define GST_TYPE_DWrite_TIME_OVERLAY_TIME_LINE (gst_dwrite_time_overlay_time_line_type ())
static GType
gst_dwrite_time_overlay_time_line_type (void)
{
  static GType type;
  static const GEnumValue modes[] = {
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_TIME,
        "buffer-time", "buffer-time"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_STREAM_TIME,
        "stream-time", "stream-time"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_RUNNING_TIME,
        "running-time", "running-time"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_TIME_CODE, "time-code", "time-code"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_ELAPSED_RUNNING_TIME,
        "elapsed-running-time", "elapsed-running-time"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_REFERENCE_TIMESTAMP,
        "reference-timestamp", "reference-timestamp"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_COUNT,
        "buffer-count", "buffer-count"},
    {GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_OFFSET,
        "buffer-offset", "buffer-offset"},
    {0, nullptr, nullptr},
  };

  GST_DWRITE_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstDWriteTimeOverlayTimeLine", modes);
  } GST_DWRITE_CALL_ONCE_END;

  return type;
}

enum
{
  PROP_0,
  PROP_TIME_LINE,
  PROP_SHOW_TIMES_AS_DATES,
  PROP_DATETIME_EPOCH,
  PROP_DATETIME_FORMAT,
  PROP_REFERENCE_TIMESTAMP_CAPS,
};

#define DEFAULT_TIME_LINE GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_TIME
#define DEFAULT_SHOW_TIMES_AS_DATES FALSE
#define DEFAULT_DATETIME_FORMAT "%F %T" /* YYYY-MM-DD hh:mm:ss */

static GstStaticCaps ntp_reference_timestamp_caps =
GST_STATIC_CAPS ("timestamp/x-ntp");

struct GstDWriteTimeOverlayPrivate
{
  GstDWriteTimeOverlayPrivate ()
  {
    datetime_epoch = g_date_time_new_utc (1900, 1, 1, 0, 0, 0);
    reference_timestamp_caps =
        gst_static_caps_get (&ntp_reference_timestamp_caps);
  }

   ~GstDWriteTimeOverlayPrivate ()
  {
    if (datetime_epoch)
      g_date_time_unref (datetime_epoch);
    gst_clear_caps (&reference_timestamp_caps);
  }

  std::mutex lock;
  GstDWriteTimeOverlayTimeLine time_line = DEFAULT_TIME_LINE;

  gboolean show_times_as_dates = DEFAULT_SHOW_TIMES_AS_DATES;
  guint64 buffer_count = 0;
  std::string datetime_format = DEFAULT_DATETIME_FORMAT;
  GDateTime *datetime_epoch;
  GstCaps *reference_timestamp_caps;
  GstClockTime first_running_time = GST_CLOCK_TIME_NONE;
};

struct _GstDWriteTimeOverlay
{
  GstDWriteBaseOverlay parent;

  GstDWriteTimeOverlayPrivate *priv;
};

static void gst_dwrite_time_overlay_finalize (GObject * object);
static void gst_dwrite_time_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_time_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_dwrite_time_overlay_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_dwrite_time_overlay_start (GstBaseTransform * overlay);
static WString gst_dwrite_time_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer);

#define gst_dwrite_time_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDWriteTimeOverlay, gst_dwrite_time_overlay,
    GST_TYPE_DWRITE_BASE_OVERLAY);

static void
gst_dwrite_time_overlay_class_init (GstDWriteTimeOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstDWriteBaseOverlayClass *overlay_class =
      GST_DWRITE_BASE_OVERLAY_CLASS (klass);

  object_class->finalize = gst_dwrite_time_overlay_finalize;
  object_class->set_property = gst_dwrite_time_overlay_set_property;
  object_class->get_property = gst_dwrite_time_overlay_get_property;

  g_object_class_install_property (object_class, PROP_TIME_LINE,
      g_param_spec_enum ("time-mode", "Time Mode", "What time to show",
          GST_TYPE_DWrite_TIME_OVERLAY_TIME_LINE, DEFAULT_TIME_LINE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_DATETIME_EPOCH,
      g_param_spec_boxed ("datetime-epoch", "Datetime Epoch",
          "When showing times as dates, the initial date from which time "
          "is counted, if not specified prime epoch is used (1900-01-01)",
          G_TYPE_DATE_TIME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_DATETIME_FORMAT,
      g_param_spec_string ("datetime-format", "Datetime Format",
          "When showing times as dates, the format to render date and time in",
          DEFAULT_DATETIME_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_SHOW_TIMES_AS_DATES,
      g_param_spec_boolean ("show-times-as-dates", "Show times as dates",
          "Whether to display times, counted from datetime-epoch, as dates",
          DEFAULT_SHOW_TIMES_AS_DATES,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_REFERENCE_TIMESTAMP_CAPS,
      g_param_spec_boxed ("reference-timestamp-caps",
          "Reference Timestamp Caps",
          "Caps to use for the reference timestamp time mode",
          GST_TYPE_CAPS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "DirectWrite Time Overlay", "Filter/Editor/Video",
      "Overlays buffer time stamps on a video stream",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_dwrite_time_overlay_sink_event);

  trans_class->start = GST_DEBUG_FUNCPTR (gst_dwrite_time_overlay_start);
  overlay_class->get_text =
      GST_DEBUG_FUNCPTR (gst_dwrite_time_overlay_get_text);

  GST_DEBUG_CATEGORY_INIT (dwrite_time_overlay_debug,
      "dwritetimeoverlay", 0, "dwritetimeoverlay");

  gst_type_mark_as_plugin_api (GST_TYPE_DWrite_TIME_OVERLAY_TIME_LINE,
      (GstPluginAPIFlags) 0);
}

static void
gst_dwrite_time_overlay_init (GstDWriteTimeOverlay * self)
{
  g_object_set (self, "text-alignment", DWRITE_TEXT_ALIGNMENT_LEADING,
      "paragraph-alignment", DWRITE_PARAGRAPH_ALIGNMENT_NEAR, nullptr);

  self->priv = new GstDWriteTimeOverlayPrivate ();
}

static void
gst_dwrite_time_overlay_finalize (GObject * object)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dwrite_time_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (object);
  GstDWriteTimeOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_TIME_LINE:
      priv->time_line = (GstDWriteTimeOverlayTimeLine) g_value_get_enum (value);
      break;
    case PROP_SHOW_TIMES_AS_DATES:
      priv->show_times_as_dates = g_value_get_boolean (value);
      break;
    case PROP_DATETIME_EPOCH:
      g_date_time_unref (priv->datetime_epoch);
      priv->datetime_epoch = (GDateTime *) g_value_dup_boxed (value);
      break;
    case PROP_DATETIME_FORMAT:
    {
      const gchar *format = g_value_get_string (value);
      if (format)
        priv->datetime_format = format;
      else
        priv->datetime_format = DEFAULT_DATETIME_FORMAT;
      break;
    }
    case PROP_REFERENCE_TIMESTAMP_CAPS:
      gst_clear_caps (&priv->reference_timestamp_caps);
      priv->reference_timestamp_caps = (GstCaps *) g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dwrite_time_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (object);
  GstDWriteTimeOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_TIME_LINE:
      g_value_set_enum (value, priv->time_line);
      break;
    case PROP_SHOW_TIMES_AS_DATES:
      g_value_set_boolean (value, priv->show_times_as_dates);
      break;
    case PROP_DATETIME_EPOCH:
      g_value_set_boxed (value, priv->datetime_epoch);
      break;
    case PROP_DATETIME_FORMAT:
      g_value_set_string (value, priv->datetime_format.c_str ());
      break;
    case PROP_REFERENCE_TIMESTAMP_CAPS:
      g_value_set_boxed (value, priv->reference_timestamp_caps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dwrite_time_overlay_start (GstBaseTransform * trans)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (trans);
  GstDWriteTimeOverlayPrivate *priv = self->priv;

  priv->first_running_time = GST_CLOCK_TIME_NONE;
  priv->buffer_count = 0;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->start (trans);
}

static gboolean
gst_dwrite_time_overlay_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (trans);
  GstDWriteTimeOverlayPrivate *priv = self->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      priv->first_running_time = GST_CLOCK_TIME_NONE;
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static WString
gst_dwrite_time_overlay_render_time (GstDWriteTimeOverlay * self,
    GstClockTime time)
{
  wchar_t text[256];
  HRESULT hr;
  guint h, m, s, ms;

  if (!GST_CLOCK_TIME_IS_VALID (time))
    return WString ();

  h = (guint) (time / (GST_SECOND * 60 * 60));
  m = (guint) ((time / (GST_SECOND * 60)) % 60);
  s = (guint) ((time / GST_SECOND) % 60);
  ms = (guint) ((time % GST_SECOND) / (1000 * 1000));

  hr = StringCbPrintfW (text, sizeof (text), L"%u:%02u:%02u.%03u", h, m, s, ms);

  if (FAILED (hr))
    return WString ();

  return WString (text);
}

static WString
gst_dwrite_time_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer)
{
  GstDWriteTimeOverlay *self = GST_DWRITE_TIME_OVERLAY (overlay);
  GstDWriteTimeOverlayPrivate *priv = self->priv;
  WString time_str;
  WString ret;
  std::lock_guard < std::mutex > lk (priv->lock);
  gboolean show_buffer_count = FALSE;

  if (priv->time_line == GST_DWRITE_TIME_OVERLAY_TIME_LINE_TIME_CODE) {
    GstVideoTimeCodeMeta *tc_meta =
        gst_buffer_get_video_time_code_meta (buffer);
    if (!tc_meta) {
      GST_DEBUG_OBJECT (self, "buffer without valid timecode");
      time_str = L"00:00:00:00";
    } else {
      gchar *str = gst_video_time_code_to_string (&tc_meta->tc);
      GST_DEBUG_OBJECT (self, "buffer with timecode %s", str);
      time_str = gst_dwrite_string_to_wstring (str);
      g_free (str);
    }
  } else {
    GstBaseTransform *trans = GST_BASE_TRANSFORM (overlay);
    GstClockTime ts, ts_buffer;
    GstSegment *seg = &trans->segment;

    ts = ts_buffer = GST_BUFFER_TIMESTAMP (buffer);
    if (GST_CLOCK_TIME_IS_VALID (ts)) {
      switch (priv->time_line) {
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_STREAM_TIME:
          ts = gst_segment_to_stream_time (seg, GST_FORMAT_TIME, ts_buffer);
          break;
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_RUNNING_TIME:
          ts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, ts_buffer);
          break;
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_ELAPSED_RUNNING_TIME:
          ts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, ts_buffer);
          if (!GST_CLOCK_TIME_IS_VALID (priv->first_running_time))
            priv->first_running_time = ts;
          ts -= priv->first_running_time;
          break;
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_REFERENCE_TIMESTAMP:
        {
          GstReferenceTimestampMeta *meta;
          if (priv->reference_timestamp_caps) {
            meta = gst_buffer_get_reference_timestamp_meta (buffer,
                priv->reference_timestamp_caps);
            if (meta)
              ts = meta->timestamp;
            else
              ts = 0;
          } else {
            ts = 0;
          }
          break;
        }
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_COUNT:
          show_buffer_count = TRUE;
          priv->buffer_count++;
          break;
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_OFFSET:
          show_buffer_count = TRUE;
          ts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, ts_buffer);
          priv->buffer_count = gst_util_uint64_scale (ts, overlay->info.fps_n,
              overlay->info.fps_d * GST_SECOND);
          break;
        case GST_DWRITE_TIME_OVERLAY_TIME_LINE_BUFFER_TIME:
        default:
          ts = ts_buffer;
          break;
      }

      if (show_buffer_count) {
        time_str = std::to_wstring (priv->buffer_count);
      } else if (priv->show_times_as_dates) {
        GDateTime *datetime;
        gchar *str;

        datetime =
            g_date_time_add_seconds (priv->datetime_epoch,
            ((gdouble) ts) / GST_SECOND);

        str = g_date_time_format (datetime, priv->datetime_format.c_str ());
        time_str = gst_dwrite_string_to_wstring (str);

        g_free (str);
        g_date_time_unref (datetime);
      } else {
        time_str = gst_dwrite_time_overlay_render_time (self, ts);
      }
    }
  }

  if (default_text.empty ())
    return time_str;

  return default_text + WString (L" ") + time_str;
}
