/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2005> Tim-Philipp Müller <tim@centricular.net>
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
 * SECTION:element-clockoverlay
 * @title: clockoverlay
 * @see_also: #GstBaseTextOverlay, #GstTimeOverlay
 *
 * This element overlays the current clock time on top of a video
 * stream. You can position the text and configure the font details
 * using its properties.
 *
 * By default, the time is displayed in the top left corner of the picture, with some
 * padding to the left and to the top.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 -v videotestsrc ! clockoverlay ! autovideosink
 * ]|
 * Display the current wall clock time in the top left corner of the video picture
 * |[
 * gst-launch-1.0 -v videotestsrc ! clockoverlay halignment=right valignment=bottom text="Edge City" shaded-background=true font-desc="Sans, 36" ! videoconvert ! autovideosink
 * ]|
 * Another pipeline that displays the current time with some leading
 * text in the bottom right corner of the video picture, with the background
 * of the text being shaded in order to make it more legible on top of a
 * bright video background.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>
#include <time.h>

#include "gstclockoverlay.h"
#include "gstpangoelements.h"

#define DEFAULT_PROP_TIMEFORMAT 	"%H:%M:%S"

enum
{
  PROP_0,
  PROP_TIMEFORMAT,
  PROP_LAST
};

#define gst_clock_overlay_parent_class parent_class
G_DEFINE_TYPE (GstClockOverlay, gst_clock_overlay, GST_TYPE_BASE_TEXT_OVERLAY);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (clockoverlay, "clockoverlay",
    GST_RANK_NONE, GST_TYPE_CLOCK_OVERLAY, pango_element_init (plugin));

static void gst_clock_overlay_finalize (GObject * object);
static void gst_clock_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_clock_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gchar *
gst_clock_overlay_render_time (GstClockOverlay * overlay)
{
#ifdef HAVE_LOCALTIME_R
  struct tm dummy;
#endif
  struct tm *t;
  time_t now;
#ifdef G_OS_WIN32
  gunichar2 buf[256];
#else
  gchar buf[256];
#endif

  now = time (NULL);

#ifdef HAVE_LOCALTIME_R
  /* Need to call tzset explicitly when calling localtime_r for changes
   * to the timezone between calls to be visible.  */
  tzset ();
  t = localtime_r (&now, &dummy);
#else
  /* on win32 this apparently returns a per-thread struct which would be fine */
  t = localtime (&now);
#endif

  if (t == NULL)
    return g_strdup ("--:--:--");

#ifdef G_OS_WIN32
  if (wcsftime (buf, sizeof (buf), (wchar_t *) overlay->wformat, t) == 0)
    return g_strdup ("");

  return g_utf16_to_utf8 (buf, -1, NULL, NULL, NULL);
#else
  if (strftime (buf, sizeof (buf), overlay->format, t) == 0)
    return g_strdup ("");

  return g_strdup (buf);
#endif
}

/* Called with lock held */
static gchar *
gst_clock_overlay_get_text (GstBaseTextOverlay * overlay,
    GstBuffer * video_frame)
{
  gchar *time_str, *txt, *ret;
  GstClockOverlay *clock_overlay = GST_CLOCK_OVERLAY (overlay);

  txt = g_strdup (overlay->default_text);

  GST_OBJECT_LOCK (overlay);
  time_str = gst_clock_overlay_render_time (clock_overlay);
  GST_OBJECT_UNLOCK (overlay);

  if (txt != NULL && *txt != '\0') {
    ret = g_strdup_printf ("%s %s", txt, time_str);
  } else {
    ret = time_str;
    time_str = NULL;
  }

  if (g_strcmp0 (ret, clock_overlay->text)) {
    overlay->need_render = TRUE;
    g_free (clock_overlay->text);
    clock_overlay->text = g_strdup (ret);
  }

  g_free (txt);
  g_free (time_str);

  return ret;
}

static void
gst_clock_overlay_class_init (GstClockOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTextOverlayClass *gsttextoverlay_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gsttextoverlay_class = (GstBaseTextOverlayClass *) klass;

  gobject_class->finalize = gst_clock_overlay_finalize;
  gobject_class->set_property = gst_clock_overlay_set_property;
  gobject_class->get_property = gst_clock_overlay_get_property;

  gst_element_class_set_static_metadata (gstelement_class, "Clock overlay",
      "Filter/Editor/Video",
      "Overlays the current clock time on a video stream",
      "Tim-Philipp Müller <tim@centricular.net>");

  gsttextoverlay_class->get_text = gst_clock_overlay_get_text;

  g_object_class_install_property (gobject_class, PROP_TIMEFORMAT,
      g_param_spec_string ("time-format", "Date/Time Format",
          "Format to use for time and date value, as in strftime.",
          DEFAULT_PROP_TIMEFORMAT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}


static void
gst_clock_overlay_finalize (GObject * object)
{
  GstClockOverlay *overlay = GST_CLOCK_OVERLAY (object);

  g_free (overlay->format);
  g_free (overlay->text);
  overlay->format = NULL;

  g_free (overlay->wformat);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gst_clock_overlay_init (GstClockOverlay * overlay)
{
  GstBaseTextOverlay *textoverlay;
  PangoContext *context;
  PangoFontDescription *font_description;

  textoverlay = GST_BASE_TEXT_OVERLAY (overlay);

  textoverlay->valign = GST_BASE_TEXT_OVERLAY_VALIGN_TOP;
  textoverlay->halign = GST_BASE_TEXT_OVERLAY_HALIGN_LEFT;

  overlay->format = g_strdup (DEFAULT_PROP_TIMEFORMAT);

#ifdef G_OS_WIN32
  overlay->wformat =
      g_utf8_to_utf16 (DEFAULT_PROP_TIMEFORMAT, -1, NULL, NULL, NULL);
#endif

  context = textoverlay->pango_context;

  pango_context_set_language (context, pango_language_from_string ("en_US"));
  pango_context_set_base_dir (context, PANGO_DIRECTION_LTR);

  font_description = pango_font_description_new ();
  pango_font_description_set_family_static (font_description, "Monospace");
  pango_font_description_set_style (font_description, PANGO_STYLE_NORMAL);
  pango_font_description_set_variant (font_description, PANGO_VARIANT_NORMAL);
  pango_font_description_set_weight (font_description, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_stretch (font_description, PANGO_STRETCH_NORMAL);
  pango_font_description_set_size (font_description, 18 * PANGO_SCALE);
  pango_context_set_font_description (context, font_description);
  pango_font_description_free (font_description);
}

static void
gst_clock_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstClockOverlay *overlay = GST_CLOCK_OVERLAY (object);

  GST_OBJECT_LOCK (overlay);
  switch (prop_id) {
    case PROP_TIMEFORMAT:
      g_free (overlay->format);
      overlay->format = g_value_dup_string (value);
      if (!overlay->format)
        overlay->format = g_strdup (DEFAULT_PROP_TIMEFORMAT);
#ifdef G_OS_WIN32
      g_free (overlay->wformat);
      overlay->wformat =
          g_utf8_to_utf16 (overlay->format, -1, NULL, NULL, NULL);
#endif
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (overlay);
}


static void
gst_clock_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstClockOverlay *overlay = GST_CLOCK_OVERLAY (object);

  GST_OBJECT_LOCK (overlay);
  switch (prop_id) {
    case PROP_TIMEFORMAT:
      g_value_set_string (value, overlay->format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (overlay);
}
