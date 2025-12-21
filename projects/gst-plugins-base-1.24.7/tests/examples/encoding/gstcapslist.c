/* GStreamer
 * Copyright (C) <2010> Edward Hervey <edward.hervey@collabora.co.uk>
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

#include "gstcapslist.h"

/*
 * Caps listing convenience functions
 */

static gboolean
remove_range_foreach (GQuark field_id, const GValue * value, GstStructure * st)
{
  GType ftype = G_VALUE_TYPE (value);
  /* const gchar *fname; */

  if (ftype == GST_TYPE_INT_RANGE || ftype == GST_TYPE_DOUBLE_RANGE ||
      ftype == GST_TYPE_FRACTION_RANGE) {
    gst_structure_remove_field (st, g_quark_to_string (field_id));
    return FALSE;
  }

  /* fname = g_quark_to_string (field_id); */
  /* if (strstr (fname, "framerate") || strstr (fname, "pixel-aspect-ratio") || */
  /*     strstr (fname, "rate")) { */
  /*   gst_structure_remove_field (st, g_quark_to_string (field_id)); */
  /*   return FALSE; */
  /* } */

  return TRUE;
}

static void
clear_caps (GstCaps * caps, GstCaps * rescaps)
{
  GstCaps *res;
  GstStructure *st;
  guint i;

  res = gst_caps_make_writable (caps);

  GST_DEBUG ("incoming caps %" GST_PTR_FORMAT, res);

  /* Remove width/height/framerate/depth/width fields */
  for (i = gst_caps_get_size (res); i; i--) {
    st = gst_caps_get_structure (res, i - 1);

    /* Remove range fields */
    while (!gst_structure_foreach (st,
            (GstStructureForeachFunc) remove_range_foreach, st));
  }

  GST_DEBUG ("stripped %" GST_PTR_FORMAT, res);

  /* And append to list without duplicates */
  while ((st = gst_caps_steal_structure (res, 0))) {
    /* Skip fake codecs/containers */
    if (gst_structure_has_name (st, "audio/x-raw") ||
        gst_structure_has_name (st, "video/x-raw") ||
        gst_structure_has_name (st, "unknown/unknown")) {
      gst_structure_free (st);
      continue;
    }

    gst_caps_append_structure (rescaps, st);
  }

  gst_caps_unref (res);
}

static GstCaps *
get_all_caps (GList * elements, GstPadDirection direction)
{
  GstCaps *res;
  GList *tmp;

  res = gst_caps_new_empty ();

  for (tmp = elements; tmp; tmp = tmp->next) {
    GstElementFactory *factory = (GstElementFactory *) tmp->data;
    const GList *templates;
    GList *walk;

    templates = gst_element_factory_get_static_pad_templates (factory);
    for (walk = (GList *) templates; walk; walk = g_list_next (walk)) {
      GstStaticPadTemplate *templ = walk->data;
      if (templ->direction == direction)
        clear_caps (gst_static_caps_get (&templ->static_caps), res);
    }
  }

  res = gst_caps_normalize (res);

  return res;
}

/**
 * gst_caps_list_container_formats:
 * @minrank: The minimum #GstRank
 *
 * Returns a #GstCaps corresponding to all the container formats
 * one can mux to on this system.
 *
 * Returns: A #GstCaps. Unref with %gst_caps_unref when done with it.
 */
GstCaps *
gst_caps_list_container_formats (GstRank minrank)
{
  GstCaps *res;
  GList *muxers;

  muxers =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_MUXER,
      minrank);
  res = get_all_caps (muxers, GST_PAD_SRC);
  gst_plugin_feature_list_free (muxers);

  return res;
}

static GstCaps *
gst_caps_list_encoding_formats (GstRank minrank)
{
  GstCaps *res;
  GList *encoders;

  encoders =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER,
      minrank);
  res = get_all_caps (encoders, GST_PAD_SRC);
  gst_plugin_feature_list_free (encoders);

  return res;
}

/**
 * gst_caps_list_video_encoding_formats:
 * @minrank: The minimum #GstRank
 *
 * Returns a #GstCaps corresponding to all the video or image formats one
 * can encode to on this system.
 *
 * Returns: A #GstCaps. Unref with %gst_caps_unref when done with it.
 */
GstCaps *
gst_caps_list_video_encoding_formats (GstRank minrank)
{
  GstCaps *res;
  GList *encoders;

  encoders =
      gst_element_factory_list_get_elements
      (GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER, minrank);
  res = get_all_caps (encoders, GST_PAD_SRC);
  gst_plugin_feature_list_free (encoders);

  return res;
}


/**
 * gst_caps_list_audio_encoding_formats:
 * @minrank: The minimum #GstRank
 *
 * Returns a #GstCaps corresponding to all the audio formats one
 * can encode to on this system.
 *
 * Returns: A  #GstCaps. Unref with %gst_caps_unref when done with it.
 */
GstCaps *
gst_caps_list_audio_encoding_formats (GstRank minrank)
{
  GstCaps *res;
  GList *encoders;

  encoders =
      gst_element_factory_list_get_elements
      (GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, minrank);
  res = get_all_caps (encoders, GST_PAD_SRC);
  gst_plugin_feature_list_free (encoders);

  return res;
}

/**
 * gst_caps_list_compatible_codecs:
 * @containerformat: A #GstCaps corresponding to a container format
 * @codecformats: An optional #GstCaps of codec formats
 * @muxers: An optional #GList of muxer #GstElementFactory.
 *
 * Returns an array of #GstCaps corresponding to the audio/video/text formats
 * one can encode to and that can be muxed in the provided @containerformat.
 *
 * If specified, only the #GstCaps contained in @codecformats will be checked
 * against, else all compatible audio/video formats will be returned.
 *
 * If specified, only the #GstElementFactory contained in @muxers will be checked,
 * else all available muxers on the system will be checked.
 *
 * Returns: A #GstCaps containing all compatible formats. Unref with %gst_caps_unref
 * when done.
 */
GstCaps *
gst_caps_list_compatible_codecs (const GstCaps * containerformat,
    GstCaps * codecformats, GList * muxers)
{
  const GList *templates;
  GstElementFactory *factory;
  GList *walk;
  GstCaps *res = NULL;
  GstCaps *tmpcaps;
  GList *tmp;
  gboolean hadmuxers = (muxers != NULL);
  gboolean hadcodecs = (codecformats != NULL);

  GST_DEBUG ("containerformat: %" GST_PTR_FORMAT, containerformat);
  GST_DEBUG ("codecformats: %" GST_PTR_FORMAT, codecformats);

  if (!hadmuxers)
    muxers =
        gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_MUXER,
        GST_RANK_NONE);
  if (!hadcodecs)
    codecformats = gst_caps_list_encoding_formats (GST_RANK_NONE);

  /* Get the highest rank muxer matching containerformat */
  tmp =
      gst_element_factory_list_filter (muxers, containerformat, GST_PAD_SRC,
      TRUE);
  if (G_UNLIKELY (tmp == NULL))
    goto beach;

  factory = (GstElementFactory *) tmp->data;

  GST_DEBUG ("Trying with factory %s",
      gst_element_factory_get_metadata (factory,
          GST_ELEMENT_METADATA_LONGNAME));

  /* Match all muxer sink pad templates against the available codec formats */
  templates = gst_element_factory_get_static_pad_templates (factory);
  gst_plugin_feature_list_free (tmp);

  tmpcaps = gst_caps_new_empty ();

  for (walk = (GList *) templates; walk; walk = walk->next) {
    GstStaticPadTemplate *templ = walk->data;

    if (templ->direction == GST_PAD_SINK) {
      GstCaps *templ_caps;

      templ_caps = gst_static_caps_get (&templ->static_caps);
      gst_caps_append (tmpcaps, gst_caps_copy (templ_caps));
    }
  }

  res = gst_caps_intersect (tmpcaps, codecformats);
  gst_caps_unref (tmpcaps);

beach:
  if (!hadmuxers)
    gst_plugin_feature_list_free (muxers);
  if (!hadcodecs)
    gst_caps_unref (codecformats);

  res = gst_caps_normalize (res);

  return res;
}
