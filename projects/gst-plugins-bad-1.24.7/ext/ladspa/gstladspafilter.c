/* GStreamer LADSPA filter category
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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

#include "gstladspafilter.h"
#include "gstladspa.h"
#include "gstladspautils.h"

GST_DEBUG_CATEGORY_EXTERN (ladspa_debug);
#define GST_CAT_DEFAULT ladspa_debug

#define GST_LADSPA_FILTER_CLASS_TAGS "Filter/Effect/Audio/LADSPA"

static GstLADSPAFilterClass *gst_ladspa_filter_type_parent_class = NULL;

/*
 * Assumes only same format (base of AudioFilter), not same channels.
 */
void
gst_my_audio_filter_class_add_pad_templates (GstAudioFilterClass * audio_class,
    GstCaps * srccaps, GstCaps * sinkcaps)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (audio_class);
  GstPadTemplate *pad_template;

  g_return_if_fail (GST_IS_CAPS (srccaps) && GST_IS_CAPS (sinkcaps));

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SRC_NAME, GST_PAD_SRC,
      GST_PAD_ALWAYS, srccaps);
  gst_element_class_add_pad_template (elem_class, pad_template);

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SINK_NAME, GST_PAD_SINK,
      GST_PAD_ALWAYS, sinkcaps);
  gst_element_class_add_pad_template (elem_class, pad_template);
}

static GstCaps *
gst_ladspa_filter_type_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *structure;
  gint rate;

  structure = gst_caps_get_structure (caps, 0);
  if (G_UNLIKELY (!gst_structure_get_int (structure, "rate", &rate)))
    return othercaps;

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);
  structure = gst_caps_get_structure (othercaps, 0);

  gst_structure_fixate_field_nearest_int (structure, "rate", rate);

  return othercaps;
}

static GstCaps *
gst_ladspa_filter_type_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *srccaps, *sinkcaps;
  GstCaps *ret = NULL;

  srccaps = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SRC_PAD (base));
  sinkcaps = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SINK_PAD (base));

  switch (direction) {
    case GST_PAD_SINK:
      if (gst_caps_can_intersect (caps, sinkcaps))
        ret = gst_caps_copy (srccaps);
      else
        ret = gst_caps_new_empty ();
      break;
    case GST_PAD_SRC:
      if (gst_caps_can_intersect (caps, srccaps))
        ret = gst_caps_copy (sinkcaps);
      else
        ret = gst_caps_new_empty ();
      break;
    default:
      g_assert_not_reached ();
  }

  GST_DEBUG_OBJECT (base, "transformed %" GST_PTR_FORMAT, ret);

  if (filter) {
    GstCaps *intersection;

    GST_DEBUG_OBJECT (base, "Using filter caps %" GST_PTR_FORMAT, filter);

    intersection =
        gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;

    GST_DEBUG_OBJECT (base, "Intersection %" GST_PTR_FORMAT, ret);
  }

  gst_caps_unref (srccaps);
  gst_caps_unref (sinkcaps);

  return ret;
}

static GstFlowReturn
gst_ladspa_filter_type_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (base);
  GstLADSPAFilterClass *ladspa_class = GST_LADSPA_FILTER_GET_CLASS (ladspa);
  guint samples;

  samples =
      gst_buffer_get_size (inbuf) / sizeof (LADSPA_Data) /
      ladspa_class->ladspa.count.audio.in;

  if (!gst_base_transform_is_in_place (base)) {
    *outbuf =
        gst_buffer_new_allocate (NULL,
        samples * sizeof (LADSPA_Data) * ladspa_class->ladspa.count.audio.out,
        NULL);
    *outbuf = gst_buffer_make_writable (*outbuf);
    return GST_FLOW_OK;
  } else {
    return
        GST_BASE_TRANSFORM_CLASS
        (gst_ladspa_filter_type_parent_class)->prepare_output_buffer (base,
        inbuf, outbuf);
  }
}

static gboolean
gst_ladspa_filter_type_setup (GstAudioFilter * audio, const GstAudioInfo * info)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (audio);

  return gst_ladspa_setup (&ladspa->ladspa, GST_AUDIO_INFO_RATE (info));
}

static gboolean
gst_ladspa_filter_type_cleanup (GstBaseTransform * base)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (base);

  return gst_ladspa_cleanup (&ladspa->ladspa);
}

static GstFlowReturn
gst_ladspa_filter_type_transform_ip (GstBaseTransform * base, GstBuffer * buf)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (base);
  GstMapInfo map;
  guint samples;

  gst_buffer_map (buf, &map, GST_MAP_READWRITE);
  samples =
      map.size / sizeof (LADSPA_Data) / ladspa->ladspa.klass->count.audio.in;
  gst_ladspa_transform (&ladspa->ladspa, map.data, samples, map.data);
  gst_buffer_unmap (buf, &map);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_ladspa_filter_type_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (base);
  GstMapInfo inmap, outmap;
  guint samples;

  gst_object_sync_values (GST_OBJECT (ladspa), GST_BUFFER_TIMESTAMP (inbuf));

  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE);
  samples =
      inmap.size / sizeof (LADSPA_Data) / ladspa->ladspa.klass->count.audio.in;
  gst_ladspa_transform (&ladspa->ladspa, outmap.data, samples, inmap.data);
  gst_buffer_unmap (outbuf, &outmap);
  gst_buffer_unmap (inbuf, &inmap);

  return GST_FLOW_OK;
}

static void
gst_ladspa_filter_type_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (object);

  gst_ladspa_object_set_property (&ladspa->ladspa, object, prop_id, value,
      pspec);
}

static void
gst_ladspa_filter_type_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (object);

  gst_ladspa_object_get_property (&ladspa->ladspa, object, prop_id, value,
      pspec);
}

static void
gst_ladspa_filter_type_init (GstLADSPAFilter * ladspa, LADSPA_Descriptor * desc)
{
  GstBaseTransform *base = GST_BASE_TRANSFORM (ladspa);
  GstLADSPAFilterClass *ladspa_class = GST_LADSPA_FILTER_GET_CLASS (ladspa);

  gst_ladspa_init (&ladspa->ladspa, &ladspa_class->ladspa);

  /* even if channels are different LADSPA still maintains same samples */
  gst_base_transform_set_in_place (base,
      ladspa_class->ladspa.count.audio.in ==
      ladspa_class->ladspa.count.audio.out
      && !LADSPA_IS_INPLACE_BROKEN (ladspa_class->ladspa.descriptor->
          Properties));

}

static void
gst_ladspa_filter_type_dispose (GObject * object)
{
  GstBaseTransform *base = GST_BASE_TRANSFORM (object);

  gst_ladspa_filter_type_cleanup (base);

  G_OBJECT_CLASS (gst_ladspa_filter_type_parent_class)->dispose (object);
}

static void
gst_ladspa_filter_type_finalize (GObject * object)
{
  GstLADSPAFilter *ladspa = GST_LADSPA_FILTER (object);

  gst_ladspa_finalize (&ladspa->ladspa);

  G_OBJECT_CLASS (gst_ladspa_filter_type_parent_class)->finalize (object);
}

/*
 * It is okay for plugins to 'leak' a one-time allocation. This will be freed when
 * the application exits. When the plugins are scanned for the first time, this is
 * done from a separate process to not impose the memory overhead on the calling
 * application (among other reasons). Hence no need for class_finalize.
 */
static void
gst_ladspa_filter_type_base_init (GstLADSPAFilterClass * ladspa_class)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (ladspa_class);
  GstAudioFilterClass *audio_class = GST_AUDIO_FILTER_CLASS (ladspa_class);

  gst_ladspa_class_init (&ladspa_class->ladspa,
      G_TYPE_FROM_CLASS (ladspa_class));

  gst_ladspa_element_class_set_metadata (&ladspa_class->ladspa, elem_class,
      GST_LADSPA_FILTER_CLASS_TAGS);
  gst_ladspa_filter_type_class_add_pad_templates (&ladspa_class->ladspa,
      audio_class);

  gst_type_mark_as_plugin_api (GST_TYPE_LADSPA_FILTER, 0);
}

static void
gst_ladspa_filter_type_base_finalize (GstLADSPAFilterClass * ladspa_class)
{
  gst_ladspa_class_finalize (&ladspa_class->ladspa);
}

static void
gst_ladspa_filter_type_class_init (GstLADSPAFilterClass * ladspa_class,
    LADSPA_Descriptor * desc)
{
  GObjectClass *object_class = G_OBJECT_CLASS (ladspa_class);
  GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS (ladspa_class);
  GstAudioFilterClass *audio_class = GST_AUDIO_FILTER_CLASS (ladspa_class);

  GST_DEBUG ("LADSPA filter class %p", ladspa_class);

  gst_ladspa_filter_type_parent_class = g_type_class_peek_parent (ladspa_class);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_get_property);

  base_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_fixate_caps);
  base_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_transform_caps);
  base_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_prepare_output_buffer);
  base_class->transform = GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_transform);
  base_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_transform_ip);

  audio_class->setup = GST_DEBUG_FUNCPTR (gst_ladspa_filter_type_setup);

  gst_ladspa_object_class_install_properties (&ladspa_class->ladspa,
      object_class, 1);
}

G_DEFINE_ABSTRACT_TYPE (GstLADSPAFilter, gst_ladspa_filter,
    GST_TYPE_AUDIO_FILTER);

static void
gst_ladspa_filter_init (GstLADSPAFilter * ladspa)
{
}

static void
gst_ladspa_filter_class_init (GstLADSPAFilterClass * ladspa_class)
{
}

/* 
 * Construct the type.
 */
void
ladspa_register_filter_element (GstPlugin * plugin, GstStructure * ladspa_meta)
{
  GTypeInfo info = {
    sizeof (GstLADSPAFilterClass),
    (GBaseInitFunc) gst_ladspa_filter_type_base_init,
    (GBaseFinalizeFunc) gst_ladspa_filter_type_base_finalize,
    (GClassInitFunc) gst_ladspa_filter_type_class_init,
    NULL,
    NULL,
    sizeof (GstLADSPAFilter),
    0,
    (GInstanceInitFunc) gst_ladspa_filter_type_init,
    NULL
  };
  ladspa_register_element (plugin, GST_TYPE_LADSPA_FILTER, &info, ladspa_meta);
}
