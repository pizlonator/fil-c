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

#include "gstdwriteclockoverlay.h"
#include <mutex>
#include <time.h>

GST_DEBUG_CATEGORY_STATIC (dwrite_clock_overlay_debug);
#define GST_CAT_DEFAULT dwrite_clock_overlay_debug

enum
{
  PROP_0,
  PROP_TIME_FORMAT,
};

#define DEFAULT_TIME_FORMAT "%H:%M:%S"

struct GstDWriteClockOverlayPrivate
{
  std::mutex lock;
  std::string format;
  WString wformat;
};

struct _GstDWriteClockOverlay
{
  GstDWriteBaseOverlay parent;

  GstDWriteClockOverlayPrivate *priv;
};

static void gst_dwrite_clock_overlay_finalize (GObject * object);
static void gst_dwrite_clock_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_clock_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static WString
gst_dwrite_clock_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer);

#define gst_dwrite_clock_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDWriteClockOverlay, gst_dwrite_clock_overlay,
    GST_TYPE_DWRITE_BASE_OVERLAY);

static void
gst_dwrite_clock_overlay_class_init (GstDWriteClockOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstDWriteBaseOverlayClass *overlay_class =
      GST_DWRITE_BASE_OVERLAY_CLASS (klass);

  object_class->finalize = gst_dwrite_clock_overlay_finalize;
  object_class->set_property = gst_dwrite_clock_overlay_set_property;
  object_class->get_property = gst_dwrite_clock_overlay_get_property;

  g_object_class_install_property (object_class, PROP_TIME_FORMAT,
      g_param_spec_string ("time-format", "Date/Time Format",
          "Format to use for time and date value, as in strftime.",
          DEFAULT_TIME_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "DirectWrite Clock Overlay", "Filter/Editor/Video",
      "Overlays the current clock time on a video stream",
      "Seungha Yang <seungha@centricular.com>");

  overlay_class->get_text =
      GST_DEBUG_FUNCPTR (gst_dwrite_clock_overlay_get_text);

  GST_DEBUG_CATEGORY_INIT (dwrite_clock_overlay_debug,
      "dwriteclockoverlay", 0, "dwriteclockoverlay");
}

static void
gst_dwrite_clock_overlay_init (GstDWriteClockOverlay * self)
{
  g_object_set (self, "text-alignment", DWRITE_TEXT_ALIGNMENT_LEADING,
      "paragraph-alignment", DWRITE_PARAGRAPH_ALIGNMENT_NEAR, nullptr);

  self->priv = new GstDWriteClockOverlayPrivate ();
  self->priv->format = DEFAULT_TIME_FORMAT;
  self->priv->wformat = gst_dwrite_string_to_wstring (DEFAULT_TIME_FORMAT);
}

static void
gst_dwrite_clock_overlay_finalize (GObject * object)
{
  GstDWriteClockOverlay *self = GST_DWRITE_CLOCK_OVERLAY (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dwrite_clock_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteClockOverlay *self = GST_DWRITE_CLOCK_OVERLAY (object);
  GstDWriteClockOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_TIME_FORMAT:
    {
      const gchar *format = g_value_get_string (value);
      if (format)
        priv->format = format;
      else
        priv->format = DEFAULT_TIME_FORMAT;

      priv->wformat = gst_dwrite_string_to_wstring (priv->format);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dwrite_clock_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteClockOverlay *self = GST_DWRITE_CLOCK_OVERLAY (object);
  GstDWriteClockOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  switch (prop_id) {
    case PROP_TIME_FORMAT:
      g_value_set_string (value, priv->format.c_str ());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static WString
gst_dwrite_clock_overlay_render_time (GstDWriteClockOverlay * self)
{
  GstDWriteClockOverlayPrivate *priv = self->priv;
  struct tm *t;
  time_t now;
  wchar_t text[256];

  now = time (nullptr);
  t = localtime (&now);

  if (!t)
    return WString (L"--:--:--");

  if (wcsftime (text, G_N_ELEMENTS (text), priv->wformat.c_str (), t) == 0)
    return WString (L"--:--:--");

  return WString (text);
}

static WString
gst_dwrite_clock_overlay_get_text (GstDWriteBaseOverlay * overlay,
    const WString & default_text, GstBuffer * buffer)
{
  GstDWriteClockOverlay *self = GST_DWRITE_CLOCK_OVERLAY (overlay);
  WString time_str = gst_dwrite_clock_overlay_render_time (self);

  if (default_text.empty ())
    return time_str;

  return default_text + WString (L" ") + time_str;
}
