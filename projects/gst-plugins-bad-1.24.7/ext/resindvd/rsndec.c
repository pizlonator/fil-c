/* GStreamer
 * Copyright (C) <2009> Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <string.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

#include "rsndec.h"

GST_DEBUG_CATEGORY_STATIC (rsn_dec_debug);
#define GST_CAT_DEFAULT rsn_dec_debug

static GstStateChangeReturn rsn_dec_change_state (GstElement * element,
    GstStateChange transition);

static void rsn_dec_dispose (GObject * gobj);
static void cleanup_child (RsnDec * self);

static GstBinClass *rsn_dec_parent_class = NULL;

static void
rsn_dec_class_init (RsnDecClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (rsn_dec_debug, "rsndec",
      0, "Resin DVD stream decoder");

  rsn_dec_parent_class = (GstBinClass *) g_type_class_peek_parent (klass);
  object_class->dispose = rsn_dec_dispose;

  element_class->change_state = GST_DEBUG_FUNCPTR (rsn_dec_change_state);
}

static gboolean
rsn_dec_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  RsnDec *self = RSN_DEC (parent);
  gboolean ret = TRUE;
  const GstStructure *s = gst_event_get_structure (event);
  const gchar *name = (s ? gst_structure_get_name (s) : NULL);

  if (name && g_str_equal (name, "application/x-gst-dvd"))
    ret = gst_pad_push_event (GST_PAD_CAST (self->srcpad), event);
  else
    ret = self->sink_event_func (pad, parent, event);

  return ret;
}

static void
rsn_dec_init (RsnDec * self, RsnDecClass * klass)
{
  GstPadTemplate *templ;

  templ =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_assert (templ != NULL);
  self->sinkpad =
      GST_GHOST_PAD_CAST (gst_ghost_pad_new_no_target_from_template ("sink",
          templ));
  self->sink_event_func = GST_PAD_EVENTFUNC (self->sinkpad);
  gst_pad_set_event_function (GST_PAD_CAST (self->sinkpad),
      GST_DEBUG_FUNCPTR (rsn_dec_sink_event));

  templ = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_assert (templ != NULL);
  self->srcpad =
      GST_GHOST_PAD_CAST (gst_ghost_pad_new_no_target_from_template ("src",
          templ));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD_CAST (self->sinkpad));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD_CAST (self->srcpad));
}

static void
rsn_dec_dispose (GObject * object)
{
  RsnDec *self = (RsnDec *) object;
  cleanup_child (self);

  G_OBJECT_CLASS (rsn_dec_parent_class)->dispose (object);
}

static gboolean
rsn_dec_set_child (RsnDec * self, GstElement * new_child)
{
  GstPad *child_pad;
  if (self->current_decoder) {
    gst_ghost_pad_set_target (self->srcpad, NULL);
    gst_ghost_pad_set_target (self->sinkpad, NULL);
    gst_bin_remove ((GstBin *) self, self->current_decoder);
    self->current_decoder = NULL;
  }

  if (new_child == NULL)
    return TRUE;

  if (!gst_bin_add ((GstBin *) self, new_child))
    return FALSE;

  child_pad = gst_element_get_static_pad (new_child, "sink");
  if (child_pad == NULL) {
    return FALSE;
  }
  gst_ghost_pad_set_target (self->sinkpad, child_pad);
  gst_object_unref (child_pad);

  child_pad = gst_element_get_static_pad (new_child, "src");
  if (child_pad == NULL) {
    return FALSE;
  }
  gst_ghost_pad_set_target (self->srcpad, child_pad);
  gst_object_unref (child_pad);

  GST_DEBUG_OBJECT (self, "Add child %" GST_PTR_FORMAT, new_child);
  self->current_decoder = new_child;

  gst_element_sync_state_with_parent (new_child);

  return TRUE;
}

static void
cleanup_child (RsnDec * self)
{
  GST_DEBUG_OBJECT (self, "Removing child element");
  (void) rsn_dec_set_child (self, NULL);
}

typedef struct
{
  GstCaps *desired_caps;
  GstCaps *decoder_caps;
} RsnDecFactoryFilterCtx;

static gboolean
rsndec_factory_filter (GstPluginFeature * feature, RsnDecFactoryFilterCtx * ctx)
{
  GstElementFactory *factory;
  guint rank;
  const gchar *klass;
  const GList *templates;
  GList *walk;
  gboolean can_sink = FALSE;

  /* we only care about element factories */
  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;

  factory = GST_ELEMENT_FACTORY (feature);

  klass =
      gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);
  /* only decoders can play */
  if (strstr (klass, "Decoder") == NULL)
    return FALSE;

  /* only select elements with autoplugging rank */
  rank = gst_plugin_feature_get_rank (feature);
  if (rank < GST_RANK_MARGINAL)
    return FALSE;

  /* See if the element has a sink pad that can possibly sink this caps */

  /* get the templates from the element factory */
  templates = gst_element_factory_get_static_pad_templates (factory);
  for (walk = (GList *) templates; walk && !can_sink; walk = g_list_next (walk)) {
    GstStaticPadTemplate *templ = walk->data;

    /* we only care about the sink templates */
    if (templ->direction == GST_PAD_SINK) {
      GstCaps *intersect;
      GstCaps *tmpl_caps;

      /* try to intersect the caps with the caps of the template */
      tmpl_caps = gst_static_caps_get (&templ->static_caps);

      intersect = gst_caps_intersect (ctx->desired_caps, tmpl_caps);
      gst_caps_unref (tmpl_caps);

      /* check if the intersection is empty */
      if (!gst_caps_is_empty (intersect)) {
        /* non empty intersection, we can use this element */
        can_sink = TRUE;
        ctx->decoder_caps = gst_caps_merge (ctx->decoder_caps, intersect);
      } else
        gst_caps_unref (intersect);
    }
  }

  if (can_sink) {
    GST_DEBUG ("Found decoder element %s (%s)",
        gst_element_factory_get_metadata (factory,
            GST_ELEMENT_METADATA_LONGNAME),
        gst_plugin_feature_get_name (feature));
  }

  return can_sink;
}

static gint
sort_by_ranks (GstPluginFeature * f1, GstPluginFeature * f2)
{
  gint diff;
  const gchar *rname1, *rname2;

  diff = gst_plugin_feature_get_rank (f2) - gst_plugin_feature_get_rank (f1);
  if (diff != 0)
    return diff;

  rname1 = gst_plugin_feature_get_name (f1);
  rname2 = gst_plugin_feature_get_name (f2);

  diff = strcmp (rname2, rname1);

  return diff;
}

static gpointer
_get_decoder_factories (gpointer arg)
{
  GstElementClass *klass = arg;
  GList *factories;
  GstPadTemplate *templ = gst_element_class_get_pad_template (klass,
      "sink");
  RsnDecFactoryFilterCtx ctx = { NULL, };
  GstCaps *raw;
  gboolean raw_audio;
  GstRegistry *registry = gst_registry_get ();

  ctx.desired_caps = gst_pad_template_get_caps (templ);

  raw =
      gst_caps_from_string
      ("audio/x-raw,format=(string){ F32LE, F32BE, F64LE, F64BE }");
  raw_audio = gst_caps_can_intersect (raw, ctx.desired_caps);
  if (raw_audio) {
    GstCaps *sub = gst_caps_subtract (ctx.desired_caps, raw);
    ctx.desired_caps = sub;
  } else {
    gst_caps_ref (ctx.desired_caps);
  }
  gst_caps_unref (raw);

  /* Set decoder caps to empty. Will be filled by the factory_filter */
  ctx.decoder_caps = gst_caps_new_empty ();
  GST_DEBUG ("Finding factories for caps: %" GST_PTR_FORMAT, ctx.desired_caps);

  factories = gst_registry_feature_filter (registry,
      (GstPluginFeatureFilter) rsndec_factory_filter, FALSE, &ctx);

  /* If these are audio caps, we add audioconvert, which is not a decoder,
     but allows raw audio to go through relatively unmolested - this will
     come handy when we have to send placeholder silence to allow preroll
     for those DVDs which have titles with no audio track. */
  if (raw_audio) {
    GstPluginFeature *feature;
    GST_DEBUG ("These are audio caps, adding audioconvert");
    feature =
        gst_registry_find_feature (registry, "audioconvert",
        GST_TYPE_ELEMENT_FACTORY);
    if (feature) {
      factories = g_list_append (factories, feature);
    } else {
      GST_WARNING ("Could not find feature audioconvert");
    }
  }

  factories = g_list_sort (factories, (GCompareFunc) sort_by_ranks);

  GST_DEBUG ("Available decoder caps %" GST_PTR_FORMAT, ctx.decoder_caps);
  gst_caps_unref (ctx.decoder_caps);
  gst_caps_unref (ctx.desired_caps);

  return factories;
}

static GstStateChangeReturn
rsn_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  RsnDec *self = RSN_DEC (element);
  RsnDecClass *klass = RSN_DEC_GET_CLASS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GstElement *new_child;
      const GList *decoder_factories;

      new_child = gst_element_factory_make ("autoconvert", NULL);
      decoder_factories = klass->get_decoder_factories (klass);
      g_object_set (G_OBJECT (new_child), "factories", decoder_factories, NULL);
      if (new_child == NULL || !rsn_dec_set_child (self, new_child))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  ret =
      GST_ELEMENT_CLASS (rsn_dec_parent_class)->change_state (element,
      transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      cleanup_child (self);
      break;
    default:
      break;
  }

  return ret;
}

GType
rsn_dec_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo type_info = {
      sizeof (RsnDecClass),
      NULL,
      NULL,
      (GClassInitFunc) rsn_dec_class_init,
      NULL,
      NULL,
      sizeof (RsnDec),
      0,
      (GInstanceInitFunc) rsn_dec_init,
    };

    _type = g_type_register_static (GST_TYPE_BIN,
        "RsnDec", &type_info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/* Audio decoder subclass */
static GstStaticPadTemplate audio_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg,mpegversion=(int)1;"
        "audio/x-private1-lpcm;"
        "audio/x-private1-ac3;" "audio/ac3;" "audio/x-ac3;"
        "audio/x-private1-dts; audio/x-raw,format=(string)"
        GST_AUDIO_FORMATS_ALL)
    );

static GstStaticPadTemplate audio_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL))
    );

G_DEFINE_TYPE (RsnAudioDec, rsn_audiodec, RSN_TYPE_DEC);

static const GList *
rsn_audiodec_get_decoder_factories (RsnDecClass * klass)
{
  static GOnce gonce = G_ONCE_INIT;

  g_once (&gonce, _get_decoder_factories, klass);

  return (const GList *) gonce.retval;
}

static void
rsn_audiodec_class_init (RsnAudioDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  RsnDecClass *dec_class = RSN_DEC_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &audio_sink_template);

  gst_element_class_set_static_metadata (element_class, "RsnAudioDec",
      "Audio/Decoder",
      "Resin DVD audio stream decoder", "Jan Schmidt <thaytan@noraisin.net>");

  dec_class->get_decoder_factories = rsn_audiodec_get_decoder_factories;
}

static void
rsn_audiodec_init (RsnAudioDec * self)
{
}

/* Video decoder subclass */
static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) [ 1, 2 ], " "systemstream = (bool) FALSE")
    );

static GstStaticPadTemplate video_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

G_DEFINE_TYPE (RsnVideoDec, rsn_videodec, RSN_TYPE_DEC);

static const GList *
rsn_videodec_get_decoder_factories (RsnDecClass * klass)
{
  static GOnce gonce = G_ONCE_INIT;

  g_once (&gonce, _get_decoder_factories, klass);

  return (const GList *) gonce.retval;
}

static void
rsn_videodec_class_init (RsnAudioDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  RsnDecClass *dec_class = RSN_DEC_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &video_sink_template);

  gst_element_class_set_static_metadata (element_class, "RsnVideoDec",
      "Video/Decoder",
      "Resin DVD video stream decoder", "Jan Schmidt <thaytan@noraisin.net>");

  dec_class->get_decoder_factories = rsn_videodec_get_decoder_factories;
}

static void
rsn_videodec_init (RsnVideoDec * self)
{
}
