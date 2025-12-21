/* GStreamer
 *
 * playback-test.c: playback sample application
 *
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 *               2006 Stefan Kost <ensonic@users.sf.net>
 *               2012 Collabora Ltd.
 *                 Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#if GTK_CHECK_VERSION(3, 24, 10)
#include <AppKit/AppKit.h>
NSView *gdk_quartz_window_get_nsview (GdkWindow * window);
#endif
#endif

#include <gst/video/videooverlay.h>
#include <gst/video/colorbalance.h>
#include <gst/video/navigation.h>

GST_DEBUG_CATEGORY_STATIC (playback_debug);
#define GST_CAT_DEFAULT (playback_debug)

/* Copied from gst-plugins-base/gst/playback/gstplay-enum.h */
typedef enum
{
  GST_PLAY_FLAG_VIDEO = (1 << 0),
  GST_PLAY_FLAG_AUDIO = (1 << 1),
  GST_PLAY_FLAG_TEXT = (1 << 2),
  GST_PLAY_FLAG_VIS = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD = (1 << 7),
  GST_PLAY_FLAG_BUFFERING = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE = (1 << 9),
  GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
  GST_PLAY_FLAG_FORCE_FILTERS = (1 << 11),
} GstPlayFlags;

/* configuration */

#define FILL_INTERVAL 100
//#define UPDATE_INTERVAL 500
//#define UPDATE_INTERVAL 100
#define UPDATE_INTERVAL 40
#define SLOW_UPDATE_INTERVAL 500

/* number of milliseconds to play for after a seek */
#define SCRUB_TIME 100

/* timeout for gst_element_get_state() after a seek */
#define SEEK_TIMEOUT 40 * GST_MSECOND

#define DEFAULT_VIDEO_HEIGHT 300

/* the state to go to when stop is pressed */
#define STOP_STATE      GST_STATE_READY

#define N_GRAD 1000.0

/* we keep an array of the visualisation entries so that we can easily switch
 * with the combo box index. */
typedef struct
{
  GstElementFactory *factory;
} VisEntry;

typedef struct
{
  /* GTK widgets */
  GtkWidget *window;
  GtkWidget *video_combo, *audio_combo, *text_combo, *vis_combo;
  GtkWidget *video_window;

  GtkWidget *vis_checkbox, *video_checkbox, *audio_checkbox;
  GtkWidget *text_checkbox, *mute_checkbox, *volume_spinbutton;
  GtkWidget *soft_volume_checkbox, *native_audio_checkbox,
      *native_video_checkbox;
  GtkWidget *download_checkbox, *buffering_checkbox, *deinterlace_checkbox;
  GtkWidget *soft_colorbalance_checkbox;
  GtkWidget *video_sink_entry, *audio_sink_entry, *text_sink_entry;
  GtkWidget *buffer_size_entry, *buffer_duration_entry;
  GtkWidget *ringbuffer_maxsize_entry, *connection_speed_entry;
  GtkWidget *av_offset_entry, *subtitle_encoding_entry;
  GtkWidget *subtitle_fontdesc_button;
  GtkWidget *text_offset_entry;

  GtkWidget *seek_format_combo, *seek_position_label, *seek_duration_label;
  GtkWidget *seek_start_label, *seek_stop_label;
  GtkWidget *seek_entry;

  GtkWidget *seek_scale, *statusbar;
  guint status_id;

  GtkWidget *step_format_combo, *step_amount_spinbutton, *step_rate_spinbutton;
  GtkWidget *shuttle_scale;

  GtkWidget *contrast_scale, *brightness_scale, *hue_scale, *saturation_scale;

  struct
  {
    GstNavigationCommand cmd;
    GtkWidget *button;
  } navigation_buttons[14];

  guintptr embed_xid;

  /* GStreamer pipeline */
  GstElement *pipeline;

  GstElement *navigation_element;
  GstElement *colorbalance_element;
  GstElement *overlay_element;

  /* Settings */
  gboolean accurate_seek;
  gboolean keyframe_seek;
  gboolean loop_seek;
  gboolean flush_seek;
  gboolean scrub;
  gboolean play_scrub;
  gboolean instant_rate_change;
  gboolean skip_seek;
  gboolean skip_seek_key_only;
  gboolean skip_seek_no_audio;
  gdouble rate;
  gboolean snap_before;
  gboolean snap_after;

  /* From commandline parameters */
  gboolean stats;
  gboolean verbose;
  const gchar *pipeline_spec;
  gint pipeline_type;
  GList *paths, *current_path;
  GList *sub_paths, *current_sub_path;

  /* Internal state */
  gint64 position, duration;

  gboolean is_live;
  gboolean buffering;
  GstBufferingMode mode;
  gint64 buffering_left;
  GstState state;
  guint update_id;
  guint slow_update_id;
  guint seek_timeout_id;        /* Used for scrubbing in paused */
  gulong changed_id;
  guint fill_id;

  gboolean need_streams;
  gint n_video, n_audio, n_text;

  GMutex state_mutex;

  GArray *vis_entries;          /* Array of VisEntry structs */

  gboolean shuttling;
  gdouble shuttle_rate;
  gdouble play_rate;

  const GstFormatDefinition *seek_format;
  GList *formats;
} PlaybackApp;

static void clear_streams (PlaybackApp * app);
static void find_interface_elements (PlaybackApp * app);
static void volume_notify_cb (GstElement * pipeline, GParamSpec * arg,
    PlaybackApp * app);
static void mute_notify_cb (GstElement * pipeline, GParamSpec * arg,
    PlaybackApp * app);

static void video_sink_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void text_sink_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void audio_sink_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void buffer_size_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void buffer_duration_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void ringbuffer_maxsize_activate_cb (GtkEntry * entry,
    PlaybackApp * app);
static void connection_speed_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void av_offset_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void text_offset_activate_cb (GtkEntry * entry, PlaybackApp * app);
static void subtitle_encoding_activate_cb (GtkEntry * entry, PlaybackApp * app);

/* pipeline construction */

static GstElement *
gst_element_factory_make_or_warn (const gchar * type, const gchar * name)
{
  GstElement *element = gst_element_factory_make (type, name);

#ifndef GST_DISABLE_PARSE
  if (!element) {
    /* Try parsing it as a pipeline description */
    element = gst_parse_bin_from_description (type, TRUE, NULL);
    if (element) {
      gst_element_set_name (element, name);
    }
  }
#endif

  if (!element) {
    g_warning ("Failed to create element %s of type %s", name, type);
  }

  return element;
}

static void
set_uri_property (GObject * object, const gchar * property,
    const gchar * location)
{
  gchar *uri;

  /* Add "file://" prefix for convenience */
  if (location && (g_str_has_prefix (location, "/")
          || !gst_uri_is_valid (location))) {
    uri = gst_filename_to_uri (location, NULL);
    g_print ("Setting URI: %s\n", uri);
    g_object_set (object, property, uri, NULL);
    g_free (uri);
  } else {
    g_print ("Setting URI: %s\n", location);
    g_object_set (object, property, location, NULL);
  }
}

static void
playbin_set_uri (GstElement * playbin, const gchar * location,
    const gchar * sub_location)
{
  set_uri_property (G_OBJECT (playbin), "uri", location);
  set_uri_property (G_OBJECT (playbin), "suburi", sub_location);
}

static void
make_playbin_pipeline (PlaybackApp * app, const gchar * location)
{
  GstElement *pipeline;

  app->pipeline = pipeline = gst_element_factory_make ("playbin", "playbin");
  g_assert (pipeline);

  playbin_set_uri (pipeline, location,
      app->current_sub_path ? app->current_sub_path->data : NULL);

  g_signal_connect (pipeline, "notify::volume", G_CALLBACK (volume_notify_cb),
      app);
  g_signal_connect (pipeline, "notify::mute", G_CALLBACK (mute_notify_cb), app);

  app->navigation_element = GST_ELEMENT (gst_object_ref (pipeline));
  app->colorbalance_element = GST_ELEMENT (gst_object_ref (pipeline));
}

#ifndef GST_DISABLE_PARSE
static void
make_parselaunch_pipeline (PlaybackApp * app, const gchar * description)
{
  app->pipeline = gst_parse_launch (description, NULL);
}
#endif

typedef struct
{
  const gchar *name;
  void (*func) (PlaybackApp * app, const gchar * location);
  const gchar *help;
}
Pipeline;

static const Pipeline pipelines[] = {
  {"playbin", make_playbin_pipeline, "[URLS|FILENAMES]"},
#ifndef GST_DISABLE_PARSE
  {"parse-launch", make_parselaunch_pipeline, "[PARSE-LAUNCH-LINE]"},
#endif
};

/* ui callbacks and helpers */

static gchar *
format_value (GtkScale * scale, gdouble value, PlaybackApp * app)
{
  gint64 real;
  gint64 seconds;

  real = value * app->duration / N_GRAD;
  seconds = (gint64) real / GST_SECOND;
  /* Use two different formatting depending on the amount */
  if (seconds < 60 * 60) {
    gint64 subseconds = (gint64) real / (GST_MSECOND);

    /* Sub hour positioning */
    return g_strdup_printf ("%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT ":%03"
        G_GINT64_FORMAT, seconds / 60, seconds % 60, subseconds % 1000);
  } else {
    gint64 days = seconds / (24 * 60 * 60);
    gint64 hours = (seconds / (60 * 60)) % 60;
    gint64 minutes = (seconds / 60) % 60;

    if (days) {
      return g_strdup_printf ("%02" G_GINT64_FORMAT "d%02" G_GINT64_FORMAT
          "h%02" G_GINT64_FORMAT "m", days, hours, minutes);
    } else {
      return g_strdup_printf ("%02" G_GINT64_FORMAT "h%02" G_GINT64_FORMAT
          "m%02" G_GINT64_FORMAT "s", hours, minutes, seconds % 60);
    }
  }
}

static gchar *
shuttle_format_value (GtkScale * scale, gdouble value)
{
  return g_strdup_printf ("%0.*g", gtk_scale_get_digits (scale), value);
}

typedef struct
{
  const gchar *name;
  const GstFormat format;
}
seek_format;

static seek_format seek_formats[] = {
  {"tim", GST_FORMAT_TIME},
  {"byt", GST_FORMAT_BYTES},
  {"buf", GST_FORMAT_BUFFERS},
  {"def", GST_FORMAT_DEFAULT},
  {NULL, 0},
};

static void
query_positions (PlaybackApp * app)
{
  gint i = 0;

  g_print ("positions %8.8s: ", GST_ELEMENT_NAME (app->pipeline));
  while (seek_formats[i].name) {
    gint64 position, total;
    GstFormat format;

    format = seek_formats[i].format;

    if (gst_element_query_position (app->pipeline, format, &position) &&
        gst_element_query_duration (app->pipeline, format, &total)) {
      g_print ("%s %13" G_GINT64_FORMAT " / %13" G_GINT64_FORMAT " | ",
          seek_formats[i].name, position, total);
    } else {
      g_print ("%s %13.13s / %13.13s | ", seek_formats[i].name, "*NA*", "*NA*");
    }
    i++;
  }
  g_print (" %s\n", GST_ELEMENT_NAME (app->pipeline));
}

static gboolean start_seek (GtkRange * range, GdkEventButton * event,
    PlaybackApp * app);
static gboolean stop_seek (GtkRange * range, GdkEventButton * event,
    PlaybackApp * app);
static void seek_cb (GtkRange * range, PlaybackApp * app);

static void
set_scale (PlaybackApp * app, gdouble value)
{
  g_signal_handlers_block_by_func (app->seek_scale, start_seek, app);
  g_signal_handlers_block_by_func (app->seek_scale, stop_seek, app);
  g_signal_handlers_block_by_func (app->seek_scale, seek_cb, app);
  gtk_range_set_value (GTK_RANGE (app->seek_scale), value);
  g_signal_handlers_unblock_by_func (app->seek_scale, start_seek, app);
  g_signal_handlers_unblock_by_func (app->seek_scale, stop_seek, app);
  g_signal_handlers_unblock_by_func (app->seek_scale, seek_cb, app);
  gtk_widget_queue_draw (app->seek_scale);
}

static gboolean
update_fill (PlaybackApp * app)
{
  GstQuery *query;

  query = gst_query_new_buffering (GST_FORMAT_PERCENT);

  if (gst_element_query (app->pipeline, query)) {
    gint64 start, stop, estimated_total;
    GstFormat format;
    gdouble fill;
    gboolean busy;
    gint percent;
    GstBufferingMode mode;
    gint avg_in, avg_out;
    gint64 buffering_left;

    gst_query_parse_buffering_percent (query, &busy, &percent);
    gst_query_parse_buffering_stats (query, &mode, &avg_in, &avg_out,
        &buffering_left);
    gst_query_parse_buffering_range (query, &format, &start, &stop,
        &estimated_total);

    /* note that we could start the playback when buffering_left < remaining
     * playback time */
    GST_DEBUG ("buffering total %" G_GINT64_FORMAT " ms, left %"
        G_GINT64_FORMAT " ms", estimated_total, buffering_left);
    GST_DEBUG ("start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT,
        start, stop);

    if (stop != -1)
      fill = N_GRAD * stop / GST_FORMAT_PERCENT_MAX;
    else
      fill = N_GRAD;

    gtk_range_set_fill_level (GTK_RANGE (app->seek_scale), fill);
  }
  gst_query_unref (query);

  return TRUE;
}

static gboolean
update_seek_range (PlaybackApp * app)
{
  GstFormat format = GST_FORMAT_TIME;
  gint64 seek_start, seek_stop;
  gboolean seekable;
  GstQuery *query;

  query = gst_query_new_seeking (format);
  if (gst_element_query (app->pipeline, query)) {
    gchar *str;

    gst_query_parse_seeking (query, &format, &seekable, &seek_start,
        &seek_stop);
    if (!seekable) {
      seek_start = seek_stop = -1;
    }

    str = g_strdup_printf ("%" G_GINT64_FORMAT, seek_start);
    gtk_label_set_text (GTK_LABEL (app->seek_start_label), str);
    g_free (str);

    str = g_strdup_printf ("%" G_GINT64_FORMAT, seek_stop);
    gtk_label_set_text (GTK_LABEL (app->seek_stop_label), str);
    g_free (str);
  }
  gst_query_unref (query);

  return TRUE;
}

static gboolean
update_scale (PlaybackApp * app)
{
  GstFormat format = GST_FORMAT_TIME;
  gint64 seek_pos, seek_dur;
  gchar *str;

  //position = 0;
  //duration = 0;

  gst_element_query_position (app->pipeline, format, &app->position);
  gst_element_query_duration (app->pipeline, format, &app->duration);

  if (app->stats)
    query_positions (app);

  if (app->position >= app->duration)
    app->duration = app->position;

  if (app->duration > 0) {
    set_scale (app, app->position * N_GRAD / app->duration);
  }

  if (app->seek_format) {
    format = app->seek_format->value;
    seek_pos = seek_dur = -1;
    gst_element_query_position (app->pipeline, format, &seek_pos);
    gst_element_query_duration (app->pipeline, format, &seek_dur);

    str = g_strdup_printf ("%" G_GINT64_FORMAT, seek_pos);
    gtk_label_set_text (GTK_LABEL (app->seek_position_label), str);
    g_free (str);

    str = g_strdup_printf ("%" G_GINT64_FORMAT, seek_dur);
    gtk_label_set_text (GTK_LABEL (app->seek_duration_label), str);
    g_free (str);
  }

  return TRUE;
}

static void set_update_scale (PlaybackApp * app, gboolean active);
static void set_update_fill (PlaybackApp * app, gboolean active);

static gboolean
end_scrub (PlaybackApp * app)
{
  GST_DEBUG ("end scrub, PAUSE");
  gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
  app->seek_timeout_id = 0;

  return FALSE;
}

static gboolean
send_event (PlaybackApp * app, GstEvent * event)
{
  gboolean res = FALSE;

  GST_DEBUG ("send event on element %s", GST_ELEMENT_NAME (app->pipeline));
  res = gst_element_send_event (app->pipeline, event);

  return res;
}

static void
do_seek (PlaybackApp * app, GstFormat format, gint64 position)
{
  gboolean res = FALSE;
  GstEvent *s_event;
  GstClockTime start, stop;
  GstSeekType start_type = GST_SEEK_TYPE_SET;
  GstSeekType stop_type = GST_SEEK_TYPE_SET;
  GstSeekFlags flags;

  flags = 0;
  if (app->flush_seek)
    flags |= GST_SEEK_FLAG_FLUSH;
  if (app->accurate_seek)
    flags |= GST_SEEK_FLAG_ACCURATE;
  if (app->keyframe_seek)
    flags |= GST_SEEK_FLAG_KEY_UNIT;
  if (app->loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (app->skip_seek)
    flags |= GST_SEEK_FLAG_TRICKMODE;
  if (app->skip_seek_key_only)
    flags |= GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
  if (app->skip_seek_no_audio)
    flags |= GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;
  if (app->snap_before)
    flags |= GST_SEEK_FLAG_SNAP_BEFORE;
  if (app->snap_after)
    flags |= GST_SEEK_FLAG_SNAP_AFTER;

  if (app->instant_rate_change) {
    flags |= GST_SEEK_FLAG_INSTANT_RATE_CHANGE;
    start_type = stop_type = GST_SEEK_TYPE_NONE;
    start = stop = GST_CLOCK_TIME_NONE;
    if (app->flush_seek) {
      g_warning ("Instant rate change seek not supported with flushing");
      return;
    }
  } else if (position == GST_CLOCK_TIME_NONE) {
    start_type = stop_type = GST_SEEK_TYPE_NONE;
    start = stop = GST_CLOCK_TIME_NONE;
  } else if (app->rate < 0) {
    stop = position;
    start = 0;
  } else {
    start = position;
    stop = GST_CLOCK_TIME_NONE;
  }

  if (app->rate >= 0) {
    s_event = gst_event_new_seek (app->rate,
        format, flags, start_type, start, stop_type, stop);
    GST_DEBUG ("seek with rate %lf to %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT,
        app->rate, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
  } else {
    s_event = gst_event_new_seek (app->rate,
        format, flags, start_type, start, stop_type, stop);
    GST_DEBUG ("seek with rate %lf to %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT,
        app->rate, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
  }

  res = send_event (app, s_event);

  if (res) {
    if (app->flush_seek) {
      gst_element_get_state (GST_ELEMENT (app->pipeline), NULL, NULL,
          SEEK_TIMEOUT);
    } else {
      set_update_scale (app, TRUE);
    }
  } else {
    g_print ("seek failed\n");
    set_update_scale (app, TRUE);
  }
}

static void
seek_cb (GtkRange * range, PlaybackApp * app)
{
  gint64 real;

  real =
      gtk_range_get_value (GTK_RANGE (app->seek_scale)) * app->duration /
      N_GRAD;

  GST_DEBUG ("value=%f, real=%" G_GINT64_FORMAT,
      gtk_range_get_value (GTK_RANGE (app->seek_scale)), real);

  GST_DEBUG ("do seek");
  do_seek (app, GST_FORMAT_TIME, real);

  if (app->play_scrub) {
    if (app->buffering) {
      GST_DEBUG ("do scrub seek, waiting for buffering");
    } else {
      GST_DEBUG ("do scrub seek, PLAYING");
      gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
    }

    if (app->seek_timeout_id == 0) {
      app->seek_timeout_id =
          g_timeout_add (SCRUB_TIME, (GSourceFunc) end_scrub, app);
    }
  }
}

static void
advanced_seek_button_cb (GtkButton * button, PlaybackApp * app)
{
  GstFormat fmt;
  gint64 pos;
  const gchar *text;
  gchar *endptr;

  if (!app->seek_format)
    return;

  fmt = app->seek_format->value;

  text = gtk_entry_get_text (GTK_ENTRY (app->seek_entry));

  pos = g_ascii_strtoll (text, &endptr, 10);
  if (endptr != text && pos != G_MAXINT64 && pos != G_MININT64) {
    do_seek (app, fmt, pos);
  } else if (strlen (text) == 0) {
    do_seek (app, fmt, GST_CLOCK_TIME_NONE);
  }
}

static void
set_update_fill (PlaybackApp * app, gboolean active)
{
  GST_DEBUG ("fill scale is %d", active);

  if (active) {
    if (app->fill_id == 0) {
      app->fill_id =
          g_timeout_add (FILL_INTERVAL, (GSourceFunc) update_fill, app);
    }
  } else {
    if (app->fill_id) {
      g_source_remove (app->fill_id);
      app->fill_id = 0;
    }
  }
}

static void
set_update_scale (PlaybackApp * app, gboolean active)
{
  GST_DEBUG ("update scale is %d", active);

  if (active) {
    if (app->update_id == 0) {
      app->update_id =
          g_timeout_add (UPDATE_INTERVAL, (GSourceFunc) update_scale, app);
    }
    if (app->slow_update_id == 0) {
      app->slow_update_id =
          g_timeout_add (SLOW_UPDATE_INTERVAL, (GSourceFunc) update_seek_range,
          app);
    }
  } else {
    if (app->update_id) {
      g_source_remove (app->update_id);
      app->update_id = 0;
    }
    if (app->slow_update_id) {
      g_source_remove (app->slow_update_id);
      app->slow_update_id = 0;
    }
  }
}

static gboolean
start_seek (GtkRange * range, GdkEventButton * event, PlaybackApp * app)
{
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  set_update_scale (app, FALSE);

  if (app->state == GST_STATE_PLAYING && app->flush_seek && app->scrub) {
    GST_DEBUG ("start scrub seek, PAUSE");
    gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
  }

  if (app->changed_id == 0 && app->flush_seek && app->scrub) {
    app->changed_id =
        g_signal_connect (app->seek_scale, "value-changed",
        G_CALLBACK (seek_cb), app);
  }

  return FALSE;
}

static gboolean
stop_seek (GtkRange * range, GdkEventButton * event, PlaybackApp * app)
{
  if (app->changed_id) {
    g_signal_handler_disconnect (app->seek_scale, app->changed_id);
    app->changed_id = 0;
  }

  if (!app->flush_seek || !app->scrub) {
    gint64 real;

    GST_DEBUG ("do final seek");
    real =
        gtk_range_get_value (GTK_RANGE (app->seek_scale)) * app->duration /
        N_GRAD;
    do_seek (app, GST_FORMAT_TIME, real);
  }

  if (app->seek_timeout_id != 0) {
    g_source_remove (app->seek_timeout_id);
    app->seek_timeout_id = 0;
    /* Still scrubbing, so the pipeline is playing, see if we need PAUSED
     * instead. */
    if (app->state == GST_STATE_PAUSED) {
      GST_DEBUG ("stop scrub seek, PAUSED");
      gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
    }
  } else {
    if (app->state == GST_STATE_PLAYING) {
      if (app->buffering) {
        GST_DEBUG ("stop scrub seek, waiting for buffering");
      } else {
        GST_DEBUG ("stop scrub seek, PLAYING");
        gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
      }
    }
  }

  return FALSE;
}

static void
play_cb (GtkButton * button, PlaybackApp * app)
{
  GstStateChangeReturn ret;

  if (app->state != GST_STATE_PLAYING) {
    g_print ("PLAY pipeline\n");
    gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar), app->status_id);

    if (app->pipeline_type == 0) {
      video_sink_activate_cb (GTK_ENTRY (app->video_sink_entry), app);
      audio_sink_activate_cb (GTK_ENTRY (app->audio_sink_entry), app);
      text_sink_activate_cb (GTK_ENTRY (app->text_sink_entry), app);
      buffer_size_activate_cb (GTK_ENTRY (app->buffer_size_entry), app);
      buffer_duration_activate_cb (GTK_ENTRY (app->buffer_duration_entry), app);
      ringbuffer_maxsize_activate_cb (GTK_ENTRY (app->ringbuffer_maxsize_entry),
          app);
      connection_speed_activate_cb (GTK_ENTRY (app->connection_speed_entry),
          app);
      av_offset_activate_cb (GTK_ENTRY (app->av_offset_entry), app);
      text_offset_activate_cb (GTK_ENTRY (app->text_offset_entry), app);
      subtitle_encoding_activate_cb (GTK_ENTRY (app->subtitle_encoding_entry),
          app);
    }

    ret = gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
    switch (ret) {
      case GST_STATE_CHANGE_FAILURE:
        goto failed;
      case GST_STATE_CHANGE_NO_PREROLL:
        app->is_live = TRUE;
        break;
      default:
        break;
    }
    app->state = GST_STATE_PLAYING;
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Playing");
  }

  return;

failed:
  {
    g_print ("PLAY failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Play failed");
  }
}

static void
pause_cb (GtkButton * button, PlaybackApp * app)
{
  g_mutex_lock (&app->state_mutex);
  if (app->state != GST_STATE_PAUSED) {
    GstStateChangeReturn ret;

    gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar), app->status_id);
    g_print ("PAUSE pipeline\n");
    ret = gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
    switch (ret) {
      case GST_STATE_CHANGE_FAILURE:
        goto failed;
      case GST_STATE_CHANGE_NO_PREROLL:
        app->is_live = TRUE;
        break;
      default:
        break;
    }

    app->state = GST_STATE_PAUSED;
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Paused");
  }
  g_mutex_unlock (&app->state_mutex);

  return;

failed:
  {
    g_mutex_unlock (&app->state_mutex);
    g_print ("PAUSE failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Pause failed");
  }
}

static void
stop_cb (GtkButton * button, PlaybackApp * app)
{
  if (app->state != STOP_STATE) {
    GstStateChangeReturn ret;
    gint i;

    g_print ("READY pipeline\n");
    gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar), app->status_id);

    g_mutex_lock (&app->state_mutex);
    ret = gst_element_set_state (app->pipeline, STOP_STATE);
    if (ret == GST_STATE_CHANGE_FAILURE)
      goto failed;

    app->state = STOP_STATE;
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Stopped");
    gtk_widget_queue_draw (app->video_window);

    app->is_live = FALSE;
    app->buffering = FALSE;
    set_update_scale (app, FALSE);
    set_scale (app, 0.0);
    set_update_fill (app, FALSE);

    if (app->pipeline_type == 0)
      clear_streams (app);
    g_mutex_unlock (&app->state_mutex);

    gtk_widget_set_sensitive (GTK_WIDGET (app->seek_scale), TRUE);
    for (i = 0; i < G_N_ELEMENTS (app->navigation_buttons); i++)
      gtk_widget_set_sensitive (app->navigation_buttons[i].button, FALSE);
  }
  return;

failed:
  {
    g_mutex_unlock (&app->state_mutex);
    g_print ("STOP failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
        "Stop failed");
  }
}

static void
snap_before_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->snap_before = gtk_toggle_button_get_active (button);
}

static void
snap_after_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->snap_after = gtk_toggle_button_get_active (button);
}

static void
accurate_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->accurate_seek = gtk_toggle_button_get_active (button);
}

static void
key_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->keyframe_seek = gtk_toggle_button_get_active (button);
}

static void
loop_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->loop_seek = gtk_toggle_button_get_active (button);
  if (app->state == GST_STATE_PLAYING) {
    gint64 real;

    real =
        gtk_range_get_value (GTK_RANGE (app->seek_scale)) * app->duration /
        N_GRAD;
    do_seek (app, GST_FORMAT_TIME, real);
  }
}

static void
flush_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->flush_seek = gtk_toggle_button_get_active (button);
}

static void
scrub_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->scrub = gtk_toggle_button_get_active (button);
}

static void
play_scrub_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->play_scrub = gtk_toggle_button_get_active (button);
}

static void
instant_rate_change_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  app->instant_rate_change = gtk_toggle_button_get_active (button);
}

static void
skip_toggle_common (gboolean * v, GtkToggleButton * button, PlaybackApp * app)
{
  *v = gtk_toggle_button_get_active (button);
  if (app->state == GST_STATE_PLAYING) {
    gint64 real;

    real =
        gtk_range_get_value (GTK_RANGE (app->seek_scale)) * app->duration /
        N_GRAD;
    do_seek (app, GST_FORMAT_TIME, real);
  }
}

static void
skip_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  skip_toggle_common (&app->skip_seek, button, app);
}

static void
skip_key_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  skip_toggle_common (&app->skip_seek_key_only, button, app);
}

static void
skip_audio_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  skip_toggle_common (&app->skip_seek_no_audio, button, app);
}

static void
rate_spinbutton_changed_cb (GtkSpinButton * button, PlaybackApp * app)
{
  gboolean res = FALSE;
  GstEvent *s_event;
  GstSeekFlags flags;
  GstClockTime start, stop;
  GstSeekType start_type = GST_SEEK_TYPE_SET;
  GstSeekType stop_type = GST_SEEK_TYPE_SET;

  app->rate = gtk_spin_button_get_value (button);

  GST_DEBUG ("rate changed to %lf", app->rate);

  flags = 0;
  if (app->flush_seek)
    flags |= GST_SEEK_FLAG_FLUSH;
  if (app->loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (app->accurate_seek)
    flags |= GST_SEEK_FLAG_ACCURATE;
  if (app->keyframe_seek)
    flags |= GST_SEEK_FLAG_KEY_UNIT;
  if (app->skip_seek)
    flags |= GST_SEEK_FLAG_TRICKMODE;
  if (app->skip_seek_key_only)
    flags |= GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
  if (app->skip_seek_no_audio)
    flags |= GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;

  if (app->instant_rate_change) {
    flags |= GST_SEEK_FLAG_INSTANT_RATE_CHANGE;
    start_type = stop_type = GST_SEEK_TYPE_NONE;
    start = stop = GST_CLOCK_TIME_NONE;
    if (app->flush_seek) {
      g_warning ("Instant rate change seek not supported with flushing");
      return;
    }
  } else if (app->rate < 0) {
    stop = app->position;
    start = 0;
  } else {
    start = app->position;
    stop = GST_CLOCK_TIME_NONE;
  }

  if (app->rate >= 0.0) {
    s_event = gst_event_new_seek (app->rate,
        GST_FORMAT_TIME, flags, start_type, start, stop_type, stop);
  } else {
    s_event = gst_event_new_seek (app->rate,
        GST_FORMAT_TIME, flags, start_type, start, stop_type, stop);
  }

  res = send_event (app, s_event);

  if (res) {
    if (app->flush_seek) {
      gst_element_get_state (GST_ELEMENT (app->pipeline), NULL, NULL,
          SEEK_TIMEOUT);
    }
  } else
    g_print ("seek failed\n");
}

static void
update_flag (GstElement * pipeline, GstPlayFlags flag, gboolean state)
{
  gint flags;

  g_print ("%ssetting flag 0x%08x\n", (state ? "" : "un"), flag);

  g_object_get (pipeline, "flags", &flags, NULL);
  if (state)
    flags |= flag;
  else
    flags &= ~(flag);
  g_object_set (pipeline, "flags", flags, NULL);
}

static void
vis_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_VIS, state);
  gtk_widget_set_sensitive (app->vis_combo, state);
}

static void
audio_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_AUDIO, state);
  gtk_widget_set_sensitive (app->audio_combo, state);
}

static void
video_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_VIDEO, state);
  gtk_widget_set_sensitive (app->video_combo, state);
}

static void
text_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_TEXT, state);
  gtk_widget_set_sensitive (app->text_combo, state);
}

static void
mute_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean mute;

  mute = gtk_toggle_button_get_active (button);
  g_object_set (app->pipeline, "mute", mute, NULL);
}

static void
download_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_DOWNLOAD, state);
}

static void
buffering_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_BUFFERING, state);
}

static void
soft_volume_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_SOFT_VOLUME, state);
}

static void
native_audio_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_NATIVE_AUDIO, state);
}

static void
native_video_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_NATIVE_VIDEO, state);
}

static void
deinterlace_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_DEINTERLACE, state);
}

static void
soft_colorbalance_toggle_cb (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (app->pipeline, GST_PLAY_FLAG_SOFT_COLORBALANCE, state);
}

static void
clear_streams (PlaybackApp * app)
{
  gint i;

  /* remove previous info */
  for (i = 0; i < app->n_video; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (app->video_combo), 0);
  for (i = 0; i < app->n_audio; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (app->audio_combo), 0);
  for (i = 0; i < app->n_text; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (app->text_combo), 0);

  app->n_audio = app->n_video = app->n_text = 0;
  gtk_widget_set_sensitive (app->video_combo, FALSE);
  gtk_widget_set_sensitive (app->audio_combo, FALSE);
  gtk_widget_set_sensitive (app->text_combo, FALSE);

  app->need_streams = TRUE;
}

static void
update_streams (PlaybackApp * app)
{
  gint i;

  if (app->pipeline_type == 0 && app->need_streams) {
    GstTagList *tags;
    gchar *name, *str;
    gint active_idx;
    gboolean state;

    /* remove previous info */
    clear_streams (app);

    /* here we get and update the different streams detected by playbin */
    g_object_get (app->pipeline, "n-video", &app->n_video, NULL);
    g_object_get (app->pipeline, "n-audio", &app->n_audio, NULL);
    g_object_get (app->pipeline, "n-text", &app->n_text, NULL);

    g_print ("video %d, audio %d, text %d\n", app->n_video, app->n_audio,
        app->n_text);

    active_idx = 0;
    for (i = 0; i < app->n_video; i++) {
      g_signal_emit_by_name (app->pipeline, "get-video-tags", i, &tags);
      if (tags) {
        str = gst_tag_list_to_string (tags);
        g_print ("video %d: %s\n", i, str);
        g_free (str);
        gst_tag_list_unref (tags);
      }
      /* find good name for the label */
      name = g_strdup_printf ("video %d", i + 1);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->video_combo),
          name);
      g_free (name);
    }
    state =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->video_checkbox));
    gtk_widget_set_sensitive (app->video_combo, state && app->n_video > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (app->video_combo), active_idx);

    active_idx = 0;
    for (i = 0; i < app->n_audio; i++) {
      g_signal_emit_by_name (app->pipeline, "get-audio-tags", i, &tags);
      if (tags) {
        str = gst_tag_list_to_string (tags);
        g_print ("audio %d: %s\n", i, str);
        g_free (str);
        gst_tag_list_unref (tags);
      }
      /* find good name for the label */
      name = g_strdup_printf ("audio %d", i + 1);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->audio_combo),
          name);
      g_free (name);
    }
    state =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->audio_checkbox));
    gtk_widget_set_sensitive (app->audio_combo, state && app->n_audio > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (app->audio_combo), active_idx);

    active_idx = 0;
    for (i = 0; i < app->n_text; i++) {
      g_signal_emit_by_name (app->pipeline, "get-text-tags", i, &tags);

      name = NULL;
      if (tags) {
        const GValue *value;

        str = gst_tag_list_to_string (tags);
        g_print ("text %d: %s\n", i, str);
        g_free (str);

        /* get the language code if we can */
        value = gst_tag_list_get_value_index (tags, GST_TAG_LANGUAGE_CODE, 0);
        if (value && G_VALUE_HOLDS_STRING (value)) {
          name = g_strdup_printf ("text %s", g_value_get_string (value));
        }
        gst_tag_list_unref (tags);
      }
      /* find good name for the label if we didn't use a tag */
      if (name == NULL)
        name = g_strdup_printf ("text %d", i + 1);

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->text_combo),
          name);
      g_free (name);
    }
    state =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->text_checkbox));
    gtk_widget_set_sensitive (app->text_combo, state && app->n_text > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (app->text_combo), active_idx);

    app->need_streams = FALSE;
  }
}

static void
video_combo_cb (GtkComboBox * combo, PlaybackApp * app)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current video track %d\n", active);
  g_object_set (app->pipeline, "current-video", active, NULL);
}

static void
audio_combo_cb (GtkComboBox * combo, PlaybackApp * app)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current audio track %d\n", active);
  g_object_set (app->pipeline, "current-audio", active, NULL);
}

static void
text_combo_cb (GtkComboBox * combo, PlaybackApp * app)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current text track %d\n", active);
  g_object_set (app->pipeline, "current-text", active, NULL);
}

static gboolean
filter_vis_features (GstPluginFeature * feature, gpointer data)
{
  GstElementFactory *f;
  const gchar *klass;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;
  f = GST_ELEMENT_FACTORY (feature);
  klass = gst_element_factory_get_metadata (f, GST_ELEMENT_METADATA_KLASS);
  if (!g_strrstr (klass, "Visualization"))
    return FALSE;

  return TRUE;
}

static void
init_visualization_features (PlaybackApp * app)
{
  GList *list, *walk;

  app->vis_entries = g_array_new (FALSE, FALSE, sizeof (VisEntry));

  list = gst_registry_feature_filter (gst_registry_get (),
      filter_vis_features, FALSE, NULL);

  for (walk = list; walk; walk = g_list_next (walk)) {
    VisEntry entry;
    const gchar *name;

    entry.factory = GST_ELEMENT_FACTORY (walk->data);
    name = gst_element_factory_get_metadata (entry.factory,
        GST_ELEMENT_METADATA_LONGNAME);

    g_array_append_val (app->vis_entries, entry);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->vis_combo), name);
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX (app->vis_combo), 0);
  gst_plugin_feature_list_free (list);
}

static void
vis_combo_cb (GtkComboBox * combo, PlaybackApp * app)
{
  guint index;
  VisEntry *entry;
  GstElement *element;

  /* get the selected index and get the factory for this index */
  index = gtk_combo_box_get_active (GTK_COMBO_BOX (app->vis_combo));
  if (app->vis_entries->len > 0) {
    entry = &g_array_index (app->vis_entries, VisEntry, index);

    /* create an instance of the element from the factory */
    element = gst_element_factory_create (entry->factory, NULL);
    if (!element)
      return;

    /* set vis plugin for playbin */
    g_object_set (app->pipeline, "vis-plugin", element, NULL);
  }
}

static void
volume_spinbutton_changed_cb (GtkSpinButton * button, PlaybackApp * app)
{
  gdouble volume;

  volume = gtk_spin_button_get_value (button);

  g_object_set (app->pipeline, "volume", volume, NULL);
}

static gboolean
volume_notify_idle_cb (PlaybackApp * app)
{
  gdouble cur_volume, new_volume;

  g_object_get (app->pipeline, "volume", &new_volume, NULL);
  cur_volume =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (app->volume_spinbutton));
  if (fabs (cur_volume - new_volume) > 0.001) {
    g_signal_handlers_block_by_func (app->volume_spinbutton,
        volume_spinbutton_changed_cb, app);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->volume_spinbutton),
        new_volume);
    g_signal_handlers_unblock_by_func (app->volume_spinbutton,
        volume_spinbutton_changed_cb, app);
  }

  return FALSE;
}

static void
volume_notify_cb (GstElement * pipeline, GParamSpec * arg, PlaybackApp * app)
{
  /* Do this from the main thread */
  g_idle_add ((GSourceFunc) volume_notify_idle_cb, app);
}

static gboolean
mute_notify_idle_cb (PlaybackApp * app)
{
  gboolean cur_mute, new_mute;

  g_object_get (app->pipeline, "mute", &new_mute, NULL);
  cur_mute =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->mute_checkbox));
  if (cur_mute != new_mute) {
    g_signal_handlers_block_by_func (app->mute_checkbox, mute_toggle_cb, app);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->mute_checkbox),
        new_mute);
    g_signal_handlers_unblock_by_func (app->mute_checkbox, mute_toggle_cb, app);
  }

  return FALSE;
}

static void
mute_notify_cb (GstElement * pipeline, GParamSpec * arg, PlaybackApp * app)
{
  /* Do this from the main thread */
  g_idle_add ((GSourceFunc) mute_notify_idle_cb, app);
}

static void
shot_cb (GtkButton * button, PlaybackApp * app)
{
  GstSample *sample = NULL;
  GstCaps *caps;

  GST_DEBUG ("taking snapshot");

  /* convert to our desired format (RGB24) */
  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "RGB",
      /* Note: we don't ask for a specific width/height here, so that
       * videoscale can adjust dimensions from a non-1/1 pixel aspect
       * ratio to a 1/1 pixel-aspect-ratio */
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

  /* convert the latest sample to the requested format */
  g_signal_emit_by_name (app->pipeline, "convert-sample", caps, &sample);
  gst_caps_unref (caps);

  if (sample) {
    GstBuffer *buffer;
    GstCaps *caps;
    GstStructure *s;
    gboolean res;
    gint width, height;
    GdkPixbuf *pixbuf;
    GError *error = NULL;
    GstMapInfo map;

    /* get the snapshot buffer format now. We set the caps on the appsink so
     * that it can only be an rgb buffer. The only thing we have not specified
     * on the caps is the height, which is dependent on the pixel-aspect-ratio
     * of the source material */
    caps = gst_sample_get_caps (sample);
    if (!caps) {
      g_warning ("could not get snapshot format\n");
      goto done;
    }
    s = gst_caps_get_structure (caps, 0);

    /* we need to get the final caps on the buffer to get the size */
    res = gst_structure_get_int (s, "width", &width);
    res |= gst_structure_get_int (s, "height", &height);
    if (!res) {
      g_warning ("could not get snapshot dimension\n");
      goto done;
    }

    /* create pixmap from buffer and save, gstreamer video buffers have a stride
     * that is rounded up to the nearest multiple of 4 */
    buffer = gst_sample_get_buffer (sample);
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    pixbuf = gdk_pixbuf_new_from_data (map.data,
        GDK_COLORSPACE_RGB, FALSE, 8, width, height,
        GST_ROUND_UP_4 (width * 3), NULL, NULL);

    /* save the pixbuf */
    gdk_pixbuf_save (pixbuf, "snapshot.png", "png", &error, NULL);
    gst_buffer_unmap (buffer, &map);
    g_clear_error (&error);

  done:
    gst_sample_unref (sample);
  }
}

/* called when the Step button is pressed */
static void
step_cb (GtkButton * button, PlaybackApp * app)
{
  GstEvent *event;
  GstFormat format;
  guint64 amount;
  gdouble rate;
  gboolean flush, res;
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (app->step_format_combo));
  amount =
      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON
      (app->step_amount_spinbutton));
  rate =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (app->step_rate_spinbutton));
  flush = TRUE;

  switch (active) {
    case 0:
      format = GST_FORMAT_BUFFERS;
      break;
    case 1:
      format = GST_FORMAT_TIME;
      amount *= GST_MSECOND;
      break;
    default:
      format = GST_FORMAT_UNDEFINED;
      break;
  }

  event = gst_event_new_step (format, amount, rate, flush, FALSE);

  res = send_event (app, event);

  if (!res) {
    g_print ("Sending step event failed\n");
  }
}

static void
message_received (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
}

static void
do_shuttle (PlaybackApp * app)
{
  guint64 duration;

  if (app->shuttling)
    duration = 40 * GST_MSECOND;
  else
    duration = 0;

  gst_element_send_event (app->pipeline,
      gst_event_new_step (GST_FORMAT_TIME, duration, app->shuttle_rate, FALSE,
          FALSE));
}

static void
msg_sync_step_done (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  GstFormat format;
  guint64 amount;
  gdouble rate;
  gboolean flush;
  gboolean intermediate;
  guint64 duration;
  gboolean eos;

  gst_message_parse_step_done (message, &format, &amount, &rate, &flush,
      &intermediate, &duration, &eos);

  if (eos) {
    g_print ("stepped till EOS\n");
    return;
  }

  if (g_mutex_trylock (&app->state_mutex)) {
    if (app->shuttling)
      do_shuttle (app);
    g_mutex_unlock (&app->state_mutex);
  } else {
    /* ignore step messages that come while we are doing a state change */
    g_print ("state change is busy\n");
  }
}

static void
shuttle_toggled (GtkToggleButton * button, PlaybackApp * app)
{
  gboolean active;

  active = gtk_toggle_button_get_active (button);

  if (active != app->shuttling) {
    app->shuttling = active;
    g_print ("shuttling %s\n", app->shuttling ? "active" : "inactive");
    if (active) {
      app->shuttle_rate = 0.0;
      app->play_rate = 1.0;
      pause_cb (NULL, app);
      gst_element_get_state (app->pipeline, NULL, NULL, -1);
    }
  }
}

static void
shuttle_rate_switch (PlaybackApp * app)
{
  GstSeekFlags flags;
  GstEvent *s_event;
  gboolean res;

  if (app->state == GST_STATE_PLAYING) {
    /* pause when we need to */
    pause_cb (NULL, app);
    gst_element_get_state (app->pipeline, NULL, NULL, -1);
  }

  if (app->play_rate == 1.0)
    app->play_rate = -1.0;
  else
    app->play_rate = 1.0;

  g_print ("rate changed to %lf %" GST_TIME_FORMAT "\n", app->play_rate,
      GST_TIME_ARGS (app->position));

  flags = GST_SEEK_FLAG_FLUSH;
  flags |= GST_SEEK_FLAG_ACCURATE;

  if (app->play_rate >= 0.0) {
    s_event = gst_event_new_seek (app->play_rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, app->position,
        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    s_event = gst_event_new_seek (app->play_rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        GST_SEEK_TYPE_SET, app->position);
  }
  res = send_event (app, s_event);
  if (res) {
    gst_element_get_state (app->pipeline, NULL, NULL, SEEK_TIMEOUT);
  } else {
    g_print ("seek failed\n");
  }
}

static void
shuttle_value_changed (GtkRange * range, PlaybackApp * app)
{
  gdouble rate;

  rate = gtk_range_get_value (range);

  if (rate == 0.0) {
    g_print ("rate 0.0, pause\n");
    pause_cb (NULL, app);
    gst_element_get_state (app->pipeline, NULL, NULL, -1);
  } else {
    g_print ("rate changed %0.3g\n", rate);

    if ((rate < 0.0 && app->play_rate > 0.0) || (rate > 0.0
            && app->play_rate < 0.0)) {
      shuttle_rate_switch (app);
    }

    app->shuttle_rate = ABS (rate);
    if (app->state != GST_STATE_PLAYING) {
      do_shuttle (app);
      play_cb (NULL, app);
    }
  }
}

static void
colorbalance_value_changed (GtkRange * range, PlaybackApp * app)
{
  const gchar *label;
  gdouble val;
  gint ival;
  GstColorBalanceChannel *channel = NULL;
  const GList *channels, *l;

  if (range == GTK_RANGE (app->contrast_scale))
    label = "CONTRAST";
  else if (range == GTK_RANGE (app->brightness_scale))
    label = "BRIGHTNESS";
  else if (range == GTK_RANGE (app->hue_scale))
    label = "HUE";
  else if (range == GTK_RANGE (app->saturation_scale))
    label = "SATURATION";
  else
    g_return_if_reached ();

  val = gtk_range_get_value (range);

  g_print ("colorbalance %s value changed %lf\n", label, val / N_GRAD);

  if (!app->colorbalance_element) {
    find_interface_elements (app);
    if (!app->colorbalance_element)
      return;
  }

  channels =
      gst_color_balance_list_channels (GST_COLOR_BALANCE
      (app->colorbalance_element));
  for (l = channels; l; l = l->next) {
    GstColorBalanceChannel *tmp = l->data;

    if (g_strrstr (tmp->label, label)) {
      channel = tmp;
      break;
    }
  }

  if (!channel)
    return;

  ival =
      (gint) (0.5 + channel->min_value +
      (val / N_GRAD) * ((gdouble) channel->max_value -
          (gdouble) channel->min_value));
  gst_color_balance_set_value (GST_COLOR_BALANCE (app->colorbalance_element),
      channel, ival);
}

static void
seek_format_changed_cb (GtkComboBox * box, PlaybackApp * app)
{
  gchar *format_str;
  GList *l;
  const GstFormatDefinition *format = NULL;

  format_str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (box));

  for (l = app->formats; l; l = l->next) {
    const GstFormatDefinition *tmp = l->data;

    if (g_strcmp0 (tmp->nick, format_str) == 0) {
      format = tmp;
      break;
    }
  }

  if (!format)
    goto done;

  app->seek_format = format;
  update_scale (app);

done:
  g_free (format_str);
}

static void
update_formats (PlaybackApp * app)
{
  GstIterator *it;
  gboolean done;
  GList *l;
  GValue item = { 0, };
  gchar *selected;
  gint selected_idx = 0, i;

  selected =
      gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT
      (app->seek_format_combo));
  if (selected == NULL)
    selected = g_strdup ("time");

  it = gst_format_iterate_definitions ();
  done = FALSE;

  g_list_free (app->formats);
  app->formats = NULL;

  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:{
        GstFormatDefinition *def = g_value_get_pointer (&item);

        app->formats = g_list_prepend (app->formats, def);
        g_value_reset (&item);
        break;
      }
      case GST_ITERATOR_RESYNC:
        g_list_free (app->formats);
        app->formats = NULL;
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
      case GST_ITERATOR_DONE:
      default:
        done = TRUE;
        break;
    }
  }
  g_value_unset (&item);

  app->formats = g_list_reverse (app->formats);
  gst_iterator_free (it);

  g_signal_handlers_block_by_func (app->seek_format_combo,
      seek_format_changed_cb, app);
  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (app->seek_format_combo));

  for (i = 0, l = app->formats; l; l = l->next, i++) {
    const GstFormatDefinition *def = l->data;

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->seek_format_combo),
        def->nick);
    if (g_strcmp0 (def->nick, selected) == 0)
      selected_idx = i;
  }
  g_signal_handlers_unblock_by_func (app->seek_format_combo,
      seek_format_changed_cb, app);

  gtk_combo_box_set_active (GTK_COMBO_BOX (app->seek_format_combo),
      selected_idx);

  g_free (selected);
}

static void
msg_async_done (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  GST_DEBUG ("async done");

  /* Now query all available GstFormats */
  update_formats (app);

  /* when we get ASYNC_DONE we can query position, duration and other
   * properties */
  update_scale (app);

  /* update the available streams */
  update_streams (app);

  find_interface_elements (app);
}

static void
msg_state_changed (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);

  /* We only care about state changed on the pipeline */
  if (s && GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (app->pipeline)) {
    GstState old, new, pending;

    gst_message_parse_state_changed (message, &old, &new, &pending);

    /* When state of the pipeline changes to paused or playing we start updating scale */
    if (new == GST_STATE_PLAYING) {
      set_update_scale (app, TRUE);
    } else {
      set_update_scale (app, FALSE);
    }

    /* dump graph for (some) pipeline state changes */
    {
      gchar *dump_name;

      dump_name = g_strdup_printf ("seek.%s_%s",
          gst_element_state_get_name (old), gst_element_state_get_name (new));

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (app->pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, dump_name);

      g_free (dump_name);
    }
  }
}

static void
msg_segment_done (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  GstEvent *s_event;
  GstSeekFlags flags;
  gboolean res;
  GstFormat format;

  GST_DEBUG ("position is %" GST_TIME_FORMAT, GST_TIME_ARGS (app->position));
  gst_message_parse_segment_done (message, &format, &app->position);
  GST_DEBUG ("end of segment at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (app->position));

  flags = 0;
  /* in the segment-done callback we never flush as this would not make sense
   * for seamless playback. */
  if (app->loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (app->skip_seek)
    flags |= GST_SEEK_FLAG_TRICKMODE;
  if (app->skip_seek_key_only)
    flags |= GST_SEEK_FLAG_TRICKMODE_KEY_UNITS;
  if (app->skip_seek_no_audio)
    flags |= GST_SEEK_FLAG_TRICKMODE_NO_AUDIO;

  s_event = gst_event_new_seek (app->rate,
      GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
      GST_SEEK_TYPE_SET, app->duration);

  GST_DEBUG ("restart loop with rate %lf to 0 / %" GST_TIME_FORMAT,
      app->rate, GST_TIME_ARGS (app->duration));

  res = send_event (app, s_event);
  if (!res)
    g_print ("segment seek failed\n");
}

/* in stream buffering mode we PAUSE the pipeline until we receive a 100%
 * message */
static void
do_stream_buffering (PlaybackApp * app, gint percent)
{
  gchar *bufstr;

  gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar), app->status_id);
  bufstr = g_strdup_printf ("Buffering...%d", percent);
  gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id, bufstr);
  g_free (bufstr);

  if (percent == 100) {
    /* a 100% message means buffering is done */
    app->buffering = FALSE;
    /* if the desired state is playing, go back */
    if (app->state == GST_STATE_PLAYING) {
      /* no state management needed for live pipelines */
      if (!app->is_live) {
        fprintf (stderr, "Done buffering, setting pipeline to PLAYING ...\n");
        gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
      }
      gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar), app->status_id);
      gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
          "Playing");
    }
  } else {
    /* buffering busy */
    if (!app->buffering && app->state == GST_STATE_PLAYING) {
      /* we were not buffering but PLAYING, PAUSE  the pipeline. */
      if (!app->is_live) {
        fprintf (stderr, "Buffering, setting pipeline to PAUSED ...\n");
        gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
      }
    }
    app->buffering = TRUE;
  }
}

static void
do_download_buffering (PlaybackApp * app, gint percent)
{
  if (!app->buffering && percent < 100) {
    gchar *bufstr;

    app->buffering = TRUE;

    bufstr = g_strdup_printf ("Downloading...");
    gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id, bufstr);
    g_free (bufstr);

    /* once we get a buffering message, we'll do the fill update */
    set_update_fill (app, TRUE);

    if (app->state == GST_STATE_PLAYING && !app->is_live) {
      fprintf (stderr, "Downloading, setting pipeline to PAUSED ...\n");
      gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
      /* user has to manually start the playback */
      app->state = GST_STATE_PAUSED;
    }
  }
}

static void
msg_buffering (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  gint percent;

  gst_message_parse_buffering (message, &percent);

  /* get more stats */
  gst_message_parse_buffering_stats (message, &app->mode, NULL, NULL,
      &app->buffering_left);

  switch (app->mode) {
    case GST_BUFFERING_DOWNLOAD:
      do_download_buffering (app, percent);
      break;
    case GST_BUFFERING_LIVE:
      app->is_live = TRUE;
    case GST_BUFFERING_TIMESHIFT:
    case GST_BUFFERING_STREAM:
      do_stream_buffering (app, percent);
      break;
  }
}

static void
msg_clock_lost (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  g_print ("clock lost! PAUSE and PLAY to select a new clock\n");
  if (app->state == GST_STATE_PLAYING) {
    gst_element_set_state (app->pipeline, GST_STATE_PAUSED);
    gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
  }
}

static gboolean
is_valid_color_balance_element (GstElement * element)
{
  GstColorBalance *bal = GST_COLOR_BALANCE (element);
  gboolean have_brightness = FALSE;
  gboolean have_contrast = FALSE;
  gboolean have_hue = FALSE;
  gboolean have_saturation = FALSE;
  const GList *channels, *l;

  channels = gst_color_balance_list_channels (bal);
  for (l = channels; l; l = l->next) {
    GstColorBalanceChannel *ch = l->data;

    if (g_strrstr (ch->label, "BRIGHTNESS"))
      have_brightness = TRUE;
    else if (g_strrstr (ch->label, "CONTRAST"))
      have_contrast = TRUE;
    else if (g_strrstr (ch->label, "HUE"))
      have_hue = TRUE;
    else if (g_strrstr (ch->label, "SATURATION"))
      have_saturation = TRUE;
  }

  return have_brightness && have_contrast && have_hue && have_saturation;
}

static void
find_interface_elements (PlaybackApp * app)
{
  GstIterator *it;
  GValue item = { 0, };
  gboolean done = FALSE, hardware = FALSE;

  if (app->pipeline_type == 0)
    return;

  if (app->navigation_element)
    gst_object_unref (app->navigation_element);
  app->navigation_element = NULL;

  if (app->colorbalance_element)
    gst_object_unref (app->colorbalance_element);
  app->colorbalance_element = NULL;

  app->navigation_element =
      gst_bin_get_by_interface (GST_BIN (app->pipeline), GST_TYPE_NAVIGATION);

  it = gst_bin_iterate_all_by_interface (GST_BIN (app->pipeline),
      GST_TYPE_COLOR_BALANCE);
  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:{
        GstElement *element = GST_ELEMENT (g_value_get_object (&item));

        if (is_valid_color_balance_element (element)) {
          if (!app->colorbalance_element) {
            app->colorbalance_element =
                GST_ELEMENT_CAST (gst_object_ref (element));
            hardware =
                (gst_color_balance_get_balance_type (GST_COLOR_BALANCE
                    (element)) == GST_COLOR_BALANCE_HARDWARE);
          } else if (!hardware) {
            gboolean tmp =
                (gst_color_balance_get_balance_type (GST_COLOR_BALANCE
                    (element)) == GST_COLOR_BALANCE_HARDWARE);

            if (tmp) {
              if (app->colorbalance_element)
                gst_object_unref (app->colorbalance_element);
              app->colorbalance_element =
                  GST_ELEMENT_CAST (gst_object_ref (element));
              hardware = TRUE;
            }
          }
        }

        g_value_reset (&item);

        if (hardware && app->colorbalance_element)
          done = TRUE;
        break;
      }
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        done = FALSE;
        hardware = FALSE;
        if (app->colorbalance_element)
          gst_object_unref (app->colorbalance_element);
        app->colorbalance_element = NULL;
        break;
      case GST_ITERATOR_DONE:
      case GST_ITERATOR_ERROR:
      default:
        done = TRUE;
    }
  }

  g_value_unset (&item);
  gst_iterator_free (it);
}

/* called when Navigation command button is pressed */
static void
navigation_cmd_cb (GtkButton * button, PlaybackApp * app)
{
  GstNavigationCommand cmd = GST_NAVIGATION_COMMAND_INVALID;
  gint i;

  if (!app->navigation_element) {
    find_interface_elements (app);
    if (!app->navigation_element)
      return;
  }

  for (i = 0; i < G_N_ELEMENTS (app->navigation_buttons); i++) {
    if (app->navigation_buttons[i].button == GTK_WIDGET (button)) {
      cmd = app->navigation_buttons[i].cmd;
      break;
    }
  }

  if (cmd != GST_NAVIGATION_COMMAND_INVALID)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_command (cmd));
}

#if defined (GDK_WINDOWING_X11) || defined (GDK_WINDOWING_WIN32) || defined (GDK_WINDOWING_QUARTZ)
/* We set the xid here in response to the prepare-xwindow-id message via a
 * bus sync handler because we don't know the actual videosink used from the
 * start (as we don't know the pipeline, or bin elements such as autovideosink
 * or gconfvideosink may be used which create the actual videosink only once
 * the pipeline is started) */
static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  if (gst_is_video_overlay_prepare_window_handle_message (message)) {
    GstElement *element = GST_ELEMENT (GST_MESSAGE_SRC (message));

    if (app->overlay_element)
      gst_object_unref (app->overlay_element);
    app->overlay_element = GST_ELEMENT (gst_object_ref (element));

    g_print ("got prepare-xwindow-id, setting XID %" G_GUINTPTR_FORMAT "\n",
        app->embed_xid);

    /* Should have been initialised from main thread before (can't use
     * GDK_WINDOW_XID here with Gtk+ >= 2.18, because the sync handler will
     * be called from a streaming thread and GDK_WINDOW_XID maps to more than
     * a simple structure lookup with Gtk+ >= 2.18, where 'more' is stuff that
     * shouldn't be done from a non-GUI thread without explicit locking).  */
    g_assert (app->embed_xid != 0);

    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (element),
        app->embed_xid);

    find_interface_elements (app);
  }
  return GST_BUS_PASS;
}
#endif

static gboolean
draw_cb (GtkWidget * widget, cairo_t * cr, PlaybackApp * app)
{
  if (app->state < GST_STATE_PAUSED) {
    int width, height;

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);
    return TRUE;
  }

  /* "prepare-window-handle" message might not be posted yet depending on
   * videosink implementation */
  if (!app->overlay_element) {
    app->overlay_element = gst_bin_get_by_interface (GST_BIN (app->pipeline),
        GST_TYPE_VIDEO_OVERLAY);
  }

  if (app->overlay_element)
    gst_video_overlay_expose (GST_VIDEO_OVERLAY (app->overlay_element));

  return FALSE;
}

static void
realize_cb (GtkWidget * widget, PlaybackApp * app)
{
  GdkWindow *window = gtk_widget_get_window (widget);

  /* This is here just for pedagogical purposes, GDK_WINDOW_XID will call it
   * as well */
  if (!gdk_window_ensure_native (window))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

#if defined (GDK_WINDOWING_WIN32)
  app->embed_xid = (guintptr) GDK_WINDOW_HWND (window);
  g_print ("Window realize: video window HWND = %" G_GUINTPTR_FORMAT "\n",
      app->embed_xid);
#elif defined (GDK_WINDOWING_QUARTZ)
  app->embed_xid = (guintptr) gdk_quartz_window_get_nsview (window);
  g_print ("Window realize: video window NSView = %" G_GUINTPTR_FORMAT "\n",
      app->embed_xid);
#elif defined (GDK_WINDOWING_X11)
  app->embed_xid = GDK_WINDOW_XID (window);
  g_print ("Window realize: video window XID = %" G_GUINTPTR_FORMAT "\n",
      app->embed_xid);
#endif
}

static gboolean
button_press_cb (GtkWidget * widget, GdkEventButton * event, PlaybackApp * app)
{
  gtk_widget_grab_focus (widget);

  if (app->navigation_element)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_mouse_button_press (event->button, event->x,
            event->y, event->state));

  return FALSE;
}

static gboolean
button_release_cb (GtkWidget * widget, GdkEventButton * event,
    PlaybackApp * app)
{
  if (app->navigation_element)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_mouse_button_release (event->button, event->x,
            event->y, event->state));

  return FALSE;
}

static gboolean
key_press_cb (GtkWidget * widget, GdkEventKey * event, PlaybackApp * app)
{
  if (app->navigation_element)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_key_press (gdk_keyval_name (event->keyval),
            event->state));

  return FALSE;
}

static gboolean
key_release_cb (GtkWidget * widget, GdkEventKey * event, PlaybackApp * app)
{
  if (app->navigation_element)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_key_release (gdk_keyval_name (event->keyval),
            event->state));

  return FALSE;
}

static gboolean
motion_notify_cb (GtkWidget * widget, GdkEventMotion * event, PlaybackApp * app)
{
  if (app->navigation_element)
    gst_navigation_send_event_simple (GST_NAVIGATION (app->navigation_element),
        gst_navigation_event_new_mouse_move (event->x, event->y, event->state));

  return FALSE;
}

#ifdef G_OS_WIN32
/* On Windows, videosink elements expect full-size overlay window handle.
 * And an expectation for window resize scenario is that WM_SIZE event of
 * parent HWND is forwarded to child window.
 * But some GUI applications like GTK might handle WM_SIZE event by themselves
 * and therefore child HWND wouldn't be able to understand any resize event.
 *
 * In this example, we will watch "configure-event" of GTK widget and
 * GstVideoOverlay::set_render_rectangle() will be used to notify videosink
 * elements of resized render area.
 */
static gboolean
configure_event_cb (GtkWidget * widget, GdkEventConfigure * event,
    PlaybackApp * app)
{
  /* GdkEventConfigure::x and GdkEventConfigure::y are positions of this
   * video widget relative to parent. So, we should not forward it to videosink.
   * From videosink point of view, its parent window is this video widget */

  if (!app->overlay_element) {
    app->overlay_element = gst_bin_get_by_interface (GST_BIN (app->pipeline),
        GST_TYPE_VIDEO_OVERLAY);
  }

  if (app->overlay_element) {
    gst_video_overlay_set_render_rectangle (GST_VIDEO_OVERLAY
        (app->overlay_element), 0, 0, event->width, event->height);
  }

  return FALSE;
}
#endif

static void
msg_eos (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  message_received (bus, message, app);

  /* Set new uri for playerbins and continue playback */
  if (app->current_path && app->pipeline_type == 0) {
    stop_cb (NULL, app);
    app->current_path = g_list_next (app->current_path);
    app->current_sub_path = g_list_next (app->current_sub_path);
    if (app->current_path) {
      playbin_set_uri (app->pipeline, app->current_path->data,
          app->current_sub_path ? app->current_sub_path->data : NULL);
      play_cb (NULL, app);
    }
  }
}

static void
msg_step_done (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  if (!app->shuttling)
    message_received (bus, message, app);
}

static void
msg (GstBus * bus, GstMessage * message, PlaybackApp * app)
{
  GstNavigationMessageType nav_type;

  nav_type = gst_navigation_message_get_type (message);
  switch (nav_type) {
    case GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED:{
      GstQuery *query;
      gboolean res, j;

      /* Heuristic to detect if we're dealing with a DVD menu */
      query = gst_navigation_query_new_commands ();
      res = gst_element_query (GST_ELEMENT (GST_MESSAGE_SRC (message)), query);

      for (j = 0; j < G_N_ELEMENTS (app->navigation_buttons); j++)
        gtk_widget_set_sensitive (app->navigation_buttons[j].button, FALSE);

      if (res) {
        gboolean is_menu = FALSE;
        guint i, n;

        if (gst_navigation_query_parse_commands_length (query, &n)) {
          for (i = 0; i < n; i++) {
            GstNavigationCommand cmd;

            if (!gst_navigation_query_parse_commands_nth (query, i, &cmd))
              break;

            is_menu |= (cmd == GST_NAVIGATION_COMMAND_ACTIVATE);
            is_menu |= (cmd == GST_NAVIGATION_COMMAND_LEFT);
            is_menu |= (cmd == GST_NAVIGATION_COMMAND_RIGHT);
            is_menu |= (cmd == GST_NAVIGATION_COMMAND_UP);
            is_menu |= (cmd == GST_NAVIGATION_COMMAND_DOWN);

            for (j = 0; j < G_N_ELEMENTS (app->navigation_buttons); j++) {
              if (app->navigation_buttons[j].cmd != cmd)
                continue;

              gtk_widget_set_sensitive (app->navigation_buttons[j].button,
                  TRUE);
            }
          }
        }

        gtk_widget_set_sensitive (GTK_WIDGET (app->seek_scale), !is_menu);
      } else {
        g_assert_not_reached ();
      }

      gst_query_unref (query);
      message_received (bus, message, app);
      break;
    }
    default:
      break;
  }
}

static void
connect_bus_signals (PlaybackApp * app)
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));

#if defined (GDK_WINDOWING_X11) || defined (GDK_WINDOWING_WIN32) || defined (GDK_WINDOWING_QUARTZ)
  if (app->pipeline_type != 0) {
    /* handle prepare-xwindow-id element message synchronously, but only for non-playbin */
    gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler, app,
        NULL);
  }
#endif

  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  gst_bus_enable_sync_message_emission (bus);

  g_signal_connect (bus, "message::state-changed",
      G_CALLBACK (msg_state_changed), app);
  g_signal_connect (bus, "message::segment-done", G_CALLBACK (msg_segment_done),
      app);
  g_signal_connect (bus, "message::async-done", G_CALLBACK (msg_async_done),
      app);

  g_signal_connect (bus, "message::new-clock", G_CALLBACK (message_received),
      app);
  g_signal_connect (bus, "message::clock-lost", G_CALLBACK (msg_clock_lost),
      app);
  g_signal_connect (bus, "message::error", G_CALLBACK (message_received), app);
  g_signal_connect (bus, "message::warning", G_CALLBACK (message_received),
      app);
  g_signal_connect (bus, "message::eos", G_CALLBACK (msg_eos), app);
  g_signal_connect (bus, "message::tag", G_CALLBACK (message_received), app);
  g_signal_connect (bus, "message::element", G_CALLBACK (message_received),
      app);
  g_signal_connect (bus, "message::segment-done", G_CALLBACK (message_received),
      app);
  g_signal_connect (bus, "message::buffering", G_CALLBACK (msg_buffering), app);
//  g_signal_connect (bus, "message::step-done", G_CALLBACK (msg_step_done),
//      app);
  g_signal_connect (bus, "message::step-start", G_CALLBACK (msg_step_done),
      app);
  g_signal_connect (bus, "sync-message::step-done",
      G_CALLBACK (msg_sync_step_done), app);
  g_signal_connect (bus, "message", G_CALLBACK (msg), app);

  gst_object_unref (bus);
}

#if !GLIB_CHECK_VERSION(2,70,0)
#define g_pattern_spec_match_string g_pattern_match_string
#endif

/* Return GList of paths described in location string */
static GList *
handle_wildcards (const gchar * location)
{
  GList *res = NULL;
  gchar *path = g_path_get_dirname (location);
  gchar *pattern = g_path_get_basename (location);
  GPatternSpec *pspec = g_pattern_spec_new (pattern);
  GDir *dir = g_dir_open (path, 0, NULL);
  const gchar *name;

  g_print ("matching %s from %s\n", pattern, path);

  if (!dir) {
    g_print ("opening directory %s failed\n", path);
    goto out;
  }

  while ((name = g_dir_read_name (dir)) != NULL) {
    if (g_pattern_spec_match_string (pspec, name)) {
      res = g_list_append (res, g_strjoin ("/", path, name, NULL));
      g_print ("  found clip %s\n", name);
    }
  }

  g_dir_close (dir);
out:
  g_pattern_spec_free (pspec);
  g_free (pattern);
  g_free (path);

  return res;
}

static void
delete_event_cb (GtkWidget * widget, GdkEvent * event, PlaybackApp * app)
{
  stop_cb (NULL, app);
  gtk_main_quit ();
}

static void
video_sink_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  GstElement *sink = NULL;
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    sink = gst_element_factory_make_or_warn (text, NULL);
  }

  g_object_set (app->pipeline, "video-sink", sink, NULL);
}

static void
audio_sink_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  GstElement *sink = NULL;
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    sink = gst_element_factory_make_or_warn (text, NULL);
  }

  g_object_set (app->pipeline, "audio-sink", sink, NULL);
}

static void
text_sink_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  GstElement *sink = NULL;
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    sink = gst_element_factory_make_or_warn (text, NULL);
  }

  g_object_set (app->pipeline, "text-sink", sink, NULL);
}

static void
buffer_size_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    gint64 v;
    gchar *endptr;

    v = g_ascii_strtoll (text, &endptr, 10);
    if (endptr != text && v >= G_MININT && v <= G_MAXINT) {
      g_object_set (app->pipeline, "buffer-size", (gint) v, NULL);
    }
  }
}

static void
buffer_duration_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    gint64 v;
    gchar *endptr;

    v = g_ascii_strtoll (text, &endptr, 10);
    if (endptr != text && v != G_MAXINT64 && v != G_MININT64) {
      g_object_set (app->pipeline, "buffer-duration", v, NULL);
    }
  }
}

static void
ringbuffer_maxsize_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    guint64 v;
    gchar *endptr;

    v = g_ascii_strtoull (text, &endptr, 10);
    if (endptr != text && v != G_MAXUINT64) {
      g_object_set (app->pipeline, "ring-buffer-max-size", v, NULL);
    }
  }
}

static void
connection_speed_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  if (text != NULL && *text != '\0') {
    gint64 v;
    gchar *endptr;

    v = g_ascii_strtoll (text, &endptr, 10);
    if (endptr != text && v != G_MAXINT64 && v != G_MININT64) {
      g_object_set (app->pipeline, "connection-speed", v, NULL);
    }
  }
}

static void
subtitle_encoding_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;

  text = gtk_entry_get_text (entry);
  g_object_set (app->pipeline, "subtitle-encoding", text, NULL);
}

static void
subtitle_fontdesc_cb (GtkFontButton * button, PlaybackApp * app)
{
  gchar *text;

  text = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (button));
  g_object_set (app->pipeline, "subtitle-font-desc", text, NULL);
  g_free (text);
}

static gboolean
text_to_gint64 (const gchar * text, gint64 * result)
{
  if (text != NULL && *text != '\0') {
    gchar *endptr;

    *result = g_ascii_strtoll (text, &endptr, 10);
    return (endptr != text && *result != G_MAXINT64 && *result != G_MININT64);
  }
  return FALSE;
}

static void
av_offset_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;
  gint64 v;

  text = gtk_entry_get_text (entry);
  if (text_to_gint64 (text, &v))
    g_object_set (app->pipeline, "av-offset", v, NULL);
}

static void
text_offset_activate_cb (GtkEntry * entry, PlaybackApp * app)
{
  const gchar *text;
  gint64 v;

  text = gtk_entry_get_text (entry);
  if (text_to_gint64 (text, &v))
    g_object_set (app->pipeline, "text-offset", v, NULL);
}

static void
print_usage (int argc, char **argv)
{
  gint i;

  g_print ("Usage: %s <type> <argument>\n", argv[0]);
  g_print ("   possible types:\n");

  for (i = 0; i < G_N_ELEMENTS (pipelines); i++) {
    g_print ("     %d = %s %s\n", i, pipelines[i].name, pipelines[i].help);
  }
}

static void
create_ui (PlaybackApp * app)
{
  GtkWidget *hbox, *vbox, *seek, *playbin, *step, *navigation, *colorbalance;
  GtkWidget *play_button, *pause_button, *stop_button;
  GtkAdjustment *adjustment;

  /* initialize gui elements ... */
  app->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  app->video_window = gtk_drawing_area_new ();
  g_signal_connect (app->video_window, "draw", G_CALLBACK (draw_cb), app);
  g_signal_connect (app->video_window, "realize", G_CALLBACK (realize_cb), app);
  g_signal_connect (app->video_window, "button-press-event",
      G_CALLBACK (button_press_cb), app);
  g_signal_connect (app->video_window, "button-release-event",
      G_CALLBACK (button_release_cb), app);
  g_signal_connect (app->video_window, "key-press-event",
      G_CALLBACK (key_press_cb), app);
  g_signal_connect (app->video_window, "key-release-event",
      G_CALLBACK (key_release_cb), app);
  g_signal_connect (app->video_window, "motion-notify-event",
      G_CALLBACK (motion_notify_cb), app);
#ifdef G_OS_WIN32
  g_signal_connect (app->video_window, "configure-event",
      G_CALLBACK (configure_event_cb), app);
#endif
  gtk_widget_set_can_focus (app->video_window, TRUE);
  gtk_widget_add_events (app->video_window,
      GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

  app->statusbar = gtk_statusbar_new ();
  app->status_id =
      gtk_statusbar_get_context_id (GTK_STATUSBAR (app->statusbar),
      "playback-test");
  gtk_statusbar_push (GTK_STATUSBAR (app->statusbar), app->status_id,
      "Stopped");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /* media controls */
  play_button = gtk_button_new_from_icon_name ("media-playback-start",
      GTK_ICON_SIZE_BUTTON);
  pause_button = gtk_button_new_from_icon_name ("media-playback-pause",
      GTK_ICON_SIZE_BUTTON);
  stop_button = gtk_button_new_from_icon_name ("media-playback-stop",
      GTK_ICON_SIZE_BUTTON);

  /* seek expander */
  {
    GtkWidget *accurate_checkbox, *key_checkbox, *loop_checkbox;
    GtkWidget *flush_checkbox, *snap_before_checkbox, *snap_after_checkbox;
    GtkWidget *scrub_checkbox, *play_scrub_checkbox, *rate_label;
    GtkWidget *skip_checkbox, *skip_key_checkbox, *skip_audio_checkbox;
    GtkWidget *instant_checkbox, *rate_spinbutton;
    GtkWidget *flagtable, *advanced_seek, *advanced_seek_grid;
    GtkWidget *duration_label, *position_label, *seek_button;
    GtkWidget *start_label, *stop_label;

    seek = gtk_expander_new ("seek options");
    flagtable = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (flagtable), 2);
    gtk_grid_set_row_homogeneous (GTK_GRID (flagtable), FALSE);
    gtk_grid_set_column_spacing (GTK_GRID (flagtable), 2);
    gtk_grid_set_column_homogeneous (GTK_GRID (flagtable), FALSE);

    accurate_checkbox = gtk_check_button_new_with_label ("Accurate Playback");
    key_checkbox = gtk_check_button_new_with_label ("Key-unit Playback");
    loop_checkbox = gtk_check_button_new_with_label ("Loop");
    flush_checkbox = gtk_check_button_new_with_label ("Flush");
    scrub_checkbox = gtk_check_button_new_with_label ("Scrub");
    play_scrub_checkbox = gtk_check_button_new_with_label ("Play Scrub");
    instant_checkbox = gtk_check_button_new_with_label ("Instant Rate Change");
    skip_checkbox = gtk_check_button_new_with_label ("Trickmode Play");
    skip_key_checkbox =
        gtk_check_button_new_with_label ("Trickmode - Keyframes Only");
    skip_audio_checkbox =
        gtk_check_button_new_with_label ("Trickmode - No Audio");
    snap_before_checkbox = gtk_check_button_new_with_label ("Snap before");
    snap_after_checkbox = gtk_check_button_new_with_label ("Snap after");
    rate_spinbutton = gtk_spin_button_new_with_range (-100, 100, 0.1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rate_spinbutton), 3);
    rate_label = gtk_label_new ("Rate");

    gtk_widget_set_tooltip_text (accurate_checkbox,
        "accurate position is requested, this might be considerably slower for some formats");
    gtk_widget_set_tooltip_text (key_checkbox,
        "seek to the nearest keyframe. This might be faster but less accurate");
    gtk_widget_set_tooltip_text (loop_checkbox, "loop playback");
    gtk_widget_set_tooltip_text (flush_checkbox,
        "flush pipeline after seeking");
    gtk_widget_set_tooltip_text (rate_spinbutton,
        "define the playback rate, " "negative value trigger reverse playback");
    gtk_widget_set_tooltip_text (scrub_checkbox, "show images while seeking");
    gtk_widget_set_tooltip_text (play_scrub_checkbox,
        "play video while seeking");
    gtk_widget_set_tooltip_text (instant_checkbox, "do instant rate changes");
    gtk_widget_set_tooltip_text (skip_checkbox,
        "Skip frames while playing at high frame rates");
    gtk_widget_set_tooltip_text (skip_key_checkbox,
        "Skip everything except keyframes while playing at high frame rates");
    gtk_widget_set_tooltip_text (skip_audio_checkbox,
        "Do not decode audio during trick mode playback");
    gtk_widget_set_tooltip_text (snap_before_checkbox,
        "Favor snapping to the frame before the seek target");
    gtk_widget_set_tooltip_text (snap_after_checkbox,
        "Favor snapping to the frame after the seek target");

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (flush_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scrub_checkbox), TRUE);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (rate_spinbutton), app->rate);

    g_signal_connect (G_OBJECT (accurate_checkbox), "toggled",
        G_CALLBACK (accurate_toggle_cb), app);
    g_signal_connect (G_OBJECT (key_checkbox), "toggled",
        G_CALLBACK (key_toggle_cb), app);
    g_signal_connect (G_OBJECT (loop_checkbox), "toggled",
        G_CALLBACK (loop_toggle_cb), app);
    g_signal_connect (G_OBJECT (flush_checkbox), "toggled",
        G_CALLBACK (flush_toggle_cb), app);
    g_signal_connect (G_OBJECT (scrub_checkbox), "toggled",
        G_CALLBACK (scrub_toggle_cb), app);
    g_signal_connect (G_OBJECT (play_scrub_checkbox), "toggled",
        G_CALLBACK (play_scrub_toggle_cb), app);
    g_signal_connect (G_OBJECT (instant_checkbox), "toggled",
        G_CALLBACK (instant_rate_change_toggle_cb), app);
    g_signal_connect (G_OBJECT (skip_checkbox), "toggled",
        G_CALLBACK (skip_toggle_cb), app);
    g_signal_connect (G_OBJECT (skip_key_checkbox), "toggled",
        G_CALLBACK (skip_key_toggle_cb), app);
    g_signal_connect (G_OBJECT (skip_audio_checkbox), "toggled",
        G_CALLBACK (skip_audio_toggle_cb), app);
    g_signal_connect (G_OBJECT (rate_spinbutton), "value-changed",
        G_CALLBACK (rate_spinbutton_changed_cb), app);
    g_signal_connect (G_OBJECT (snap_before_checkbox), "toggled",
        G_CALLBACK (snap_before_toggle_cb), app);
    g_signal_connect (G_OBJECT (snap_after_checkbox), "toggled",
        G_CALLBACK (snap_after_toggle_cb), app);

    gtk_grid_attach (GTK_GRID (flagtable), accurate_checkbox, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), flush_checkbox, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), loop_checkbox, 2, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), key_checkbox, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), scrub_checkbox, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), play_scrub_checkbox, 2, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), instant_checkbox, 2, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), skip_checkbox, 3, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), skip_key_checkbox, 3, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), skip_audio_checkbox, 3, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), rate_label, 4, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), rate_spinbutton, 4, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), snap_before_checkbox, 0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (flagtable), snap_after_checkbox, 1, 2, 1, 1);

    advanced_seek = gtk_frame_new ("Advanced Seeking");
    advanced_seek_grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (advanced_seek_grid), 2);
    gtk_grid_set_row_homogeneous (GTK_GRID (advanced_seek_grid), FALSE);
    gtk_grid_set_column_spacing (GTK_GRID (advanced_seek_grid), 5);
    gtk_grid_set_column_homogeneous (GTK_GRID (advanced_seek_grid), FALSE);

    app->seek_format_combo = gtk_combo_box_text_new ();
    g_signal_connect (app->seek_format_combo, "changed",
        G_CALLBACK (seek_format_changed_cb), app);
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_format_combo, 0,
        0, 1, 1);

    app->seek_entry = gtk_entry_new ();
    gtk_entry_set_width_chars (GTK_ENTRY (app->seek_entry), 12);
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_entry, 0, 1, 1,
        1);

    seek_button = gtk_button_new_with_label ("Seek");
    g_signal_connect (G_OBJECT (seek_button), "clicked",
        G_CALLBACK (advanced_seek_button_cb), app);
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), seek_button, 1, 0, 1, 1);

    position_label = gtk_label_new ("Position:");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), position_label, 2, 0, 1, 1);
    duration_label = gtk_label_new ("Duration:");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), duration_label, 2, 1, 1, 1);

    app->seek_position_label = gtk_label_new ("-1");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_position_label, 3,
        0, 1, 1);
    app->seek_duration_label = gtk_label_new ("-1");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_duration_label, 3,
        1, 1, 1);

    start_label = gtk_label_new ("Seek start:");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), start_label, 4, 0, 1, 1);
    stop_label = gtk_label_new ("Seek stop:");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), stop_label, 4, 1, 1, 1);

    app->seek_start_label = gtk_label_new ("-1");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_start_label, 5,
        0, 1, 1);
    app->seek_stop_label = gtk_label_new ("-1");
    gtk_grid_attach (GTK_GRID (advanced_seek_grid), app->seek_stop_label, 5,
        1, 1, 1);

    gtk_container_add (GTK_CONTAINER (advanced_seek), advanced_seek_grid);
    gtk_grid_attach (GTK_GRID (flagtable), advanced_seek, 0, 3, 3, 2);
    gtk_container_add (GTK_CONTAINER (seek), flagtable);
  }

  /* step expander */
  {
    GtkWidget *hbox;
    GtkWidget *step_button, *shuttle_checkbox;

    step = gtk_expander_new ("step options");
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    app->step_format_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->step_format_combo),
        "frames");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (app->step_format_combo),
        "time (ms)");
    gtk_combo_box_set_active (GTK_COMBO_BOX (app->step_format_combo), 0);
    gtk_box_pack_start (GTK_BOX (hbox), app->step_format_combo, FALSE, FALSE,
        2);

    app->step_amount_spinbutton = gtk_spin_button_new_with_range (1, 1000, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (app->step_amount_spinbutton),
        0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->step_amount_spinbutton),
        1.0);
    gtk_box_pack_start (GTK_BOX (hbox), app->step_amount_spinbutton, FALSE,
        FALSE, 2);

    app->step_rate_spinbutton = gtk_spin_button_new_with_range (0.0, 100, 0.1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (app->step_rate_spinbutton), 3);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->step_rate_spinbutton),
        1.0);
    gtk_box_pack_start (GTK_BOX (hbox), app->step_rate_spinbutton, FALSE, FALSE,
        2);

    step_button =
        gtk_button_new_from_icon_name ("media-seek-forward",
        GTK_ICON_SIZE_BUTTON);
    gtk_button_set_label (GTK_BUTTON (step_button), "Step");
    gtk_box_pack_start (GTK_BOX (hbox), step_button, FALSE, FALSE, 2);

    g_signal_connect (G_OBJECT (step_button), "clicked", G_CALLBACK (step_cb),
        app);

    /* shuttle scale */
    shuttle_checkbox = gtk_check_button_new_with_label ("Shuttle");
    gtk_box_pack_start (GTK_BOX (hbox), shuttle_checkbox, FALSE, FALSE, 2);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (shuttle_checkbox), FALSE);
    g_signal_connect (shuttle_checkbox, "toggled", G_CALLBACK (shuttle_toggled),
        app);

    adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (0.0, -3.00, 4.0, 0.1, 1.0, 1.0));
    app->shuttle_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_digits (GTK_SCALE (app->shuttle_scale), 2);
    gtk_scale_set_value_pos (GTK_SCALE (app->shuttle_scale), GTK_POS_TOP);
    g_signal_connect (app->shuttle_scale, "value-changed",
        G_CALLBACK (shuttle_value_changed), app);
    g_signal_connect (app->shuttle_scale, "format_value",
        G_CALLBACK (shuttle_format_value), app);

    gtk_box_pack_start (GTK_BOX (hbox), app->shuttle_scale, TRUE, TRUE, 2);

    gtk_container_add (GTK_CONTAINER (step), hbox);
  }

  /* navigation command expander */
  {
    GtkWidget *navigation_button;
    GtkWidget *grid;
    gint i = 0;

    navigation = gtk_expander_new ("navigation commands");
    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), FALSE);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 2);
    gtk_grid_set_column_homogeneous (GTK_GRID (grid), FALSE);

    navigation_button = gtk_button_new_with_label ("Menu 1");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU1;

    navigation_button = gtk_button_new_with_label ("Menu 2");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Title Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU2;

    navigation_button = gtk_button_new_with_label ("Menu 3");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Root Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU3;

    navigation_button = gtk_button_new_with_label ("Menu 4");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Subpicture Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU4;

    navigation_button = gtk_button_new_with_label ("Menu 5");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Audio Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU5;

    navigation_button = gtk_button_new_with_label ("Menu 6");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Angle Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU6;

    navigation_button = gtk_button_new_with_label ("Menu 7");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i, 0, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    gtk_widget_set_tooltip_text (navigation_button, "DVD Chapter Menu");
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_MENU7;

    navigation_button = gtk_button_new_with_label ("Left");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_LEFT;

    navigation_button = gtk_button_new_with_label ("Right");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_RIGHT;

    navigation_button = gtk_button_new_with_label ("Up");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_UP;

    navigation_button = gtk_button_new_with_label ("Down");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_DOWN;

    navigation_button = gtk_button_new_with_label ("Activate");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_ACTIVATE;

    navigation_button = gtk_button_new_with_label ("Prev. Angle");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_PREV_ANGLE;

    navigation_button = gtk_button_new_with_label ("Next. Angle");
    g_signal_connect (G_OBJECT (navigation_button), "clicked",
        G_CALLBACK (navigation_cmd_cb), app);
    gtk_grid_attach (GTK_GRID (grid), navigation_button, i - 7, 1, 1, 1);
    gtk_widget_set_sensitive (navigation_button, FALSE);
    app->navigation_buttons[i].button = navigation_button;
    app->navigation_buttons[i++].cmd = GST_NAVIGATION_COMMAND_NEXT_ANGLE;

    gtk_container_add (GTK_CONTAINER (navigation), grid);
  }

  /* colorbalance expander */
  {
    GtkWidget *vbox, *frame;

    colorbalance = gtk_expander_new ("color balance options");
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    /* contrast scale */
    frame = gtk_frame_new ("Contrast");
    adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (N_GRAD / 2.0, 0.00, N_GRAD, 0.1,
            1.0, 1.0));
    app->contrast_scale =
        gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_draw_value (GTK_SCALE (app->contrast_scale), FALSE);
    g_signal_connect (app->contrast_scale, "value-changed",
        G_CALLBACK (colorbalance_value_changed), app);
    gtk_container_add (GTK_CONTAINER (frame), app->contrast_scale);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 2);

    /* brightness scale */
    frame = gtk_frame_new ("Brightness");
    adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (N_GRAD / 2.0, 0.00, N_GRAD, 0.1,
            1.0, 1.0));
    app->brightness_scale =
        gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_draw_value (GTK_SCALE (app->brightness_scale), FALSE);
    g_signal_connect (app->brightness_scale, "value-changed",
        G_CALLBACK (colorbalance_value_changed), app);
    gtk_container_add (GTK_CONTAINER (frame), app->brightness_scale);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 2);

    /* hue scale */
    frame = gtk_frame_new ("Hue");
    adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (N_GRAD / 2.0, 0.00, N_GRAD, 0.1,
            1.0, 1.0));
    app->hue_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_draw_value (GTK_SCALE (app->hue_scale), FALSE);
    g_signal_connect (app->hue_scale, "value-changed",
        G_CALLBACK (colorbalance_value_changed), app);
    gtk_container_add (GTK_CONTAINER (frame), app->hue_scale);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 2);

    /* saturation scale */
    frame = gtk_frame_new ("Saturation");
    adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (N_GRAD / 2.0, 0.00, N_GRAD, 0.1,
            1.0, 1.0));
    app->saturation_scale =
        gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_draw_value (GTK_SCALE (app->saturation_scale), FALSE);
    g_signal_connect (app->saturation_scale, "value-changed",
        G_CALLBACK (colorbalance_value_changed), app);
    gtk_container_add (GTK_CONTAINER (frame), app->saturation_scale);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 2);

    gtk_container_add (GTK_CONTAINER (colorbalance), vbox);
  }

  /* seek bar */
  adjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.00, N_GRAD, 0.1, 1.0, 1.0));
  app->seek_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_set_digits (GTK_SCALE (app->seek_scale), 2);
  gtk_scale_set_value_pos (GTK_SCALE (app->seek_scale), GTK_POS_RIGHT);
  gtk_range_set_show_fill_level (GTK_RANGE (app->seek_scale), TRUE);
  gtk_range_set_restrict_to_fill_level (GTK_RANGE (app->seek_scale), FALSE);
  gtk_range_set_fill_level (GTK_RANGE (app->seek_scale), N_GRAD);

  g_signal_connect (app->seek_scale, "button_press_event",
      G_CALLBACK (start_seek), app);
  g_signal_connect (app->seek_scale, "button_release_event",
      G_CALLBACK (stop_seek), app);
  g_signal_connect (app->seek_scale, "format_value", G_CALLBACK (format_value),
      app);

  if (app->pipeline_type == 0) {
    GtkWidget *pb2vbox, *boxes, *boxes2, *panel, *boxes3;
    GtkWidget *volume_label, *shot_button;
    GtkWidget *label;

    playbin = gtk_expander_new ("playbin options");
    /* the playbin panel controls for the video/audio/subtitle tracks */
    panel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    app->video_combo = gtk_combo_box_text_new ();
    app->audio_combo = gtk_combo_box_text_new ();
    app->text_combo = gtk_combo_box_text_new ();
    gtk_widget_set_sensitive (app->video_combo, FALSE);
    gtk_widget_set_sensitive (app->audio_combo, FALSE);
    gtk_widget_set_sensitive (app->text_combo, FALSE);
    gtk_box_pack_start (GTK_BOX (panel), app->video_combo, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (panel), app->audio_combo, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (panel), app->text_combo, TRUE, TRUE, 2);
    g_signal_connect (G_OBJECT (app->video_combo), "changed",
        G_CALLBACK (video_combo_cb), app);
    g_signal_connect (G_OBJECT (app->audio_combo), "changed",
        G_CALLBACK (audio_combo_cb), app);
    g_signal_connect (G_OBJECT (app->text_combo), "changed",
        G_CALLBACK (text_combo_cb), app);
    /* playbin panel for flag checkboxes and volume/mute */
    boxes = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (boxes), 2);
    gtk_grid_set_row_homogeneous (GTK_GRID (boxes), FALSE);
    gtk_grid_set_column_spacing (GTK_GRID (boxes), 2);
    gtk_grid_set_column_homogeneous (GTK_GRID (boxes), FALSE);

    app->video_checkbox = gtk_check_button_new_with_label ("Video");
    app->audio_checkbox = gtk_check_button_new_with_label ("Audio");
    app->text_checkbox = gtk_check_button_new_with_label ("Text");
    app->vis_checkbox = gtk_check_button_new_with_label ("Vis");
    app->soft_volume_checkbox = gtk_check_button_new_with_label ("Soft Volume");
    app->native_audio_checkbox =
        gtk_check_button_new_with_label ("Native Audio");
    app->native_video_checkbox =
        gtk_check_button_new_with_label ("Native Video");
    app->download_checkbox = gtk_check_button_new_with_label ("Download");
    app->buffering_checkbox = gtk_check_button_new_with_label ("Buffering");
    app->deinterlace_checkbox = gtk_check_button_new_with_label ("Deinterlace");
    app->soft_colorbalance_checkbox =
        gtk_check_button_new_with_label ("Soft Colorbalance");
    app->mute_checkbox = gtk_check_button_new_with_label ("Mute");
    volume_label = gtk_label_new ("Volume");
    app->volume_spinbutton = gtk_spin_button_new_with_range (0, 10.0, 0.1);

    gtk_grid_attach (GTK_GRID (boxes), app->video_checkbox, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->audio_checkbox, 1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->text_checkbox, 2, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->vis_checkbox, 3, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->soft_volume_checkbox, 4, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->native_audio_checkbox, 5, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->native_video_checkbox, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->download_checkbox, 1, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->buffering_checkbox, 2, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->deinterlace_checkbox, 3, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->soft_colorbalance_checkbox, 4, 1, 1,
        1);

    gtk_grid_attach (GTK_GRID (boxes), app->mute_checkbox, 6, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), volume_label, 5, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (boxes), app->volume_spinbutton, 6, 1, 1, 1);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->video_checkbox),
        TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->audio_checkbox),
        TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->text_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->vis_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->soft_volume_checkbox),
        TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
        (app->native_audio_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
        (app->native_video_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->download_checkbox),
        FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->buffering_checkbox),
        FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->deinterlace_checkbox),
        FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
        (app->soft_colorbalance_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->mute_checkbox),
        FALSE);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->volume_spinbutton), 1.0);

    g_signal_connect (G_OBJECT (app->video_checkbox), "toggled",
        G_CALLBACK (video_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->audio_checkbox), "toggled",
        G_CALLBACK (audio_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->text_checkbox), "toggled",
        G_CALLBACK (text_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->vis_checkbox), "toggled",
        G_CALLBACK (vis_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->soft_volume_checkbox), "toggled",
        G_CALLBACK (soft_volume_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->native_audio_checkbox), "toggled",
        G_CALLBACK (native_audio_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->native_video_checkbox), "toggled",
        G_CALLBACK (native_video_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->download_checkbox), "toggled",
        G_CALLBACK (download_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->buffering_checkbox), "toggled",
        G_CALLBACK (buffering_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->deinterlace_checkbox), "toggled",
        G_CALLBACK (deinterlace_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->soft_colorbalance_checkbox), "toggled",
        G_CALLBACK (soft_colorbalance_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->mute_checkbox), "toggled",
        G_CALLBACK (mute_toggle_cb), app);
    g_signal_connect (G_OBJECT (app->volume_spinbutton), "value-changed",
        G_CALLBACK (volume_spinbutton_changed_cb), app);
    /* playbin panel for snapshot */
    boxes2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    shot_button =
        gtk_button_new_from_icon_name ("document-save", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text (shot_button,
        "save a screenshot .png in the current directory");
    g_signal_connect (G_OBJECT (shot_button), "clicked", G_CALLBACK (shot_cb),
        app);
    app->vis_combo = gtk_combo_box_text_new ();
    g_signal_connect (G_OBJECT (app->vis_combo), "changed",
        G_CALLBACK (vis_combo_cb), app);
    gtk_widget_set_sensitive (app->vis_combo, FALSE);
    gtk_box_pack_start (GTK_BOX (boxes2), shot_button, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes2), app->vis_combo, TRUE, TRUE, 2);

    /* fill the vis combo box and the array of factories */
    init_visualization_features (app);

    /* Grid with other properties */
    boxes3 = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (boxes3), 2);
    gtk_grid_set_row_homogeneous (GTK_GRID (boxes3), FALSE);
    gtk_grid_set_column_spacing (GTK_GRID (boxes3), 2);
    gtk_grid_set_column_homogeneous (GTK_GRID (boxes3), FALSE);

    label = gtk_label_new ("Video sink");
    gtk_grid_attach (GTK_GRID (boxes3), label, 0, 0, 1, 1);
    app->video_sink_entry = gtk_entry_new ();
    g_signal_connect (app->video_sink_entry, "activate",
        G_CALLBACK (video_sink_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->video_sink_entry, 0, 1, 1, 1);

    label = gtk_label_new ("Audio sink");
    gtk_grid_attach (GTK_GRID (boxes3), label, 1, 0, 1, 1);
    app->audio_sink_entry = gtk_entry_new ();
    g_signal_connect (app->audio_sink_entry, "activate",
        G_CALLBACK (audio_sink_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->audio_sink_entry, 1, 1, 1, 1);

    label = gtk_label_new ("Text sink");
    gtk_grid_attach (GTK_GRID (boxes3), label, 2, 0, 1, 1);
    app->text_sink_entry = gtk_entry_new ();
    g_signal_connect (app->text_sink_entry, "activate",
        G_CALLBACK (text_sink_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->text_sink_entry, 2, 1, 1, 1);

    label = gtk_label_new ("Buffer Size");
    gtk_grid_attach (GTK_GRID (boxes3), label, 0, 2, 1, 1);
    app->buffer_size_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (app->buffer_size_entry), "-1");
    g_signal_connect (app->buffer_size_entry, "activate",
        G_CALLBACK (buffer_size_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->buffer_size_entry, 0, 3, 1, 1);

    label = gtk_label_new ("Buffer Duration");
    gtk_grid_attach (GTK_GRID (boxes3), label, 1, 2, 1, 1);
    app->buffer_duration_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (app->buffer_duration_entry), "-1");
    g_signal_connect (app->buffer_duration_entry, "activate",
        G_CALLBACK (buffer_duration_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->buffer_duration_entry, 1, 3, 1, 1);

    label = gtk_label_new ("Ringbuffer Max Size");
    gtk_grid_attach (GTK_GRID (boxes3), label, 2, 2, 1, 1);
    app->ringbuffer_maxsize_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (app->ringbuffer_maxsize_entry), "0");
    g_signal_connect (app->ringbuffer_maxsize_entry, "activate",
        G_CALLBACK (ringbuffer_maxsize_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->ringbuffer_maxsize_entry, 2, 3, 1,
        1);

    label = gtk_label_new ("Connection Speed");
    gtk_grid_attach (GTK_GRID (boxes3), label, 3, 2, 1, 1);
    app->connection_speed_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (app->connection_speed_entry), "0");
    g_signal_connect (app->connection_speed_entry, "activate",
        G_CALLBACK (connection_speed_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->connection_speed_entry, 3, 3, 1,
        1);

    label = gtk_label_new ("A/V offset");
    gtk_grid_attach (GTK_GRID (boxes3), label, 4, 2, 1, 1);
    app->av_offset_entry = gtk_entry_new ();
    g_signal_connect (app->av_offset_entry, "activate",
        G_CALLBACK (av_offset_activate_cb), app);
    gtk_entry_set_text (GTK_ENTRY (app->av_offset_entry), "0");
    g_signal_connect (app->av_offset_entry, "activate",
        G_CALLBACK (av_offset_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->av_offset_entry, 4, 3, 1, 1);

    label = gtk_label_new ("Subtitle offset");
    gtk_grid_attach (GTK_GRID (boxes3), label, 5, 2, 1, 1);
    app->text_offset_entry = gtk_entry_new ();
    g_signal_connect (app->text_offset_entry, "activate",
        G_CALLBACK (text_offset_activate_cb), app);
    gtk_entry_set_text (GTK_ENTRY (app->text_offset_entry), "0");
    g_signal_connect (app->text_offset_entry, "activate",
        G_CALLBACK (text_offset_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->text_offset_entry, 5, 3, 1, 1);

    label = gtk_label_new ("Subtitle Encoding");
    gtk_grid_attach (GTK_GRID (boxes3), label, 0, 4, 1, 1);
    app->subtitle_encoding_entry = gtk_entry_new ();
    g_signal_connect (app->subtitle_encoding_entry, "activate",
        G_CALLBACK (subtitle_encoding_activate_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->subtitle_encoding_entry, 0, 5, 1,
        1);

    label = gtk_label_new ("Subtitle Fontdesc");
    gtk_grid_attach (GTK_GRID (boxes3), label, 1, 4, 1, 1);
    app->subtitle_fontdesc_button = gtk_font_button_new ();
    g_signal_connect (app->subtitle_fontdesc_button, "font-set",
        G_CALLBACK (subtitle_fontdesc_cb), app);
    gtk_grid_attach (GTK_GRID (boxes3), app->subtitle_fontdesc_button, 1, 5, 1,
        1);

    pb2vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (pb2vbox), panel, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (pb2vbox), boxes, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (pb2vbox), boxes2, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (pb2vbox), boxes3, FALSE, FALSE, 2);
    gtk_container_add (GTK_CONTAINER (playbin), pb2vbox);
  } else {
    playbin = NULL;
  }
  if (app->pipeline_type == 0)
    gtk_window_set_title (GTK_WINDOW (app->window), app->current_path->data);
  /* do the packing stuff ... */
  gtk_window_set_default_size (GTK_WINDOW (app->window), 250, 96);
  /* FIXME: can we avoid this for audio only? */
  gtk_widget_set_size_request (GTK_WIDGET (app->video_window), -1,
      DEFAULT_VIDEO_HEIGHT);
  gtk_container_add (GTK_CONTAINER (app->window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), app->video_window, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), play_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), pause_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), stop_button, FALSE, FALSE, 2);

  gtk_box_pack_start (GTK_BOX (vbox), seek, FALSE, FALSE, 2);
  if (playbin)
    gtk_box_pack_start (GTK_BOX (vbox), playbin, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), step, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), navigation, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), colorbalance, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox),
      gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), app->seek_scale, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), app->statusbar, FALSE, FALSE, 2);

  /* connect things ... */
  g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (play_cb),
      app);
  g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_cb),
      app);
  g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb),
      app);

  g_signal_connect (G_OBJECT (app->window), "delete-event",
      G_CALLBACK (delete_event_cb), app);

  gtk_widget_set_can_default (play_button, TRUE);
  gtk_widget_grab_default (play_button);
}

static void
set_defaults (PlaybackApp * app)
{
  memset (app, 0, sizeof (PlaybackApp));

  app->flush_seek = TRUE;
  app->scrub = TRUE;
  app->rate = 1.0;

  app->position = app->duration = -1;
  app->state = GST_STATE_NULL;

  app->need_streams = TRUE;

  g_mutex_init (&app->state_mutex);

  app->play_rate = 1.0;
}

static void
reset_app (PlaybackApp * app)
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  g_list_free (app->formats);
  g_mutex_clear (&app->state_mutex);

  if (app->overlay_element)
    gst_object_unref (app->overlay_element);
  if (app->navigation_element)
    gst_object_unref (app->navigation_element);
  if (app->colorbalance_element)
    gst_object_unref (app->colorbalance_element);

  g_list_foreach (app->paths, (GFunc) g_free, NULL);
  g_list_free (app->paths);
  g_list_foreach (app->sub_paths, (GFunc) g_free, NULL);
  g_list_free (app->sub_paths);
  if (app->vis_entries)
    g_array_free (app->vis_entries, TRUE);
  g_print ("free pipeline\n");
  gst_object_unref (app->pipeline);
}

int
main (int argc, char **argv)
{
  PlaybackApp app;
  GOptionEntry options[] = {
    {"stats", 's', 0, G_OPTION_ARG_NONE, &app.stats,
        "Show pad stats", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &app.verbose,
        "Verbose properties", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;

  set_defaults (&app);

  ctx = g_option_context_new ("- playback testing in gsteamer");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }
  g_option_context_free (ctx);
  GST_DEBUG_CATEGORY_INIT (playback_debug, "playback-test", 0,
      "playback example");

  if (argc < 3) {
    print_usage (argc, argv);
    exit (-1);
  }

  app.pipeline_type = -1;
  if (g_ascii_isdigit (argv[1][0])) {
    app.pipeline_type = atoi (argv[1]);
  } else {
    gint i;

    for (i = 0; i < G_N_ELEMENTS (pipelines); ++i) {
      if (strcmp (pipelines[i].name, argv[1]) == 0) {
        app.pipeline_type = i;
        break;
      }
    }
  }

  if (app.pipeline_type < 0 || app.pipeline_type >= G_N_ELEMENTS (pipelines)) {
    print_usage (argc, argv);
    exit (-1);
  }

  app.pipeline_spec = argv[2];

  if (g_path_is_absolute (app.pipeline_spec) &&
      (g_strrstr (app.pipeline_spec, "*") != NULL ||
          g_strrstr (app.pipeline_spec, "?") != NULL)) {
    app.paths = handle_wildcards (app.pipeline_spec);
  } else {
    app.paths = g_list_prepend (app.paths, g_strdup (app.pipeline_spec));
  }

  if (!app.paths) {
    g_print ("opening %s failed\n", app.pipeline_spec);
    exit (-1);
  }

  app.current_path = app.paths;

  if (argc > 3 && argv[3]) {
    if (g_path_is_absolute (argv[3]) &&
        (g_strrstr (argv[3], "*") != NULL ||
            g_strrstr (argv[3], "?") != NULL)) {
      app.sub_paths = handle_wildcards (argv[3]);
    } else {
      app.sub_paths = g_list_prepend (app.sub_paths, g_strdup (argv[3]));
    }

    if (!app.sub_paths) {
      g_print ("opening %s failed\n", argv[3]);
      exit (-1);
    }

    app.current_sub_path = app.sub_paths;
  }

  pipelines[app.pipeline_type].func (&app, app.current_path->data);
  if (!app.pipeline || !GST_IS_PIPELINE (app.pipeline)) {
    g_print ("Pipeline failed on %s\n", argv[3]);
    exit (-1);
  }

  create_ui (&app);

  /* show the gui. */
  gtk_widget_show_all (app.window);

  /* realize window now so that the video window gets created and we can
   * obtain its XID before the pipeline is started up and the videosink
   * asks for the XID of the window to render onto */
  gtk_widget_realize (app.window);

#if defined (GDK_WINDOWING_X11) || defined (GDK_WINDOWING_WIN32) || defined (GDK_WINDOWING_QUARTZ)
  /* we should have the XID now */
  g_assert (app.embed_xid != 0);

  if (app.pipeline_type == 0) {
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (app.pipeline),
        app.embed_xid);
  }
#endif

  if (app.verbose) {
    g_signal_connect (app.pipeline, "deep_notify",
        G_CALLBACK (gst_object_default_deep_notify), NULL);
  }

  connect_bus_signals (&app);

  gtk_main ();

  g_print ("NULL pipeline\n");
  gst_element_set_state (app.pipeline, GST_STATE_NULL);

  reset_app (&app);
  gst_deinit ();

  return 0;
}
