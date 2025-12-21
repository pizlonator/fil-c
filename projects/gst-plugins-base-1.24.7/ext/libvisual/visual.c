/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <otte@gnome.org>
 *               2012 Stefan Sauer <ensonic@users.sf.net>
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

#include "visual.h"

GST_DEBUG_CATEGORY_EXTERN (libvisual_debug);
#define GST_CAT_DEFAULT (libvisual_debug)

/* amounf of samples before we can feed libvisual */
#define VISUAL_SAMPLES  512

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER_CAPS "xRGB, RGB"
#else
#define RGB_ORDER_CAPS "BGRx, BGR"
#endif

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (" { " RGB_ORDER_CAPS ", RGB16 } "))
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, " "channels = (int) { 1, 2 }, "
        "rate = (int) { 8000, 11250, 22500, 32000, 44100, 48000, 96000 }")
    );


static void gst_visual_init (GstVisual * visual);
static void gst_visual_finalize (GObject * object);

static gboolean gst_visual_setup (GstAudioVisualizer * bscope);
static gboolean gst_visual_render (GstAudioVisualizer * bscope,
    GstBuffer * audio, GstVideoFrame * video);

static GstElementClass *parent_class = NULL;

GType
gst_visual_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0)) {
    static const GTypeInfo info = {
      sizeof (GstVisualClass),
      NULL,
      NULL,
      gst_visual_class_init,
      NULL,
      NULL,
      sizeof (GstVisual),
      0,
      (GInstanceInitFunc) gst_visual_init,
    };

    type =
        g_type_register_static (GST_TYPE_AUDIO_VISUALIZER, "GstVisual",
        &info, 0);
  }
  return type;
}

void
gst_visual_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioVisualizerClass *scope_class = (GstAudioVisualizerClass *) g_class;
  GstVisualClass *klass = (GstVisualClass *) g_class;

  klass->plugin = class_data;

  if (class_data == NULL) {
    parent_class = g_type_class_peek_parent (g_class);
  } else {
    gchar *longname = g_strdup_printf ("libvisual %s plugin v.%s",
        klass->plugin->info->name, klass->plugin->info->version);

    /* FIXME: improve to only register what plugin supports? */
    gst_element_class_add_static_pad_template (element_class, &src_template);
    gst_element_class_add_static_pad_template (element_class, &sink_template);

    gst_element_class_set_metadata (element_class,
        longname, "Visualization",
        klass->plugin->info->about, "Benjamin Otte <otte@gnome.org>");

    g_free (longname);
  }

  gobject_class->finalize = gst_visual_finalize;

  scope_class->setup = GST_DEBUG_FUNCPTR (gst_visual_setup);
  scope_class->render = GST_DEBUG_FUNCPTR (gst_visual_render);

  gst_type_mark_as_plugin_api (GST_TYPE_VISUAL, 0);
}

static void
gst_visual_init (GstVisual * visual)
{
  /* do nothing */
}

static void
gst_visual_clear_actors (GstVisual * visual)
{
  if (visual->actor) {
    visual_object_unref (VISUAL_OBJECT (visual->actor));
    visual->actor = NULL;
  }
  if (visual->video) {
    visual_object_unref (VISUAL_OBJECT (visual->video));
    visual->video = NULL;
  }
  if (visual->audio) {
    visual_object_unref (VISUAL_OBJECT (visual->audio));
    visual->audio = NULL;
  }
}

static void
gst_visual_finalize (GObject * object)
{
  GstVisual *visual = GST_VISUAL (object);

  gst_visual_clear_actors (visual);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
gst_visual_setup (GstAudioVisualizer * bscope)
{
  GstVisual *visual = GST_VISUAL (bscope);
  gint depth;

  gst_visual_clear_actors (visual);

  /* FIXME: we need to know how many bits we actually have in memory */
  depth = bscope->vinfo.finfo->pixel_stride[0];
  if (bscope->vinfo.finfo->bits >= 8) {
    depth *= 8;
  }

  visual->actor =
      visual_actor_new (GST_VISUAL_GET_CLASS (visual)->plugin->info->plugname);
  visual->video = visual_video_new ();
  visual->audio = visual_audio_new ();
  /* can't have a play without actors */
  if (!visual->actor || !visual->video)
    goto no_actors;

  if (visual_actor_realize (visual->actor) != 0)
    goto no_realize;

  visual_actor_set_video (visual->actor, visual->video);

  visual_video_set_depth (visual->video,
      visual_video_depth_enum_from_value (depth));
  visual_video_set_dimension (visual->video,
      GST_VIDEO_INFO_WIDTH (&bscope->vinfo),
      GST_VIDEO_INFO_HEIGHT (&bscope->vinfo));
  visual_actor_video_negotiate (visual->actor, 0, FALSE, FALSE);

  GST_DEBUG_OBJECT (visual, "WxH: %dx%d, bpp: %d, depth: %d",
      GST_VIDEO_INFO_WIDTH (&bscope->vinfo),
      GST_VIDEO_INFO_HEIGHT (&bscope->vinfo), visual->video->bpp, depth);

  return TRUE;
  /* ERRORS */
no_actors:
  {
    GST_ELEMENT_ERROR (visual, LIBRARY, INIT, (NULL),
        ("could not create actors"));
    gst_visual_clear_actors (visual);
    return FALSE;
  }
no_realize:
  {
    GST_ELEMENT_ERROR (visual, LIBRARY, INIT, (NULL),
        ("could not realize actor"));
    gst_visual_clear_actors (visual);
    return FALSE;
  }
}

static gboolean
gst_visual_render (GstAudioVisualizer * bscope, GstBuffer * audio,
    GstVideoFrame * video)
{
  GstVisual *visual = GST_VISUAL (bscope);
  GstMapInfo amap;
  const guint16 *adata;
  gint i, channels;
  gboolean res = TRUE;
  VisBuffer *lbuf, *rbuf;
  guint16 ldata[VISUAL_SAMPLES], rdata[VISUAL_SAMPLES];
  VisAudioSampleRateType vrate;

  visual_video_set_buffer (visual->video, GST_VIDEO_FRAME_PLANE_DATA (video,
          0));
  visual_video_set_pitch (visual->video, GST_VIDEO_FRAME_PLANE_STRIDE (video,
          0));

  channels = GST_AUDIO_INFO_CHANNELS (&bscope->ainfo);

  gst_buffer_map (audio, &amap, GST_MAP_READ);
  adata = (const guint16 *) amap.data;

  lbuf = visual_buffer_new_with_buffer (ldata, sizeof (ldata), NULL);
  rbuf = visual_buffer_new_with_buffer (rdata, sizeof (rdata), NULL);

  if (channels == 2) {
    for (i = 0; i < VISUAL_SAMPLES; i++) {
      ldata[i] = *adata++;
      rdata[i] = *adata++;
    }
  } else {
    for (i = 0; i < VISUAL_SAMPLES; i++) {
      ldata[i] = *adata;
      rdata[i] = *adata++;
    }
  }

  /* TODO(ensonic): move to setup */
  switch (bscope->ainfo.rate) {
    case 8000:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_8000;
      break;
    case 11250:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_11250;
      break;
    case 22500:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_22500;
      break;
    case 32000:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_32000;
      break;
    case 44100:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_44100;
      break;
    case 48000:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_48000;
      break;
    case 96000:
      vrate = VISUAL_AUDIO_SAMPLE_RATE_96000;
      break;
    default:
      visual_object_unref (VISUAL_OBJECT (lbuf));
      visual_object_unref (VISUAL_OBJECT (rbuf));
      GST_ERROR_OBJECT (visual, "unsupported rate %d", bscope->ainfo.rate);
      res = FALSE;
      goto done;
  }

  visual_audio_samplepool_input_channel (visual->audio->samplepool,
      lbuf,
      vrate, VISUAL_AUDIO_SAMPLE_FORMAT_S16,
      (char *) VISUAL_AUDIO_CHANNEL_LEFT);
  visual_audio_samplepool_input_channel (visual->audio->samplepool, rbuf,
      vrate, VISUAL_AUDIO_SAMPLE_FORMAT_S16,
      (char *) VISUAL_AUDIO_CHANNEL_RIGHT);

  visual_object_unref (VISUAL_OBJECT (lbuf));
  visual_object_unref (VISUAL_OBJECT (rbuf));

  visual_audio_analyze (visual->audio);
  visual_actor_run (visual->actor, visual->audio);
  visual_video_set_buffer (visual->video, NULL);

  GST_DEBUG_OBJECT (visual, "rendered one frame");
done:
  gst_buffer_unmap (audio, &amap);

  return res;
}
