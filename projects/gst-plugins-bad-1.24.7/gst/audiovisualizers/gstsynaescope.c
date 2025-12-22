/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstsynaescope.c: frequency spectrum scope
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
 * SECTION:element-synaescope
 * @title: synaescope
 * @see_also: goom
 *
 * Synaescope is an audio visualisation element. It analyzes frequencies and
 * out-of phase properties of audio and draws this as clouds of stars.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! synaescope ! ximagesink
 * ]|
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsynaescope.h"

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

static GstStaticPadTemplate gst_synae_scope_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (RGB_ORDER))
    );

static GstStaticPadTemplate gst_synae_scope_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 2, " "channel-mask = (bitmask) 0x3")
    );


GST_DEBUG_CATEGORY_STATIC (synae_scope_debug);
#define GST_CAT_DEFAULT synae_scope_debug

static void gst_synae_scope_finalize (GObject * object);

static gboolean gst_synae_scope_setup (GstAudioVisualizer * scope);
static gboolean gst_synae_scope_render (GstAudioVisualizer * scope,
    GstBuffer * audio, GstVideoFrame * video);


G_DEFINE_TYPE_WITH_CODE (GstSynaeScope, gst_synae_scope,
    GST_TYPE_AUDIO_VISUALIZER, GST_DEBUG_CATEGORY_INIT (synae_scope_debug,
        "synaescope", 0, "synaescope"););
GST_ELEMENT_REGISTER_DEFINE (synaescope, "synaescope", GST_RANK_NONE,
    GST_TYPE_SYNAE_SCOPE);

static void
gst_synae_scope_class_init (GstSynaeScopeClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioVisualizerClass *scope_class = (GstAudioVisualizerClass *) g_class;

  gobject_class->finalize = gst_synae_scope_finalize;

  gst_element_class_set_static_metadata (element_class, "Synaescope",
      "Visualization",
      "Creates video visualizations of audio input, using stereo and pitch information",
      "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_synae_scope_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_synae_scope_sink_template);

  scope_class->setup = GST_DEBUG_FUNCPTR (gst_synae_scope_setup);
  scope_class->render = GST_DEBUG_FUNCPTR (gst_synae_scope_render);
}

static void
gst_synae_scope_init (GstSynaeScope * scope)
{
  guint32 *colors = scope->colors;
  guint *shade = scope->shade;
  guint i, r, g, b;

#define BOUND(x) ((x) > 255 ? 255 : (x))
#define PEAKIFY(x) BOUND((x) - (x)*(255-(x))/255/2)

  for (i = 0; i < 256; i++) {
    r = PEAKIFY ((i & 15 * 16));
    g = PEAKIFY ((i & 15) * 16 + (i & 15 * 16) / 4);
    b = PEAKIFY ((i & 15) * 16);

    colors[i] = (r << 16) | (g << 8) | b;
  }
#undef BOUND
#undef PEAKIFY

  for (i = 0; i < 256; i++)
    shade[i] = i * 200 >> 8;
}

static void
gst_synae_scope_finalize (GObject * object)
{
  GstSynaeScope *scope = GST_SYNAE_SCOPE (object);

  if (scope->fft_ctx) {
    gst_fft_s16_free (scope->fft_ctx);
    scope->fft_ctx = NULL;
  }
  if (scope->freq_data_l) {
    g_free (scope->freq_data_l);
    scope->freq_data_l = NULL;
  }
  if (scope->freq_data_r) {
    g_free (scope->freq_data_r);
    scope->freq_data_r = NULL;
  }
  if (scope->adata_l) {
    g_free (scope->adata_l);
    scope->adata_l = NULL;
  }
  if (scope->adata_r) {
    g_free (scope->adata_r);
    scope->adata_r = NULL;
  }

  G_OBJECT_CLASS (gst_synae_scope_parent_class)->finalize (object);
}

static gboolean
gst_synae_scope_setup (GstAudioVisualizer * bscope)
{
  GstSynaeScope *scope = GST_SYNAE_SCOPE (bscope);
  guint num_freq = GST_VIDEO_INFO_HEIGHT (&bscope->vinfo) + 1;

  if (scope->fft_ctx)
    gst_fft_s16_free (scope->fft_ctx);
  g_free (scope->freq_data_l);
  g_free (scope->freq_data_r);
  g_free (scope->adata_l);
  g_free (scope->adata_r);

  /* FIXME: we could have horizontal or vertical layout */

  /* we'd need this amount of samples per render() call */
  bscope->req_spf = num_freq * 2 - 2;
  scope->fft_ctx = gst_fft_s16_new (bscope->req_spf, FALSE);
  scope->freq_data_l = g_new (GstFFTS16Complex, num_freq);
  scope->freq_data_r = g_new (GstFFTS16Complex, num_freq);

  scope->adata_l = g_new (gint16, bscope->req_spf);
  scope->adata_r = g_new (gint16, bscope->req_spf);

  return TRUE;
}

static inline void
add_pixel (guint32 * _p, guint32 _c)
{
  guint8 *p = (guint8 *) _p;
  guint8 *c = (guint8 *) & _c;

  if (p[0] < 255 - c[0])
    p[0] += c[0];
  else
    p[0] = 255;
  if (p[1] < 255 - c[1])
    p[1] += c[1];
  else
    p[1] = 255;
  if (p[2] < 255 - c[2])
    p[2] += c[2];
  else
    p[2] = 255;
  if (p[3] < 255 - c[3])
    p[3] += c[3];
  else
    p[3] = 255;
}

static gboolean
gst_synae_scope_render (GstAudioVisualizer * bscope, GstBuffer * audio,
    GstVideoFrame * video)
{
  GstSynaeScope *scope = GST_SYNAE_SCOPE (bscope);
  GstMapInfo amap;
  guint32 *vdata;
  gint16 *adata;
  gint16 *adata_l = scope->adata_l;
  gint16 *adata_r = scope->adata_r;
  GstFFTS16Complex *fdata_l = scope->freq_data_l;
  GstFFTS16Complex *fdata_r = scope->freq_data_r;
  gint x, y;
  guint off;
  guint w = GST_VIDEO_INFO_WIDTH (&bscope->vinfo);
  guint h = GST_VIDEO_INFO_HEIGHT (&bscope->vinfo);
  guint32 *colors = scope->colors, c;
  guint *shade = scope->shade;
  //guint w2 = w /2;
  guint ch = GST_AUDIO_INFO_CHANNELS (&bscope->ainfo);
  guint num_samples;
  gint i, j, b;
  gint br, br1, br2;
  gint clarity;
  gdouble fc, r, l, rr, ll;
  gdouble frl, fil, frr, fir;
  const guint sl = 30;

  gst_buffer_map (audio, &amap, GST_MAP_READ);

  vdata = (guint32 *) GST_VIDEO_FRAME_PLANE_DATA (video, 0);
  adata = (gint16 *) amap.data;

  num_samples = amap.size / (ch * sizeof (gint16));

  /* deinterleave */
  for (i = 0, j = 0; i < num_samples; i++) {
    adata_l[i] = adata[j++];
    adata_r[i] = adata[j++];
  }

  /* run fft */
  /*gst_fft_s16_window (scope->fft_ctx, adata_l, GST_FFT_WINDOW_HAMMING); */
  gst_fft_s16_fft (scope->fft_ctx, adata_l, fdata_l);
  /*gst_fft_s16_window (scope->fft_ctx, adata_r, GST_FFT_WINDOW_HAMMING); */
  gst_fft_s16_fft (scope->fft_ctx, adata_r, fdata_r);

  /* draw stars */
  for (y = 0; y < h; y++) {
    b = h - y;
    frl = (gdouble) fdata_l[b].r;
    fil = (gdouble) fdata_l[b].i;
    frr = (gdouble) fdata_r[b].r;
    fir = (gdouble) fdata_r[b].i;

    ll = (frl + fil) * (frl + fil) + (frr - fir) * (frr - fir);
    l = sqrt (ll);
    rr = (frl - fil) * (frl - fil) + (frr + fir) * (frr + fir);
    r = sqrt (rr);
    /* out-of-phase'ness for this frequency component */
    clarity = (gint) (
        ((frl + fil) * (frl - fil) + (frr + fir) * (frr - fir)) /
        (ll + rr) * 256);
    fc = r + l;

    x = (guint) (r * w / fc);
    /* the brightness scaling factor was picked by experimenting */
    br = b * fc * 0.01;

    br1 = br * (clarity + 128) >> 8;
    br2 = br * (128 - clarity) >> 8;
    br1 = CLAMP (br1, 0, 255);
    br2 = CLAMP (br2, 0, 255);

    GST_DEBUG ("y %3d fc %10.6f clarity %d br %d br1 %d br2 %d", y, fc, clarity,
        br, br1, br2);

    /* draw a star */
    off = (y * w) + x;
    c = colors[(br1 >> 4) | (br2 & 0xf0)];
    add_pixel (&vdata[off], c);
    if ((x > (sl - 1)) && (x < (w - sl)) && (y > (sl - 1)) && (y < (h - sl))) {
      for (i = 1; br1 || br2; i++, br1 = shade[br1], br2 = shade[br2]) {
        c = colors[(br1 >> 4) + (br2 & 0xf0)];
        add_pixel (&vdata[off - i], c);
        add_pixel (&vdata[off + i], c);
        add_pixel (&vdata[off - i * w], c);
        add_pixel (&vdata[off + i * w], c);
      }
    } else {
      for (i = 1; br1 || br2; i++, br1 = shade[br1], br2 = shade[br2]) {
        c = colors[(br1 >> 4) | (br2 & 0xf0)];
        if (x - i > 0)
          add_pixel (&vdata[off - i], c);
        if (x + i < (w - 1))
          add_pixel (&vdata[off + i], c);
        if (y - i > 0)
          add_pixel (&vdata[off - i * w], c);
        if (y + i < (h - 1))
          add_pixel (&vdata[off + i * w], c);
      }
    }
  }
  gst_buffer_unmap (audio, &amap);

  return TRUE;
}
