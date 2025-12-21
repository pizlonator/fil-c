/* GStreamer
 *
 * seek.c: seeking sample application
 *
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 *               2006 Stefan Kost <ensonic@users.sf.net>
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

#include <glib.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <string.h>

#include <linux/input.h>
#include <linux/joystick.h>

#ifdef HAVE_X11
#include <gdk/gdkx.h>
#endif
#include <gst/video/videooverlay.h>

GST_DEBUG_CATEGORY_STATIC (seek_debug);
#define GST_CAT_DEFAULT (seek_debug)

/* configuration */
#define SOURCE "filesrc"

#define ASINK "alsasink"
//#define ASINK "osssink"

#define VSINK "xvimagesink"
//#define VSINK "sdlvideosink"
//#define VSINK "ximagesink"
//#define VSINK "aasink"
//#define VSINK "cacasink"

#define FILL_INTERVAL 100
//#define UPDATE_INTERVAL 500
//#define UPDATE_INTERVAL 100
//#define UPDATE_INTERVAL 10
#define UPDATE_INTERVAL 40

/* number of milliseconds to play for after a seek */
#define SCRUB_TIME 100

/* timeout for gst_element_get_state() after a seek */
#define SEEK_TIMEOUT 40 * GST_MSECOND

#define DEFAULT_VIDEO_HEIGHT 300

/* the state to go to when stop is pressed */
#define STOP_STATE      GST_STATE_READY


static GList *seekable_pads = NULL;
static GList *rate_pads = NULL;
static GList *seekable_elements = NULL;

static gboolean accurate_seek = FALSE;
static gboolean keyframe_seek = FALSE;
static gboolean loop_seek = FALSE;
static gboolean flush_seek = TRUE;
static gboolean scrub = TRUE;
static gboolean play_scrub = FALSE;
static gboolean skip_seek = FALSE;
static gdouble rate = 1.0;

static GstElement *pipeline;
static gint pipeline_type;
static const gchar *pipeline_spec;
static gint64 position = -1;
static gint64 duration = -1;
static GtkAdjustment *adjustment;
static GtkWidget *hscale, *statusbar;
static guint status_id = 0;
static gboolean stats = FALSE;
static gboolean elem_seek = FALSE;
static gboolean verbose = FALSE;
static gchar *js_device = NULL;

static gboolean is_live = FALSE;
static gboolean buffering = FALSE;
static GstBufferingMode mode;
static gint64 buffering_left;
static GstState state = GST_STATE_NULL;
static guint update_id = 0;
static guint seek_timeout_id = 0;
static gulong changed_id;
static guint fill_id = 0;

static gint n_video = 0, n_audio = 0, n_text = 0;
static gboolean need_streams = TRUE;
static GtkWidget *video_combo, *audio_combo, *text_combo, *vis_combo;
static GtkWidget *vis_checkbox, *video_checkbox, *audio_checkbox;
static GtkWidget *text_checkbox, *mute_checkbox, *volume_spinbutton;
static GtkWidget *skip_checkbox, *video_window, *download_checkbox;
static GtkWidget *buffer_checkbox, *rate_spinbutton;

static GMutex state_mutex;

static GtkWidget *format_combo, *step_amount_spinbutton, *step_rate_spinbutton;
static GtkWidget *shuttle_checkbox, *step_button;
static GtkWidget *shuttle_hscale;
static GtkAdjustment *shuttle_adjustment;

static GList *paths = NULL, *l = NULL;

gint js_fd;

/* we keep an array of the visualisation entries so that we can easily switch
 * with the combo box index. */
typedef struct
{
  GstElementFactory *factory;
} VisEntry;

static GArray *vis_entries;

static void clear_streams (GstElement * pipeline);
static void volume_notify_cb (GstElement * pipeline, GParamSpec * arg,
    gpointer user_dat);

/* pipeline construction */

typedef struct
{
  const gchar *padname;
  GstPad *target;
  GstElement *bin;
}
dyn_link;

static GstElement *
gst_element_factory_make_or_warn (const gchar * type, const gchar * name)
{
  GstElement *element = gst_element_factory_make (type, name);

  if (!element) {
    g_warning ("Failed to create element %s of type %s", name, type);
  }

  return element;
}

static void
dynamic_link (GstPadTemplate * templ, GstPad * newpad, gpointer data)
{
  gchar *padname;
  dyn_link *connect = (dyn_link *) data;

  padname = gst_pad_get_name (newpad);

  if (connect->padname == NULL || !strcmp (padname, connect->padname)) {
    if (connect->bin)
      gst_bin_add (GST_BIN (pipeline), connect->bin);
    gst_pad_link (newpad, connect->target);

    //seekable_pads = g_list_prepend (seekable_pads, newpad);
    rate_pads = g_list_prepend (rate_pads, newpad);
  }
  g_free (padname);
}

static void
setup_dynamic_link (GstElement * element, const gchar * padname,
    GstPad * target, GstElement * bin)
{
  dyn_link *connect;

  connect = g_new0 (dyn_link, 1);
  connect->padname = g_strdup (padname);
  connect->target = target;
  connect->bin = bin;

  g_signal_connect (G_OBJECT (element), "pad-added", G_CALLBACK (dynamic_link),
      connect);
}

static GstElement *
make_mod_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *decoder, *audiosink;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  decoder = gst_element_factory_make_or_warn ("modplug", "decoder");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");
  //g_object_set (G_OBJECT (audiosink), "sync", FALSE, NULL);

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), audiosink);

  gst_element_link (src, decoder);
  gst_element_link (decoder, audiosink);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_dv_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *demux, *decoder, *audiosink, *videosink;
  GstElement *a_queue, *v_queue;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  demux = gst_element_factory_make_or_warn ("dvdemux", "demuxer");
  v_queue = gst_element_factory_make_or_warn ("queue", "v_queue");
  decoder = gst_element_factory_make_or_warn ("ffdec_dvvideo", "decoder");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  audiosink = gst_element_factory_make_or_warn ("alsasink", "a_sink");

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_bin_add (GST_BIN (pipeline), a_queue);
  gst_bin_add (GST_BIN (pipeline), audiosink);
  gst_bin_add (GST_BIN (pipeline), v_queue);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), videosink);

  gst_element_link (src, demux);
  gst_element_link (a_queue, audiosink);
  gst_element_link (v_queue, decoder);
  gst_element_link (decoder, videosink);

  setup_dynamic_link (demux, "video", gst_element_get_static_pad (v_queue,
          "sink"), NULL);
  setup_dynamic_link (demux, "audio", gst_element_get_static_pad (a_queue,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);

  return pipeline;
}

static GstElement *
make_wav_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *decoder, *audiosink;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  decoder = gst_element_factory_make_or_warn ("wavparse", "decoder");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), audiosink);

  gst_element_link (src, decoder);

  setup_dynamic_link (decoder, "src", gst_element_get_static_pad (audiosink,
          "sink"), NULL);

  seekable_elements = g_list_prepend (seekable_elements, audiosink);

  /* force element seeking on this pipeline */
  elem_seek = TRUE;

  return pipeline;
}

static GstElement *
make_flac_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *decoder, *audiosink;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  decoder = gst_element_factory_make_or_warn ("flacdec", "decoder");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");
  g_object_set (G_OBJECT (audiosink), "sync", FALSE, NULL);

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), audiosink);

  gst_element_link (src, decoder);
  gst_element_link (decoder, audiosink);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_sid_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *decoder, *audiosink;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  decoder = gst_element_factory_make_or_warn ("siddec", "decoder");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");
  //g_object_set (G_OBJECT (audiosink), "sync", FALSE, NULL);

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), audiosink);

  gst_element_link (src, decoder);
  gst_element_link (decoder, audiosink);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_parse_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *parser, *fakesink;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  parser = gst_element_factory_make_or_warn ("mpegparse", "parse");
  fakesink = gst_element_factory_make_or_warn ("fakesink", "sink");
  g_object_set (G_OBJECT (fakesink), "silent", TRUE, NULL);
  g_object_set (G_OBJECT (fakesink), "sync", TRUE, NULL);

  g_object_set (G_OBJECT (src), "location", location, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), parser);
  gst_bin_add (GST_BIN (pipeline), fakesink);

  gst_element_link (src, parser);
  gst_element_link (parser, fakesink);

  seekable = gst_element_get_static_pad (parser, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (parser, "sink"));

  return pipeline;
}

static GstElement *
make_vorbis_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin;
  GstElement *src, *demux, *decoder, *convert, *audiosink;
  GstPad *pad, *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  demux = gst_element_factory_make_or_warn ("oggdemux", "demux");
  decoder = gst_element_factory_make_or_warn ("vorbisdec", "decoder");
  convert = gst_element_factory_make_or_warn ("audioconvert", "convert");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");
  g_object_set (G_OBJECT (audiosink), "sync", TRUE, NULL);

  g_object_set (G_OBJECT (src), "location", location, NULL);

  audio_bin = gst_bin_new ("a_decoder_bin");

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_bin_add (GST_BIN (audio_bin), decoder);
  gst_bin_add (GST_BIN (audio_bin), convert);
  gst_bin_add (GST_BIN (audio_bin), audiosink);
  gst_bin_add (GST_BIN (pipeline), audio_bin);

  gst_element_link (src, demux);
  gst_element_link (decoder, convert);
  gst_element_link (convert, audiosink);

  pad = gst_element_get_static_pad (decoder, "sink");
  gst_element_add_pad (audio_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (audio_bin,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_theora_pipeline (const gchar * location)
{
  GstElement *pipeline, *video_bin;
  GstElement *src, *demux, *decoder, *convert, *videosink;
  GstPad *pad, *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  demux = gst_element_factory_make_or_warn ("oggdemux", "demux");
  decoder = gst_element_factory_make_or_warn ("theoradec", "decoder");
  convert = gst_element_factory_make_or_warn ("videoconvert", "convert");
  videosink = gst_element_factory_make_or_warn (VSINK, "sink");

  g_object_set (G_OBJECT (src), "location", location, NULL);

  video_bin = gst_bin_new ("v_decoder_bin");

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_bin_add (GST_BIN (video_bin), decoder);
  gst_bin_add (GST_BIN (video_bin), convert);
  gst_bin_add (GST_BIN (video_bin), videosink);
  gst_bin_add (GST_BIN (pipeline), video_bin);

  gst_element_link (src, demux);
  gst_element_link (decoder, convert);
  gst_element_link (convert, videosink);

  pad = gst_element_get_static_pad (decoder, "sink");
  gst_element_add_pad (video_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (video_bin,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_vorbis_theora_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin, *video_bin;
  GstElement *src, *demux, *a_decoder, *a_convert, *v_decoder, *v_convert;
  GstElement *audiosink, *videosink;
  GstElement *a_queue, *v_queue, *v_scale;
  GstPad *seekable;
  GstPad *pad;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  g_object_set (G_OBJECT (src), "location", location, NULL);

  demux = gst_element_factory_make_or_warn ("oggdemux", "demux");

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_element_link (src, demux);

  audio_bin = gst_bin_new ("a_decoder_bin");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  a_decoder = gst_element_factory_make_or_warn ("vorbisdec", "a_dec");
  a_convert = gst_element_factory_make_or_warn ("audioconvert", "a_convert");
  audiosink = gst_element_factory_make_or_warn (ASINK, "a_sink");

  gst_bin_add (GST_BIN (pipeline), audio_bin);

  gst_bin_add (GST_BIN (audio_bin), a_queue);
  gst_bin_add (GST_BIN (audio_bin), a_decoder);
  gst_bin_add (GST_BIN (audio_bin), a_convert);
  gst_bin_add (GST_BIN (audio_bin), audiosink);

  gst_element_link (a_queue, a_decoder);
  gst_element_link (a_decoder, a_convert);
  gst_element_link (a_convert, audiosink);

  pad = gst_element_get_static_pad (a_queue, "sink");
  gst_element_add_pad (audio_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (audio_bin,
          "sink"), NULL);

  video_bin = gst_bin_new ("v_decoder_bin");
  v_queue = gst_element_factory_make_or_warn ("queue", "v_queue");
  v_decoder = gst_element_factory_make_or_warn ("theoradec", "v_dec");
  v_convert = gst_element_factory_make_or_warn ("videoconvert", "v_convert");
  v_scale = gst_element_factory_make_or_warn ("videoscale", "v_scale");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");

  gst_bin_add (GST_BIN (pipeline), video_bin);

  gst_bin_add (GST_BIN (video_bin), v_queue);
  gst_bin_add (GST_BIN (video_bin), v_decoder);
  gst_bin_add (GST_BIN (video_bin), v_convert);
  gst_bin_add (GST_BIN (video_bin), v_scale);
  gst_bin_add (GST_BIN (video_bin), videosink);

  gst_element_link_many (v_queue, v_decoder, v_convert, v_scale, videosink,
      NULL);

  pad = gst_element_get_static_pad (v_queue, "sink");
  gst_element_add_pad (video_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (video_bin,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (a_decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (a_decoder,
          "sink"));

  return pipeline;
}

static GstElement *
make_avi_msmpeg4v3_mp3_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin, *video_bin;
  GstElement *src, *demux, *a_decoder, *a_convert, *v_decoder, *v_convert;
  GstElement *audiosink, *videosink;
  GstElement *a_queue, *v_queue;
  GstPad *seekable, *pad;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  g_object_set (G_OBJECT (src), "location", location, NULL);

  demux = gst_element_factory_make_or_warn ("avidemux", "demux");

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_element_link (src, demux);

  audio_bin = gst_bin_new ("a_decoder_bin");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  a_decoder = gst_element_factory_make_or_warn ("mpg123audiodec", "a_dec");
  a_convert = gst_element_factory_make_or_warn ("audioconvert", "a_convert");
  audiosink = gst_element_factory_make_or_warn (ASINK, "a_sink");

  gst_bin_add (GST_BIN (audio_bin), a_queue);
  gst_bin_add (GST_BIN (audio_bin), a_decoder);
  gst_bin_add (GST_BIN (audio_bin), a_convert);
  gst_bin_add (GST_BIN (audio_bin), audiosink);

  gst_element_link (a_queue, a_decoder);
  gst_element_link (a_decoder, a_convert);
  gst_element_link (a_convert, audiosink);

  gst_bin_add (GST_BIN (pipeline), audio_bin);

  pad = gst_element_get_static_pad (a_queue, "sink");
  gst_element_add_pad (audio_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (audio_bin,
          "sink"), NULL);

  video_bin = gst_bin_new ("v_decoder_bin");
  v_queue = gst_element_factory_make_or_warn ("queue", "v_queue");
  v_decoder = gst_element_factory_make_or_warn ("ffdec_msmpeg4", "v_dec");
  v_convert = gst_element_factory_make_or_warn ("videoconvert", "v_convert");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");

  gst_bin_add (GST_BIN (video_bin), v_queue);
  gst_bin_add (GST_BIN (video_bin), v_decoder);
  gst_bin_add (GST_BIN (video_bin), v_convert);
  gst_bin_add (GST_BIN (video_bin), videosink);

  gst_element_link_many (v_queue, v_decoder, v_convert, videosink, NULL);

  gst_bin_add (GST_BIN (pipeline), video_bin);

  pad = gst_element_get_static_pad (v_queue, "sink");
  gst_element_add_pad (video_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, NULL, gst_element_get_static_pad (video_bin,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (a_decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (a_decoder,
          "sink"));

  return pipeline;
}

static GstElement *
make_mp3_pipeline (const gchar * location)
{
  GstElement *pipeline;
  GstElement *src, *parser, *decoder, *audiosink, *queue;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  parser = gst_element_factory_make_or_warn ("mpegaudioparse", "parse");
  decoder = gst_element_factory_make_or_warn ("mpg123audiodec", "dec");
  queue = gst_element_factory_make_or_warn ("queue", "queue");
  audiosink = gst_element_factory_make_or_warn (ASINK, "sink");

  seekable_elements = g_list_prepend (seekable_elements, audiosink);

  g_object_set (G_OBJECT (src), "location", location, NULL);
  //g_object_set (G_OBJECT (audiosink), "fragment", 0x00180008, NULL);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), parser);
  gst_bin_add (GST_BIN (pipeline), decoder);
  gst_bin_add (GST_BIN (pipeline), queue);
  gst_bin_add (GST_BIN (pipeline), audiosink);

  gst_element_link (src, parser);
  gst_element_link (parser, decoder);
  gst_element_link (decoder, queue);
  gst_element_link (queue, audiosink);

  seekable = gst_element_get_static_pad (queue, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (decoder, "sink"));

  return pipeline;
}

static GstElement *
make_avi_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin, *video_bin;
  GstElement *src, *demux, *a_decoder, *v_decoder, *audiosink, *videosink;
  GstElement *a_queue = NULL, *v_queue = NULL;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  g_object_set (G_OBJECT (src), "location", location, NULL);

  demux = gst_element_factory_make_or_warn ("avidemux", "demux");
  seekable_elements = g_list_prepend (seekable_elements, demux);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_element_link (src, demux);

  audio_bin = gst_bin_new ("a_decoder_bin");
  a_decoder = gst_element_factory_make_or_warn ("mpg123audiodec", "a_dec");
  audiosink = gst_element_factory_make_or_warn (ASINK, "a_sink");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  gst_element_link (a_decoder, a_queue);
  gst_element_link (a_queue, audiosink);
  gst_bin_add (GST_BIN (audio_bin), a_decoder);
  gst_bin_add (GST_BIN (audio_bin), a_queue);
  gst_bin_add (GST_BIN (audio_bin), audiosink);
  gst_element_set_state (audio_bin, GST_STATE_PAUSED);

  setup_dynamic_link (demux, "audio_00", gst_element_get_static_pad (a_decoder,
          "sink"), audio_bin);

  seekable = gst_element_get_static_pad (a_queue, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (a_decoder,
          "sink"));

  video_bin = gst_bin_new ("v_decoder_bin");
  v_decoder = gst_element_factory_make_or_warn ("ffmpegdecall", "v_dec");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");
  v_queue = gst_element_factory_make_or_warn ("queue", "v_queue");
  gst_element_link (v_decoder, v_queue);
  gst_element_link (v_queue, videosink);
  gst_bin_add (GST_BIN (video_bin), v_decoder);
  gst_bin_add (GST_BIN (video_bin), v_queue);
  gst_bin_add (GST_BIN (video_bin), videosink);

  gst_element_set_state (video_bin, GST_STATE_PAUSED);

  setup_dynamic_link (demux, "video_00", gst_element_get_static_pad (v_decoder,
          "sink"), video_bin);

  seekable = gst_element_get_static_pad (v_queue, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (v_decoder,
          "sink"));

  return pipeline;
}

static GstElement *
make_mpeg_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin, *video_bin;
  GstElement *src, *demux, *a_decoder, *v_decoder, *v_filter;
  GstElement *audiosink, *videosink;
  GstElement *a_queue, *v_queue;
  GstPad *seekable;
  GstPad *pad;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  g_object_set (G_OBJECT (src), "location", location, NULL);

  demux = gst_element_factory_make_or_warn ("mpegdemux", "demux");

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_element_link (src, demux);

  audio_bin = gst_bin_new ("a_decoder_bin");
  a_decoder = gst_element_factory_make_or_warn ("mpg123audiodec", "a_dec");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  audiosink = gst_element_factory_make_or_warn (ASINK, "a_sink");
  gst_bin_add (GST_BIN (audio_bin), a_decoder);
  gst_bin_add (GST_BIN (audio_bin), a_queue);
  gst_bin_add (GST_BIN (audio_bin), audiosink);

  gst_element_link (a_decoder, a_queue);
  gst_element_link (a_queue, audiosink);

  gst_bin_add (GST_BIN (pipeline), audio_bin);

  pad = gst_element_get_static_pad (a_decoder, "sink");
  gst_element_add_pad (audio_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, "audio_c0", gst_element_get_static_pad (audio_bin,
          "sink"), NULL);

  video_bin = gst_bin_new ("v_decoder_bin");
  v_decoder = gst_element_factory_make_or_warn ("mpeg2dec", "v_dec");
  v_queue = gst_element_factory_make_or_warn ("queue", "v_queue");
  v_filter = gst_element_factory_make_or_warn ("videoconvert", "v_filter");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");

  gst_bin_add (GST_BIN (video_bin), v_decoder);
  gst_bin_add (GST_BIN (video_bin), v_queue);
  gst_bin_add (GST_BIN (video_bin), v_filter);
  gst_bin_add (GST_BIN (video_bin), videosink);

  gst_element_link (v_decoder, v_queue);
  gst_element_link (v_queue, v_filter);
  gst_element_link (v_filter, videosink);

  gst_bin_add (GST_BIN (pipeline), video_bin);

  pad = gst_element_get_static_pad (v_decoder, "sink");
  gst_element_add_pad (video_bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (pad);

  setup_dynamic_link (demux, "video_e0", gst_element_get_static_pad (video_bin,
          "sink"), NULL);

  seekable = gst_element_get_static_pad (v_filter, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (v_decoder,
          "sink"));

  return pipeline;
}

static GstElement *
make_mpegnt_pipeline (const gchar * location)
{
  GstElement *pipeline, *audio_bin, *video_bin;
  GstElement *src, *demux, *a_decoder, *v_decoder, *v_filter;
  GstElement *audiosink, *videosink;
  GstElement *a_queue;
  GstPad *seekable;

  pipeline = gst_pipeline_new ("app");

  src = gst_element_factory_make_or_warn (SOURCE, "src");
  g_object_set (G_OBJECT (src), "location", location, NULL);

  demux = gst_element_factory_make_or_warn ("mpegdemux", "demux");
  //g_object_set (G_OBJECT (demux), "sync", TRUE, NULL);

  seekable_elements = g_list_prepend (seekable_elements, demux);

  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), demux);
  gst_element_link (src, demux);

  audio_bin = gst_bin_new ("a_decoder_bin");
  a_decoder = gst_element_factory_make_or_warn ("mpg123audiodec", "a_dec");
  a_queue = gst_element_factory_make_or_warn ("queue", "a_queue");
  audiosink = gst_element_factory_make_or_warn (ASINK, "a_sink");
  //g_object_set (G_OBJECT (audiosink), "fragment", 0x00180008, NULL);
  g_object_set (G_OBJECT (audiosink), "sync", FALSE, NULL);
  gst_element_link (a_decoder, a_queue);
  gst_element_link (a_queue, audiosink);
  gst_bin_add (GST_BIN (audio_bin), a_decoder);
  gst_bin_add (GST_BIN (audio_bin), a_queue);
  gst_bin_add (GST_BIN (audio_bin), audiosink);

  setup_dynamic_link (demux, "audio_00", gst_element_get_static_pad (a_decoder,
          "sink"), audio_bin);

  seekable = gst_element_get_static_pad (a_queue, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (a_decoder,
          "sink"));

  video_bin = gst_bin_new ("v_decoder_bin");
  v_decoder = gst_element_factory_make_or_warn ("mpeg2dec", "v_dec");
  v_filter = gst_element_factory_make_or_warn ("videoconvert", "v_filter");
  videosink = gst_element_factory_make_or_warn (VSINK, "v_sink");
  gst_element_link_many (v_decoder, v_filter, videosink, NULL);

  gst_bin_add_many (GST_BIN (video_bin), v_decoder, v_filter, videosink, NULL);

  setup_dynamic_link (demux, "video_00", gst_element_get_static_pad (v_decoder,
          "sink"), video_bin);

  seekable = gst_element_get_static_pad (v_decoder, "src");
  seekable_pads = g_list_prepend (seekable_pads, seekable);
  rate_pads = g_list_prepend (rate_pads, seekable);
  rate_pads =
      g_list_prepend (rate_pads, gst_element_get_static_pad (v_decoder,
          "sink"));

  return pipeline;
}

static void
playerbin_set_uri (GstElement * player, const gchar * location)
{
  gchar *uri;

  /* Add "file://" prefix for convenience */
  if (g_str_has_prefix (location, "/")) {
    uri = g_strconcat ("file://", location, NULL);
    g_object_set (G_OBJECT (player), "uri", uri, NULL);
    g_free (uri);
  } else {
    g_object_set (G_OBJECT (player), "uri", location, NULL);
  }
}

static GstElement *
construct_playerbin (const gchar * name, const gchar * location)
{
  GstElement *player;

  player = gst_element_factory_make (name, "player");
  g_assert (player);

  playerbin_set_uri (player, location);

  seekable_elements = g_list_prepend (seekable_elements, player);

  /* force element seeking on this pipeline */
  elem_seek = TRUE;

  return player;
}

static GstElement *
make_playerbin_pipeline (const gchar * location)
{
  return construct_playerbin ("playbin", location);
}

static GstElement *
make_playerbin2_pipeline (const gchar * location)
{
  GstElement *pipeline = construct_playerbin ("playbin", location);

  /* FIXME: this is not triggered, playbin is not forwarding it from the sink */
  g_signal_connect (pipeline, "notify::volume", G_CALLBACK (volume_notify_cb),
      NULL);
  return pipeline;
}

#ifndef GST_DISABLE_PARSE
static GstElement *
make_parselaunch_pipeline (const gchar * description)
{
  GstElement *pipeline;

  pipeline = gst_parse_launch (description, NULL);

  seekable_elements = g_list_prepend (seekable_elements, pipeline);

  elem_seek = TRUE;

  return pipeline;
}
#endif

typedef struct
{
  const gchar *name;
  GstElement *(*func) (const gchar * location);
}
Pipeline;

static Pipeline pipelines[] = {
  {"mp3", make_mp3_pipeline},
  {"avi", make_avi_pipeline},
  {"mpeg1", make_mpeg_pipeline},
  {"mpegparse", make_parse_pipeline},
  {"vorbis", make_vorbis_pipeline},
  {"theora", make_theora_pipeline},
  {"ogg/v/t", make_vorbis_theora_pipeline},
  {"avi/msmpeg4v3/mp3", make_avi_msmpeg4v3_mp3_pipeline},
  {"sid", make_sid_pipeline},
  {"flac", make_flac_pipeline},
  {"wav", make_wav_pipeline},
  {"mod", make_mod_pipeline},
  {"dv", make_dv_pipeline},
  {"mpeg1nothreads", make_mpegnt_pipeline},
  {"playerbin", make_playerbin_pipeline},
#ifndef GST_DISABLE_PARSE
  {"parse-launch", make_parselaunch_pipeline},
#endif
  {"playerbin2", make_playerbin2_pipeline},
  {NULL, NULL},
};

#define NUM_TYPES       ((sizeof (pipelines) / sizeof (Pipeline)) - 1)

/* ui callbacks and helpers */

static gchar *
format_value (GtkScale * scale, gdouble value)
{
  gint64 real;
  gint64 seconds;
  gint64 subseconds;

  real = value * duration / 100;
  seconds = (gint64) real / GST_SECOND;
  subseconds = (gint64) real / (GST_SECOND / 100);

  return g_strdup_printf ("%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT ":%02"
      G_GINT64_FORMAT, seconds / 60, seconds % 60, subseconds % 100);
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

G_GNUC_UNUSED static void
query_rates (void)
{
  GList *walk = rate_pads;

  while (walk) {
    GstPad *pad = GST_PAD (walk->data);
    gint i = 0;

    g_print ("rate/sec  %8.8s: ", GST_PAD_NAME (pad));
    while (seek_formats[i].name) {
      gint64 value;
      GstFormat format;

      format = seek_formats[i].format;

      if (gst_pad_query_convert (pad, GST_FORMAT_TIME, GST_SECOND, format,
              &value)) {
        g_print ("%s %13" G_GINT64_FORMAT " | ", seek_formats[i].name, value);
      } else {
        g_print ("%s %13.13s | ", seek_formats[i].name, "*NA*");
      }

      i++;
    }
    g_print (" %s:%s\n", GST_DEBUG_PAD_NAME (pad));

    walk = g_list_next (walk);
  }
}

G_GNUC_UNUSED static void
query_positions_elems (void)
{
  GList *walk = seekable_elements;

  while (walk) {
    GstElement *element = GST_ELEMENT (walk->data);
    gint i = 0;

    g_print ("positions %8.8s: ", GST_ELEMENT_NAME (element));
    while (seek_formats[i].name) {
      gint64 position, total;
      GstFormat format;

      format = seek_formats[i].format;

      if (gst_element_query_position (element, format, &position) &&
          gst_element_query_duration (element, format, &total)) {
        g_print ("%s %13" G_GINT64_FORMAT " / %13" G_GINT64_FORMAT " | ",
            seek_formats[i].name, position, total);
      } else {
        g_print ("%s %13.13s / %13.13s | ", seek_formats[i].name, "*NA*",
            "*NA*");
      }
      i++;
    }
    g_print (" %s\n", GST_ELEMENT_NAME (element));

    walk = g_list_next (walk);
  }
}

G_GNUC_UNUSED static void
query_positions_pads (void)
{
  GList *walk = seekable_pads;

  while (walk) {
    GstPad *pad = GST_PAD (walk->data);
    gint i = 0;

    g_print ("positions %8.8s: ", GST_PAD_NAME (pad));
    while (seek_formats[i].name) {
      GstFormat format;
      gint64 position, total;

      format = seek_formats[i].format;

      if (gst_pad_query_position (pad, format, &position) &&
          gst_pad_query_duration (pad, format, &total)) {
        g_print ("%s %13" G_GINT64_FORMAT " / %13" G_GINT64_FORMAT " | ",
            seek_formats[i].name, position, total);
      } else {
        g_print ("%s %13.13s / %13.13s | ", seek_formats[i].name, "*NA*",
            "*NA*");
      }

      i++;
    }
    g_print (" %s:%s\n", GST_DEBUG_PAD_NAME (pad));

    walk = g_list_next (walk);
  }
}

static gboolean start_seek (GtkWidget * widget, GdkEventButton * event,
    gpointer user_data);
static gboolean stop_seek (GtkWidget * widget, GdkEventButton * event,
    gpointer user_data);
static void seek_cb (GtkWidget * widget);

static void
set_scale (gdouble value)
{
  g_signal_handlers_block_by_func (hscale, (void *) start_seek,
      (void *) pipeline);
  g_signal_handlers_block_by_func (hscale, (void *) stop_seek,
      (void *) pipeline);
  g_signal_handlers_block_by_func (hscale, (void *) seek_cb, (void *) pipeline);
  gtk_adjustment_set_value (adjustment, value);
  g_signal_handlers_unblock_by_func (hscale, (void *) start_seek,
      (void *) pipeline);
  g_signal_handlers_unblock_by_func (hscale, (void *) stop_seek,
      (void *) pipeline);
  g_signal_handlers_unblock_by_func (hscale, (void *) seek_cb,
      (void *) pipeline);
  gtk_widget_queue_draw (hscale);
}

static gboolean
update_fill (gpointer data)
{
  if (elem_seek) {
    if (seekable_elements) {
      GstElement *element = GST_ELEMENT (seekable_elements->data);
      GstQuery *query;

      query = gst_query_new_buffering (GST_FORMAT_PERCENT);
      if (gst_element_query (element, query)) {
        gint64 start, stop, buffering_total;
        GstFormat format;
        gdouble fill;
        gboolean busy;
        gint percent;
        GstBufferingMode mode;
        gint avg_in, avg_out;
        gint64 buffering_left;

        gst_query_parse_buffering_percent (query, &busy, &percent);
        gst_query_parse_buffering_range (query, &format, &start, &stop,
            &buffering_total);
        gst_query_parse_buffering_stats (query, &mode, &avg_in, &avg_out,
            &buffering_left);

        /* note that we could start the playback when buffering_left < remaining
         * playback time */
        GST_DEBUG ("buffering total %" G_GINT64_FORMAT " ms, left %"
            G_GINT64_FORMAT " ms", buffering_total, buffering_left);
        GST_DEBUG ("start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT,
            start, stop);

        if (stop != -1)
          fill = 100.0 * stop / GST_FORMAT_PERCENT_MAX;
        else
          fill = 100.0;

        gtk_range_set_fill_level (GTK_RANGE (hscale), fill);
      }
      gst_query_unref (query);
    }
  }
  return TRUE;
}

static gboolean
update_scale (gpointer data)
{
  if (elem_seek) {
    if (seekable_elements) {
      GstElement *element = GST_ELEMENT (seekable_elements->data);

      gst_element_query_position (element, GST_FORMAT_TIME, &position);
      gst_element_query_duration (element, GST_FORMAT_TIME, &duration);
    }
  } else {
    if (seekable_pads) {
      GstPad *pad = GST_PAD (seekable_pads->data);

      gst_pad_query_position (pad, GST_FORMAT_TIME, &position);
      gst_pad_query_duration (pad, GST_FORMAT_TIME, &duration);
    }
  }

  if (stats) {
    if (elem_seek) {
      query_positions_elems ();
    } else {
      query_positions_pads ();
    }
    query_rates ();
  }

  if (position >= duration)
    duration = position;

  if (duration > 0) {
    set_scale (position * 100.0 / duration);
  }

  /* FIXME: see make_playerbin2_pipeline() and volume_notify_cb() */
  if (pipeline_type == 16) {
    g_object_notify (G_OBJECT (pipeline), "volume");
  }

  return TRUE;
}

static void do_seek (GtkWidget * widget);
static void connect_bus_signals (GstElement * pipeline);
static void set_update_scale (gboolean active);
static void set_update_fill (gboolean active);

static gboolean
end_scrub (GtkWidget * widget)
{
  GST_DEBUG ("end scrub, PAUSE");
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  seek_timeout_id = 0;

  return FALSE;
}

static gboolean
send_event (GstEvent * event)
{
  gboolean res = FALSE;

  if (!elem_seek) {
    GList *walk = seekable_pads;

    while (walk) {
      GstPad *seekable = GST_PAD (walk->data);

      GST_DEBUG ("send event on pad %s:%s", GST_DEBUG_PAD_NAME (seekable));

      gst_event_ref (event);
      res = gst_pad_send_event (seekable, event);

      walk = g_list_next (walk);
    }
  } else {
    GList *walk = seekable_elements;

    while (walk) {
      GstElement *seekable = GST_ELEMENT (walk->data);

      GST_DEBUG ("send event on element %s", GST_ELEMENT_NAME (seekable));

      gst_event_ref (event);
      res = gst_element_send_event (seekable, event);

      walk = g_list_next (walk);
    }
  }
  gst_event_unref (event);
  return res;
}

static void
do_seek (GtkWidget * widget)
{
  gint64 real;
  gboolean res = FALSE;
  GstEvent *s_event;
  GstSeekFlags flags;

  real = gtk_range_get_value (GTK_RANGE (widget)) * duration / 100;

  flags = 0;
  if (flush_seek)
    flags |= GST_SEEK_FLAG_FLUSH;
  if (accurate_seek)
    flags |= GST_SEEK_FLAG_ACCURATE;
  if (keyframe_seek)
    flags |= GST_SEEK_FLAG_KEY_UNIT;
  if (loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (skip_seek)
    flags |= GST_SEEK_FLAG_SKIP;

  if (rate >= 0) {
    s_event = gst_event_new_seek (rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, real, GST_SEEK_TYPE_SET,
        GST_CLOCK_TIME_NONE);
    GST_DEBUG ("seek with rate %lf to %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT,
        rate, GST_TIME_ARGS (real), GST_TIME_ARGS (duration));
  } else {
    s_event = gst_event_new_seek (rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        GST_SEEK_TYPE_SET, real);
    GST_DEBUG ("seek with rate %lf to %" GST_TIME_FORMAT " / %" GST_TIME_FORMAT,
        rate, GST_TIME_ARGS (0), GST_TIME_ARGS (real));
  }

  res = send_event (s_event);

  if (res) {
    if (flush_seek) {
      gst_element_get_state (GST_ELEMENT (pipeline), NULL, NULL, SEEK_TIMEOUT);
    } else {
      set_update_scale (TRUE);
    }
  } else {
    g_print ("seek failed\n");
    set_update_scale (TRUE);
  }
}

static void
seek_cb (GtkWidget * widget)
{
  /* If the timer hasn't expired yet, then the pipeline is running */
  if (play_scrub && seek_timeout_id != 0) {
    GST_DEBUG ("do scrub seek, PAUSED");
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
  }

  GST_DEBUG ("do seek");
  do_seek (widget);

  if (play_scrub) {
    GST_DEBUG ("do scrub seek, PLAYING");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    if (seek_timeout_id == 0) {
      seek_timeout_id =
          g_timeout_add (SCRUB_TIME, (GSourceFunc) end_scrub, widget);
    }
  }
}

static void
set_update_fill (gboolean active)
{
  GST_DEBUG ("fill scale is %d", active);

  if (active) {
    if (fill_id == 0) {
      fill_id =
          g_timeout_add (FILL_INTERVAL, (GSourceFunc) update_fill, pipeline);
    }
  } else {
    if (fill_id) {
      g_source_remove (fill_id);
      fill_id = 0;
    }
  }
}

static void
set_update_scale (gboolean active)
{

  GST_DEBUG ("update scale is %d", active);

  if (active) {
    if (update_id == 0) {
      update_id =
          g_timeout_add (UPDATE_INTERVAL, (GSourceFunc) update_scale, pipeline);
    }
  } else {
    if (update_id) {
      g_source_remove (update_id);
      update_id = 0;
    }
  }
}

static gboolean
start_seek (GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  set_update_scale (FALSE);

  if (state == GST_STATE_PLAYING && flush_seek && scrub) {
    GST_DEBUG ("start scrub seek, PAUSE");
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
  }

  if (changed_id == 0 && flush_seek && scrub) {
    changed_id =
        g_signal_connect (hscale, "value_changed", G_CALLBACK (seek_cb),
        pipeline);
  }

  return FALSE;
}

static gboolean
stop_seek (GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
  if (changed_id) {
    g_signal_handler_disconnect (hscale, changed_id);
    changed_id = 0;
  }

  if (!flush_seek || !scrub) {
    GST_DEBUG ("do final seek");
    do_seek (widget);
  }

  if (seek_timeout_id != 0) {
    g_source_remove (seek_timeout_id);
    seek_timeout_id = 0;
    /* Still scrubbing, so the pipeline is playing, see if we need PAUSED
     * instead. */
    if (state == GST_STATE_PAUSED) {
      GST_DEBUG ("stop scrub seek, PAUSED");
      gst_element_set_state (pipeline, GST_STATE_PAUSED);
    }
  } else {
    if (state == GST_STATE_PLAYING) {
      GST_DEBUG ("stop scrub seek, PLAYING");
      gst_element_set_state (pipeline, GST_STATE_PLAYING);
    }
  }

  return FALSE;
}

static void
play_cb (GtkButton * button, gpointer data)
{
  GstStateChangeReturn ret;

  if (state != GST_STATE_PLAYING) {
    g_print ("PLAY pipeline\n");
    gtk_statusbar_pop (GTK_STATUSBAR (statusbar), status_id);

    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    switch (ret) {
      case GST_STATE_CHANGE_FAILURE:
        goto failed;
      case GST_STATE_CHANGE_NO_PREROLL:
        is_live = TRUE;
        break;
      default:
        break;
    }
    state = GST_STATE_PLAYING;
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Playing");
  }

  return;

failed:
  {
    g_print ("PLAY failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Play failed");
  }
}

static void
pause_cb (GtkButton * button, gpointer data)
{
  g_mutex_lock (&state_mutex);
  if (state != GST_STATE_PAUSED) {
    GstStateChangeReturn ret;

    gtk_statusbar_pop (GTK_STATUSBAR (statusbar), status_id);
    g_print ("PAUSE pipeline\n");
    ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
    switch (ret) {
      case GST_STATE_CHANGE_FAILURE:
        goto failed;
      case GST_STATE_CHANGE_NO_PREROLL:
        is_live = TRUE;
        break;
      default:
        break;
    }

    state = GST_STATE_PAUSED;
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Paused");
  }
  g_mutex_unlock (&state_mutex);

  return;

failed:
  {
    g_mutex_unlock (&state_mutex);
    g_print ("PAUSE failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Pause failed");
  }
}

static void
stop_cb (GtkButton * button, gpointer data)
{
  if (state != STOP_STATE) {
    GstStateChangeReturn ret;

    g_print ("READY pipeline\n");
    gtk_statusbar_pop (GTK_STATUSBAR (statusbar), status_id);

    g_mutex_lock (&state_mutex);
    ret = gst_element_set_state (pipeline, STOP_STATE);
    if (ret == GST_STATE_CHANGE_FAILURE)
      goto failed;

    state = STOP_STATE;
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Stopped");
    gtk_widget_queue_draw (video_window);

    is_live = FALSE;
    buffering = FALSE;
    set_update_scale (FALSE);
    set_scale (0.0);
    set_update_fill (FALSE);

    if (pipeline_type == 16)
      clear_streams (pipeline);
    g_mutex_unlock (&state_mutex);

#if 0
    /* if one uses parse_launch, play, stop and play again it fails as all the
     * pads after the demuxer can't be reconnected
     */
    if (!strcmp (pipelines[pipeline_type].name, "parse-launch")) {
      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (pipeline);

      g_list_free (seekable_elements);
      seekable_elements = NULL;
      g_list_free (seekable_pads);
      seekable_pads = NULL;
      g_list_free (rate_pads);
      rate_pads = NULL;

      pipeline = pipelines[pipeline_type].func (pipeline_spec);
      g_assert (pipeline);
      gst_element_set_state (pipeline, STOP_STATE);
      connect_bus_signals (pipeline);
    }
#endif
  }
  return;

failed:
  {
    g_mutex_unlock (&state_mutex);
    g_print ("STOP failed\n");
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Stop failed");
  }
}

static void
accurate_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  accurate_seek = gtk_toggle_button_get_active (button);
}

static void
key_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  keyframe_seek = gtk_toggle_button_get_active (button);
}

static void
loop_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  loop_seek = gtk_toggle_button_get_active (button);
  if (state == GST_STATE_PLAYING) {
    do_seek (hscale);
  }
}

static void
flush_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  flush_seek = gtk_toggle_button_get_active (button);
}

static void
scrub_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  scrub = gtk_toggle_button_get_active (button);
}

static void
play_scrub_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  play_scrub = gtk_toggle_button_get_active (button);
}

static void
skip_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  skip_seek = gtk_toggle_button_get_active (button);
  if (state == GST_STATE_PLAYING) {
    do_seek (hscale);
  }
}

static void
rate_spinbutton_changed_cb (GtkSpinButton * button, GstPipeline * pipeline)
{
  gboolean res = FALSE;
  GstEvent *s_event;
  GstSeekFlags flags;

  rate = gtk_spin_button_get_value (button);

  GST_DEBUG ("rate changed to %lf", rate);

  flags = 0;
  if (flush_seek)
    flags |= GST_SEEK_FLAG_FLUSH;
  if (loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (accurate_seek)
    flags |= GST_SEEK_FLAG_ACCURATE;
  if (keyframe_seek)
    flags |= GST_SEEK_FLAG_KEY_UNIT;
  if (skip_seek)
    flags |= GST_SEEK_FLAG_SKIP;

  if (rate >= 0.0) {
    s_event = gst_event_new_seek (rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, position,
        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    s_event = gst_event_new_seek (rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        GST_SEEK_TYPE_SET, position);
  }

  res = send_event (s_event);

  if (res) {
    if (flush_seek) {
      gst_element_get_state (GST_ELEMENT (pipeline), NULL, NULL, SEEK_TIMEOUT);
    }
  } else
    g_print ("seek failed\n");
}

static void
update_flag (GstPipeline * pipeline, gint num, gboolean state)
{
  gint flags;

  g_object_get (pipeline, "flags", &flags, NULL);
  if (state)
    flags |= (1 << num);
  else
    flags &= ~(1 << num);
  g_object_set (pipeline, "flags", flags, NULL);
}

static void
vis_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 3, state);
  gtk_widget_set_sensitive (vis_combo, state);
}

static void
audio_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 1, state);
  gtk_widget_set_sensitive (audio_combo, state);
}

static void
video_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 0, state);
  gtk_widget_set_sensitive (video_combo, state);
}

static void
text_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 2, state);
  gtk_widget_set_sensitive (text_combo, state);
}

static void
mute_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean mute;

  mute = gtk_toggle_button_get_active (button);
  g_object_set (pipeline, "mute", mute, NULL);
}

static void
download_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 7, state);
}

static void
buffer_toggle_cb (GtkToggleButton * button, GstPipeline * pipeline)
{
  gboolean state;

  state = gtk_toggle_button_get_active (button);
  update_flag (pipeline, 8, state);
}

static void
clear_streams (GstElement * pipeline)
{
  gint i;

  /* remove previous info */
  for (i = 0; i < n_video; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (video_combo), 0);
  for (i = 0; i < n_audio; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (audio_combo), 0);
  for (i = 0; i < n_text; i++)
    gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (text_combo), 0);

  n_audio = n_video = n_text = 0;
  gtk_widget_set_sensitive (video_combo, FALSE);
  gtk_widget_set_sensitive (audio_combo, FALSE);
  gtk_widget_set_sensitive (text_combo, FALSE);

  need_streams = TRUE;
}

static void
update_streams (GstPipeline * pipeline)
{
  gint i;

  if (pipeline_type == 16 && need_streams) {
    GstTagList *tags;
    gchar *name, *str;
    gint active_idx;
    gboolean state;

    /* remove previous info */
    clear_streams (GST_ELEMENT_CAST (pipeline));

    /* here we get and update the different streams detected by playbin */
    g_object_get (pipeline, "n-video", &n_video, NULL);
    g_object_get (pipeline, "n-audio", &n_audio, NULL);
    g_object_get (pipeline, "n-text", &n_text, NULL);

    g_print ("video %d, audio %d, text %d\n", n_video, n_audio, n_text);

    active_idx = 0;
    for (i = 0; i < n_video; i++) {
      g_signal_emit_by_name (pipeline, "get-video-tags", i, &tags);
      if (tags) {
        str = gst_tag_list_to_string (tags);
        g_print ("video %d: %s\n", i, str);
        g_free (str);
        gst_tag_list_unref (tags);
      }
      /* find good name for the label */
      name = g_strdup_printf ("video %d", i + 1);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (video_combo), name);
      g_free (name);
    }
    state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (video_checkbox));
    gtk_widget_set_sensitive (video_combo, state && n_video > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (video_combo), active_idx);

    active_idx = 0;
    for (i = 0; i < n_audio; i++) {
      g_signal_emit_by_name (pipeline, "get-audio-tags", i, &tags);
      if (tags) {
        str = gst_tag_list_to_string (tags);
        g_print ("audio %d: %s\n", i, str);
        g_free (str);
        gst_tag_list_unref (tags);
      }
      /* find good name for the label */
      name = g_strdup_printf ("audio %d", i + 1);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (audio_combo), name);
      g_free (name);
    }
    state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (audio_checkbox));
    gtk_widget_set_sensitive (audio_combo, state && n_audio > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (audio_combo), active_idx);

    active_idx = 0;
    for (i = 0; i < n_text; i++) {
      g_signal_emit_by_name (pipeline, "get-text-tags", i, &tags);

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

      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (text_combo), name);
      g_free (name);
    }
    state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (text_checkbox));
    gtk_widget_set_sensitive (text_combo, state && n_text > 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), active_idx);

    need_streams = FALSE;
  }
}

static void
video_combo_cb (GtkComboBox * combo, GstPipeline * pipeline)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current video track %d\n", active);
  g_object_set (pipeline, "current-video", active, NULL);
}

static void
audio_combo_cb (GtkComboBox * combo, GstPipeline * pipeline)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current audio track %d\n", active);
  g_object_set (pipeline, "current-audio", active, NULL);
}

static void
text_combo_cb (GtkComboBox * combo, GstPipeline * pipeline)
{
  gint active;

  active = gtk_combo_box_get_active (combo);

  g_print ("setting current text track %d\n", active);
  g_object_set (pipeline, "current-text", active, NULL);
}

static gboolean
filter_features (GstPluginFeature * feature, gpointer data)
{
  GstElementFactory *f;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;
  f = GST_ELEMENT_FACTORY (feature);
  if (!g_strrstr (gst_element_factory_get_metadata (f,
              GST_ELEMENT_METADATA_KLASS), "Visualization"))
    return FALSE;

  return TRUE;
}

static void
init_visualization_features (void)
{
  GList *list, *walk;

  vis_entries = g_array_new (FALSE, FALSE, sizeof (VisEntry));

  list = gst_registry_feature_filter (gst_registry_get (),
      filter_features, FALSE, NULL);

  for (walk = list; walk; walk = g_list_next (walk)) {
    VisEntry entry;
    const gchar *name;

    entry.factory = GST_ELEMENT_FACTORY (walk->data);
    name = gst_element_factory_get_metadata (entry.factory,
        GST_ELEMENT_METADATA_LONGNAME);

    g_array_append_val (vis_entries, entry);
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (vis_combo), name);
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX (vis_combo), 0);
  gst_plugin_feature_list_free (list);
}

static void
vis_combo_cb (GtkComboBox * combo, GstPipeline * pipeline)
{
  guint index;
  VisEntry *entry;
  GstElement *element;

  /* get the selected index and get the factory for this index */
  index = gtk_combo_box_get_active (GTK_COMBO_BOX (vis_combo));
  if (vis_entries->len > 0) {
    entry = &g_array_index (vis_entries, VisEntry, index);

    /* create an instance of the element from the factory */
    element = gst_element_factory_create (entry->factory, NULL);
    if (!element)
      return;

    /* set vis plugin for playbin */
    g_object_set (pipeline, "vis-plugin", element, NULL);
  }
}

static void
volume_spinbutton_changed_cb (GtkSpinButton * button, GstPipeline * pipeline)
{
  gdouble volume;

  volume = gtk_spin_button_get_value (button);

  g_object_set (pipeline, "volume", volume, NULL);
}

static void
volume_notify_cb (GstElement * pipeline, GParamSpec * arg, gpointer user_dat)
{
  gdouble cur_volume, new_volume;

  g_object_get (pipeline, "volume", &new_volume, NULL);
  cur_volume = gtk_spin_button_get_value (GTK_SPIN_BUTTON (volume_spinbutton));
  if (fabs (cur_volume - new_volume) > 0.001) {
    g_signal_handlers_block_by_func (volume_spinbutton,
        volume_spinbutton_changed_cb, pipeline);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (volume_spinbutton), new_volume);
    g_signal_handlers_unblock_by_func (volume_spinbutton,
        volume_spinbutton_changed_cb, pipeline);
  }
}

static void
shot_cb (GtkButton * button, gpointer data)
{
  GstBuffer *buffer;
  GstCaps *caps;

  /* convert to our desired format (RGB24) */
  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "RGB24",
      /* Note: we don't ask for a specific width/height here, so that
       * videoscale can adjust dimensions from a non-1/1 pixel aspect
       * ratio to a 1/1 pixel-aspect-ratio */
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

  /* convert the latest frame to the requested format */
  g_signal_emit_by_name (pipeline, "convert-frame", caps, &buffer);
  gst_caps_unref (caps);

  if (buffer) {
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
#if 0
    caps = GST_BUFFER_CAPS (buffer);
#endif
    /* FIXME, get the caps on the buffer somehow */
    caps = NULL;
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
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    pixbuf = gdk_pixbuf_new_from_data (map.data,
        GDK_COLORSPACE_RGB, FALSE, 8, width, height,
        GST_ROUND_UP_4 (width * 3), NULL, NULL);

    /* save the pixbuf */
    gdk_pixbuf_save (pixbuf, "snapshot.png", "png", &error, NULL);
    gst_buffer_unmap (buffer, &map);
    g_clear_error (&error);

  done:
    gst_buffer_unref (buffer);
  }
}

/* called when the Step button is pressed */
static void
step_cb (GtkButton * button, gpointer data)
{
  GstEvent *event;
  GstFormat format;
  guint64 amount;
  gdouble rate;
  gboolean flush, res;
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (format_combo));
  amount =
      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON
      (step_amount_spinbutton));
  rate = gtk_spin_button_get_value (GTK_SPIN_BUTTON (step_rate_spinbutton));
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

  res = send_event (event);

  if (!res) {
    g_print ("Sending step event failed\n");
  }
}

static void
message_received (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);
  g_print ("message from \"%s\" (%s): ",
      GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))),
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
  if (s) {
    gchar *sstr;

    sstr = gst_structure_to_string (s);
    g_print ("%s\n", sstr);
    g_free (sstr);
  } else {
    g_print ("no message details\n");
  }
}

static gboolean shuttling = FALSE;
static gdouble shuttle_rate = 0.0;
static gdouble play_rate = 1.0;

static void
do_shuttle (GstElement * element)
{
  guint64 duration;

  if (shuttling)
    duration = 40 * GST_MSECOND;
  else
    duration = 0;

  gst_element_send_event (element,
      gst_event_new_step (GST_FORMAT_TIME, duration, shuttle_rate, FALSE,
          FALSE));
}

static void
msg_sync_step_done (GstBus * bus, GstMessage * message, GstElement * element)
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

  if (g_mutex_trylock (&state_mutex)) {
    if (shuttling)
      do_shuttle (element);
    g_mutex_unlock (&state_mutex);
  } else {
    /* ignore step messages that come while we are doing a state change */
    g_print ("state change is busy\n");
  }
}

static void
shuttle_toggled (GtkToggleButton * button, GstElement * element)
{
  gboolean active;

  active = gtk_toggle_button_get_active (button);

  if (active != shuttling) {
    shuttling = active;
    g_print ("shuttling %s\n", shuttling ? "active" : "inactive");
    if (active) {
      shuttle_rate = 0.0;
      play_rate = 1.0;
      pause_cb (NULL, NULL);
      gst_element_get_state (element, NULL, NULL, -1);
    }
  }
}

static void
shuttle_rate_switch (GstElement * element)
{
  GstSeekFlags flags;
  GstEvent *s_event;
  gboolean res;

  if (state == GST_STATE_PLAYING) {
    /* pause when we need to */
    pause_cb (NULL, NULL);
    gst_element_get_state (element, NULL, NULL, -1);
  }

  if (play_rate == 1.0)
    play_rate = -1.0;
  else
    play_rate = 1.0;

  g_print ("rate changed to %lf %" GST_TIME_FORMAT "\n", play_rate,
      GST_TIME_ARGS (position));

  flags = GST_SEEK_FLAG_FLUSH;
  flags |= GST_SEEK_FLAG_ACCURATE;

  if (play_rate >= 0.0) {
    s_event = gst_event_new_seek (play_rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, position,
        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  } else {
    s_event = gst_event_new_seek (play_rate,
        GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        GST_SEEK_TYPE_SET, position);
  }
  res = send_event (s_event);
  if (res) {
    gst_element_get_state (element, NULL, NULL, SEEK_TIMEOUT);
  } else {
    g_print ("seek failed\n");
  }
}

static void
shuttle_value_changed (GtkRange * range, GstElement * element)
{
  gdouble rate;

  rate = gtk_adjustment_get_value (shuttle_adjustment);

  if (rate == 0.0) {
    g_print ("rate 0.0, pause\n");
    pause_cb (NULL, NULL);
    gst_element_get_state (element, NULL, NULL, -1);
  } else {
    g_print ("rate changed %0.3g\n", rate);

    if ((rate < 0.0 && play_rate > 0.0) || (rate > 0.0 && play_rate < 0.0)) {
      shuttle_rate_switch (element);
    }

    shuttle_rate = ABS (rate);
    if (state != GST_STATE_PLAYING) {
      do_shuttle (element);
      play_cb (NULL, NULL);
    }
  }
}

static void
msg_async_done (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  GST_DEBUG ("async done");
  /* when we get ASYNC_DONE we can query position, duration and other
   * properties */
  update_scale (pipeline);

  /* update the available streams */
  update_streams (pipeline);
}

static void
msg_state_changed (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);

  /* We only care about state changed on the pipeline */
  if (s && GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (pipeline)) {
    GstState old, new, pending;

    gst_message_parse_state_changed (message, &old, &new, &pending);

    /* When state of the pipeline changes to paused or playing we start updating scale */
    if (new == GST_STATE_PLAYING) {
      set_update_scale (TRUE);
    } else {
      set_update_scale (FALSE);
    }
  }
}

static void
msg_segment_done (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  GstEvent *s_event;
  GstSeekFlags flags;
  gboolean res;
  GstFormat format;

  GST_DEBUG ("position is %" GST_TIME_FORMAT, GST_TIME_ARGS (position));
  gst_message_parse_segment_done (message, &format, &position);
  GST_DEBUG ("end of segment at %" GST_TIME_FORMAT, GST_TIME_ARGS (position));

  flags = 0;
  /* in the segment-done callback we never flush as this would not make sense
   * for seamless playback. */
  if (loop_seek)
    flags |= GST_SEEK_FLAG_SEGMENT;
  if (skip_seek)
    flags |= GST_SEEK_FLAG_SKIP;

  s_event = gst_event_new_seek (rate,
      GST_FORMAT_TIME, flags, GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
      GST_SEEK_TYPE_SET, duration);

  GST_DEBUG ("restart loop with rate %lf to 0 / %" GST_TIME_FORMAT,
      rate, GST_TIME_ARGS (duration));

  res = send_event (s_event);
  if (!res)
    g_print ("segment seek failed\n");
}

/* in stream buffering mode we PAUSE the pipeline until we receive a 100%
 * message */
static void
do_stream_buffering (gint percent)
{
  gchar *bufstr;

  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), status_id);
  bufstr = g_strdup_printf ("Buffering...%d", percent);
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, bufstr);
  g_free (bufstr);

  if (percent == 100) {
    /* a 100% message means buffering is done */
    buffering = FALSE;
    /* if the desired state is playing, go back */
    if (state == GST_STATE_PLAYING) {
      /* no state management needed for live pipelines */
      if (!is_live) {
        fprintf (stderr, "Done buffering, setting pipeline to PLAYING ...\n");
        gst_element_set_state (pipeline, GST_STATE_PLAYING);
      }
      gtk_statusbar_pop (GTK_STATUSBAR (statusbar), status_id);
      gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Playing");
    }
  } else {
    /* buffering busy */
    if (!buffering && state == GST_STATE_PLAYING) {
      /* we were not buffering but PLAYING, PAUSE  the pipeline. */
      if (!is_live) {
        fprintf (stderr, "Buffering, setting pipeline to PAUSED ...\n");
        gst_element_set_state (pipeline, GST_STATE_PAUSED);
      }
    }
    buffering = TRUE;
  }
}

static void
do_download_buffering (gint percent)
{
  if (!buffering && percent < 100) {
    gchar *bufstr;

    buffering = TRUE;

    bufstr = g_strdup_printf ("Downloading...");
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, bufstr);
    g_free (bufstr);

    /* once we get a buffering message, we'll do the fill update */
    set_update_fill (TRUE);

    if (state == GST_STATE_PLAYING && !is_live) {
      fprintf (stderr, "Downloading, setting pipeline to PAUSED ...\n");
      gst_element_set_state (pipeline, GST_STATE_PAUSED);
      /* user has to manually start the playback */
      state = GST_STATE_PAUSED;
    }
  }
}

static void
msg_buffering (GstBus * bus, GstMessage * message, GstPipeline * data)
{
  gint percent;

  gst_message_parse_buffering (message, &percent);

  /* get more stats */
  gst_message_parse_buffering_stats (message, &mode, NULL, NULL,
      &buffering_left);

  switch (mode) {
    case GST_BUFFERING_DOWNLOAD:
      do_download_buffering (percent);
      break;
    case GST_BUFFERING_LIVE:
    case GST_BUFFERING_TIMESHIFT:
    case GST_BUFFERING_STREAM:
      do_stream_buffering (percent);
      break;
  }
}

static void
msg_clock_lost (GstBus * bus, GstMessage * message, GstPipeline * data)
{
  g_print ("clock lost! PAUSE and PLAY to select a new clock\n");

  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

#ifdef HAVE_X11

static gulong embed_xid = 0;

/* We set the xid here in response to the prepare-window-handle message via a
 * bus sync handler because we don't know the actual videosink used from the
 * start (as we don't know the pipeline, or bin elements such as autovideosink
 * or gconfvideosink may be used which create the actual videosink only once
 * the pipeline is started) */
static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, GstPipeline * data)
{
  GstElement *element;

  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  element = GST_ELEMENT (GST_MESSAGE_SRC (message));

  g_print ("got prepare-window-handle, setting XID %lu\n", embed_xid);

  /* Should have been initialised from main thread before (can't use
   * GDK_WINDOW_XID here with Gtk+ >= 2.18, because the sync handler will
   * be called from a streaming thread and GDK_WINDOW_XID maps to more than
   * a simple structure lookup with Gtk+ >= 2.18, where 'more' is stuff that
   * shouldn't be done from a non-GUI thread without explicit locking).  */
  g_assert (embed_xid != 0);

  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (element), embed_xid);
  return GST_BUS_PASS;
}
#endif

static gboolean
draw_cb (GtkWidget * widget, cairo_t * cr, gpointer data)
{
  if (state < GST_STATE_PAUSED) {
    int width, height;

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, width, height);
    cairo_fill (cr);
    return TRUE;
  }
  return FALSE;
}

static void
realize_cb (GtkWidget * widget, gpointer data)
{
  GdkWindow *window = gtk_widget_get_window (widget);

  /* This is here just for pedagogical purposes, GDK_WINDOW_XID will call it
   * as well */
  if (!gdk_window_ensure_native (window))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

#ifdef HAVE_X11
  embed_xid = GDK_WINDOW_XID (window);
  g_print ("Window realize: video window XID = %lu\n", embed_xid);
#endif
}

static void
msg_eos (GstBus * bus, GstMessage * message, GstPipeline * data)
{
  message_received (bus, message, data);

  /* Set new uri for playerbins and continue playback */
  if (l && (pipeline_type == 14 || pipeline_type == 16)) {
    stop_cb (NULL, NULL);
    l = g_list_next (l);
    if (l) {
      playerbin_set_uri (GST_ELEMENT (data), l->data);
      play_cb (NULL, NULL);
    }
  }
}

static void
msg_step_done (GstBus * bus, GstMessage * message, GstPipeline * data)
{
  if (!shuttling)
    message_received (bus, message, data);
}

static void
connect_bus_signals (GstElement * pipeline)
{
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

#ifdef HAVE_X11
  /* handle prepare-window-handle element message synchronously */
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) bus_sync_handler,
      pipeline, NULL);
#endif

  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  gst_bus_enable_sync_message_emission (bus);

  g_signal_connect (bus, "message::state-changed",
      (GCallback) msg_state_changed, pipeline);
  g_signal_connect (bus, "message::segment-done", (GCallback) msg_segment_done,
      pipeline);
  g_signal_connect (bus, "message::async-done", (GCallback) msg_async_done,
      pipeline);

  g_signal_connect (bus, "message::new-clock", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::clock-lost", (GCallback) msg_clock_lost,
      pipeline);
  g_signal_connect (bus, "message::error", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::warning", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::eos", (GCallback) msg_eos, pipeline);
  g_signal_connect (bus, "message::tag", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::element", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::segment-done", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::buffering", (GCallback) msg_buffering,
      pipeline);
//  g_signal_connect (bus, "message::step-done", (GCallback) msg_step_done,
//      pipeline);
  g_signal_connect (bus, "message::step-start", (GCallback) msg_step_done,
      pipeline);
  g_signal_connect (bus, "sync-message::step-done",
      (GCallback) msg_sync_step_done, pipeline);

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
delete_event_cb (void)
{
  stop_cb (NULL, NULL);
  gtk_main_quit ();
}

static void
print_usage (int argc, char **argv)
{
  gint i;

  g_print ("usage: %s <type> <filename>\n", argv[0]);
  g_print ("   possible types:\n");

  for (i = 0; i < NUM_TYPES; i++) {
    g_print ("     %d = %s\n", i, pipelines[i].name);
  }
}

static gboolean
read_joystick (GIOChannel * source, GIOCondition condition, gpointer user_data)
{
  gchar buf[sizeof (struct js_event)];
  struct js_event *js = (struct js_event *) buf;
  GError *err = NULL;
  gsize bytes_read = 0;
  GIOStatus result;

  result =
      g_io_channel_read_chars (source, buf, sizeof (struct js_event),
      &bytes_read, &err);
  if (err) {
    g_print ("error reading from joystick: %s", err->message);
    g_clear_error (&err);
    return FALSE;
  } else if (bytes_read != sizeof (struct js_event)) {
    g_print ("error reading joystick, read %u bytes of %u\n",
        (guint) bytes_read, (guint) sizeof (struct js_event));
    return TRUE;
  } else if (result != G_IO_STATUS_NORMAL) {
    g_print ("reading from joystick returned status %d", result);
  }

  switch (js->type & ~JS_EVENT_INIT) {
    case JS_EVENT_AXIS:
      if (js->number == 0) {
        gdouble new_rate = (gdouble) (js->value) / 3000;
        g_print ("Got: %d (rate %g)\n", js->value, new_rate);
        if (shuttling)
          gtk_adjustment_set_value (shuttle_adjustment, new_rate);
      }
      break;
  }

  return TRUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *hbox, *vbox, *panel, *expander, *pb2vbox, *boxes,
      *flaggrid, *boxes2, *step;
  GtkWidget *play_button, *pause_button, *stop_button, *shot_button;
  GtkWidget *accurate_checkbox, *key_checkbox, *loop_checkbox, *flush_checkbox;
  GtkWidget *scrub_checkbox, *play_scrub_checkbox;
  GtkWidget *rate_label, *volume_label;
  GOptionEntry options[] = {
    {"stats", 's', 0, G_OPTION_ARG_NONE, &stats,
        "Show pad stats", NULL},
    {"elem", 'e', 0, G_OPTION_ARG_NONE, &elem_seek,
        "Seek on elements instead of pads", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Verbose properties", NULL},
    {"joystick", 'j', 0, G_OPTION_ARG_STRING, &js_device,
        "Joystick device to use", NULL},
    {NULL}
  };
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("- test seeking in gsteamer");
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
  GST_DEBUG_CATEGORY_INIT (seek_debug, "seek", 0, "seek example");

  if (argc != 3) {
    print_usage (argc, argv);
    exit (-1);
  }

  pipeline_type = atoi (argv[1]);

  if (pipeline_type < 0 || pipeline_type >= NUM_TYPES) {
    print_usage (argc, argv);
    exit (-1);
  }

  pipeline_spec = argv[2];

  if (js_device == NULL)
    js_device = g_strdup ("/dev/input/js0");

  js_fd = g_open (js_device, O_RDONLY, 0);
  if (js_fd < 0) {
    g_print ("Failed to open joystick device %s\n", js_device);
    exit (-1);
  }

  if (g_strrstr (pipeline_spec, "*") != NULL ||
      g_strrstr (pipeline_spec, "?") != NULL) {
    paths = handle_wildcards (pipeline_spec);
  } else {
    paths = g_list_prepend (paths, g_strdup (pipeline_spec));
  }

  if (!paths) {
    g_print ("opening %s failed\n", pipeline_spec);
    exit (-1);
  }

  l = paths;

  pipeline = pipelines[pipeline_type].func ((gchar *) l->data);
  g_assert (pipeline);

  /* initialize gui elements ... */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  video_window = gtk_drawing_area_new ();
  g_signal_connect (video_window, "draw", G_CALLBACK (draw_cb), NULL);
  g_signal_connect (video_window, "realize", G_CALLBACK (realize_cb), NULL);

  statusbar = gtk_statusbar_new ();
  status_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "seek");
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), status_id, "Stopped");
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  flaggrid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  /* media controls */
  play_button = gtk_button_new_from_icon_name ("media-playback-start",
      GTK_ICON_SIZE_BUTTON);
  pause_button = gtk_button_new_from_icon_name ("media-playback-pause",
      GTK_ICON_SIZE_BUTTON);
  stop_button = gtk_button_new_from_icon_name ("media-playback-stop",
      GTK_ICON_SIZE_BUTTON);

  /* seek flags */
  accurate_checkbox = gtk_check_button_new_with_label ("Accurate Seek");
  key_checkbox = gtk_check_button_new_with_label ("Key-unit Seek");
  loop_checkbox = gtk_check_button_new_with_label ("Loop");
  flush_checkbox = gtk_check_button_new_with_label ("Flush");
  scrub_checkbox = gtk_check_button_new_with_label ("Scrub");
  play_scrub_checkbox = gtk_check_button_new_with_label ("Play Scrub");
  skip_checkbox = gtk_check_button_new_with_label ("Play Skip");
  rate_spinbutton = gtk_spin_button_new_with_range (-100, 100, 0.1);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rate_spinbutton), 3);
  rate_label = gtk_label_new ("Rate");

  gtk_widget_set_tooltip_text (accurate_checkbox,
      "accurate position is requested, this might be considerably slower for some formats");
  gtk_widget_set_tooltip_text (key_checkbox,
      "seek to the nearest keyframe. This might be faster but less accurate");
  gtk_widget_set_tooltip_text (loop_checkbox, "loop playback");
  gtk_widget_set_tooltip_text (flush_checkbox, "flush pipeline after seeking");
  gtk_widget_set_tooltip_text (rate_spinbutton, "define the playback rate, "
      "negative value trigger reverse playback");
  gtk_widget_set_tooltip_text (scrub_checkbox, "show images while seeking");
  gtk_widget_set_tooltip_text (play_scrub_checkbox, "play video while seeking");
  gtk_widget_set_tooltip_text (skip_checkbox,
      "Skip frames while playing at high frame rates");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (flush_checkbox), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scrub_checkbox), TRUE);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (rate_spinbutton), rate);

  /* step expander */
  {
    GtkWidget *hbox;

    step = gtk_expander_new ("step options");
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    format_combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (format_combo),
        "frames");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (format_combo),
        "time (ms)");
    gtk_combo_box_set_active (GTK_COMBO_BOX (format_combo), 0);
    gtk_box_pack_start (GTK_BOX (hbox), format_combo, FALSE, FALSE, 2);

    step_amount_spinbutton = gtk_spin_button_new_with_range (1, 1000, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (step_amount_spinbutton), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (step_amount_spinbutton), 1.0);
    gtk_box_pack_start (GTK_BOX (hbox), step_amount_spinbutton, FALSE, FALSE,
        2);

    step_rate_spinbutton = gtk_spin_button_new_with_range (0.0, 100, 0.1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (step_rate_spinbutton), 3);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (step_rate_spinbutton), 1.0);
    gtk_box_pack_start (GTK_BOX (hbox), step_rate_spinbutton, FALSE, FALSE, 2);

    step_button = gtk_button_new_from_icon_name ("media-seek-forward",
        GTK_ICON_SIZE_BUTTON);
    gtk_button_set_label (GTK_BUTTON (step_button), "Step");
    gtk_box_pack_start (GTK_BOX (hbox), step_button, FALSE, FALSE, 2);

    g_signal_connect (G_OBJECT (step_button), "clicked", G_CALLBACK (step_cb),
        pipeline);

    /* shuttle scale */
    shuttle_checkbox = gtk_check_button_new_with_label ("Shuttle");
    gtk_box_pack_start (GTK_BOX (hbox), shuttle_checkbox, FALSE, FALSE, 2);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (shuttle_checkbox), FALSE);
    g_signal_connect (shuttle_checkbox, "toggled", G_CALLBACK (shuttle_toggled),
        pipeline);

    shuttle_adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (0.0, -3.00, 4.0, 0.1, 1.0, 1.0));
    shuttle_hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL,
        shuttle_adjustment);
    gtk_scale_set_digits (GTK_SCALE (shuttle_hscale), 2);
    gtk_scale_set_value_pos (GTK_SCALE (shuttle_hscale), GTK_POS_TOP);
    g_signal_connect (shuttle_hscale, "value_changed",
        G_CALLBACK (shuttle_value_changed), pipeline);
    g_signal_connect (shuttle_hscale, "format_value",
        G_CALLBACK (shuttle_format_value), pipeline);

    gtk_box_pack_start (GTK_BOX (hbox), shuttle_hscale, TRUE, TRUE, 2);

    gtk_container_add (GTK_CONTAINER (step), hbox);
  }

  /* seek bar */
  adjustment =
      GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.00, 100.0, 0.1, 1.0, 1.0));
  hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_set_digits (GTK_SCALE (hscale), 2);
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_RIGHT);
  gtk_range_set_show_fill_level (GTK_RANGE (hscale), TRUE);
  gtk_range_set_fill_level (GTK_RANGE (hscale), 100.0);

  g_signal_connect (hscale, "button_press_event", G_CALLBACK (start_seek),
      pipeline);
  g_signal_connect (hscale, "button_release_event", G_CALLBACK (stop_seek),
      pipeline);
  g_signal_connect (hscale, "format_value", G_CALLBACK (format_value),
      pipeline);

  if (pipeline_type == 16) {
    /* the playbin panel controls for the video/audio/subtitle tracks */
    panel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    video_combo = gtk_combo_box_text_new ();
    audio_combo = gtk_combo_box_text_new ();
    text_combo = gtk_combo_box_text_new ();
    gtk_widget_set_sensitive (video_combo, FALSE);
    gtk_widget_set_sensitive (audio_combo, FALSE);
    gtk_widget_set_sensitive (text_combo, FALSE);
    gtk_box_pack_start (GTK_BOX (panel), video_combo, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (panel), audio_combo, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (panel), text_combo, TRUE, TRUE, 2);
    g_signal_connect (G_OBJECT (video_combo), "changed",
        G_CALLBACK (video_combo_cb), pipeline);
    g_signal_connect (G_OBJECT (audio_combo), "changed",
        G_CALLBACK (audio_combo_cb), pipeline);
    g_signal_connect (G_OBJECT (text_combo), "changed",
        G_CALLBACK (text_combo_cb), pipeline);
    /* playbin panel for flag checkboxes and volume/mute */
    boxes = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    vis_checkbox = gtk_check_button_new_with_label ("Vis");
    video_checkbox = gtk_check_button_new_with_label ("Video");
    audio_checkbox = gtk_check_button_new_with_label ("Audio");
    text_checkbox = gtk_check_button_new_with_label ("Text");
    mute_checkbox = gtk_check_button_new_with_label ("Mute");
    download_checkbox = gtk_check_button_new_with_label ("Download");
    buffer_checkbox = gtk_check_button_new_with_label ("Buffer");
    volume_label = gtk_label_new ("Volume");
    volume_spinbutton = gtk_spin_button_new_with_range (0, 10.0, 0.1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (volume_spinbutton), 1.0);
    gtk_box_pack_start (GTK_BOX (boxes), video_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), audio_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), text_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), vis_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), mute_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), download_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), buffer_checkbox, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), volume_label, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes), volume_spinbutton, TRUE, TRUE, 2);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vis_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (audio_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (video_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (text_checkbox), TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mute_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (download_checkbox), FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (buffer_checkbox), FALSE);
    g_signal_connect (G_OBJECT (vis_checkbox), "toggled",
        G_CALLBACK (vis_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (audio_checkbox), "toggled",
        G_CALLBACK (audio_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (video_checkbox), "toggled",
        G_CALLBACK (video_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (text_checkbox), "toggled",
        G_CALLBACK (text_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (mute_checkbox), "toggled",
        G_CALLBACK (mute_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (download_checkbox), "toggled",
        G_CALLBACK (download_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (buffer_checkbox), "toggled",
        G_CALLBACK (buffer_toggle_cb), pipeline);
    g_signal_connect (G_OBJECT (volume_spinbutton), "value_changed",
        G_CALLBACK (volume_spinbutton_changed_cb), pipeline);
    /* playbin panel for snapshot */
    boxes2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    shot_button = gtk_button_new_from_icon_name ("document-save",
        GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text (shot_button,
        "save a screenshot .png in the current directory");
    g_signal_connect (G_OBJECT (shot_button), "clicked", G_CALLBACK (shot_cb),
        pipeline);
    vis_combo = gtk_combo_box_text_new ();
    g_signal_connect (G_OBJECT (vis_combo), "changed",
        G_CALLBACK (vis_combo_cb), pipeline);
    gtk_widget_set_sensitive (vis_combo, FALSE);
    gtk_box_pack_start (GTK_BOX (boxes2), shot_button, TRUE, TRUE, 2);
    gtk_box_pack_start (GTK_BOX (boxes2), vis_combo, TRUE, TRUE, 2);

    /* fill the vis combo box and the array of factories */
    init_visualization_features ();
  } else {
    panel = boxes = boxes2 = NULL;
  }

  /* do the packing stuff ... */
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 96);
  /* FIXME: can we avoid this for audio only? */
  gtk_widget_set_size_request (GTK_WIDGET (video_window), -1,
      DEFAULT_VIDEO_HEIGHT);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), video_window, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), play_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), pause_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), stop_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), flaggrid, FALSE, FALSE, 2);
  gtk_grid_attach (GTK_GRID (flaggrid), accurate_checkbox, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), flush_checkbox, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), loop_checkbox, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), key_checkbox, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), scrub_checkbox, 1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), play_scrub_checkbox, 2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), skip_checkbox, 3, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), rate_label, 4, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (flaggrid), rate_spinbutton, 4, 1, 1, 1);

  if (panel && boxes && boxes2) {
    expander = gtk_expander_new ("playbin options");
    pb2vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start (GTK_BOX (pb2vbox), panel, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (pb2vbox), boxes, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (pb2vbox), boxes2, FALSE, FALSE, 2);
    gtk_container_add (GTK_CONTAINER (expander), pb2vbox);
    gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, FALSE, 2);
  }
  gtk_box_pack_start (GTK_BOX (vbox), step, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hscale, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, FALSE, 2);

  /* connect things ... */
  g_signal_connect (G_OBJECT (play_button), "clicked", G_CALLBACK (play_cb),
      pipeline);
  g_signal_connect (G_OBJECT (pause_button), "clicked", G_CALLBACK (pause_cb),
      pipeline);
  g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb),
      pipeline);
  g_signal_connect (G_OBJECT (accurate_checkbox), "toggled",
      G_CALLBACK (accurate_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (key_checkbox), "toggled",
      G_CALLBACK (key_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (loop_checkbox), "toggled",
      G_CALLBACK (loop_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (flush_checkbox), "toggled",
      G_CALLBACK (flush_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (scrub_checkbox), "toggled",
      G_CALLBACK (scrub_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (play_scrub_checkbox), "toggled",
      G_CALLBACK (play_scrub_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (skip_checkbox), "toggled",
      G_CALLBACK (skip_toggle_cb), pipeline);
  g_signal_connect (G_OBJECT (rate_spinbutton), "value_changed",
      G_CALLBACK (rate_spinbutton_changed_cb), pipeline);

  g_signal_connect (G_OBJECT (window), "delete-event", delete_event_cb, NULL);

  /* show the gui. */
  gtk_widget_show_all (window);

  /* realize window now so that the video window gets created and we can
   * obtain its XID before the pipeline is started up and the videosink
   * asks for the XID of the window to render onto */
  gtk_widget_realize (window);

#ifdef HAVE_X11
  /* we should have the XID now */
  g_assert (embed_xid != 0);
#endif

  if (verbose) {
    g_signal_connect (pipeline, "deep_notify",
        G_CALLBACK (gst_object_default_deep_notify), NULL);
  }

  {
    GIOChannel *js_watch = g_io_channel_unix_new (js_fd);
    g_io_channel_set_encoding (js_watch, NULL, NULL);
    g_io_add_watch (js_watch, G_IO_IN, read_joystick, NULL);
  }

  connect_bus_signals (pipeline);
  gtk_main ();

  g_print ("NULL pipeline\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("free pipeline\n");
  g_array_free (vis_entries, TRUE);
  gst_object_unref (pipeline);

  g_list_foreach (paths, (GFunc) g_free, NULL);
  g_list_free (paths);

  return 0;
}
