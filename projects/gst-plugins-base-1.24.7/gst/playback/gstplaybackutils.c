/* Copyright (C) <2014> Intel Corporation
 * Copyright (C) <2014> Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#include <gst/gst.h>
#include "gstplaybackutils.h"

static GstStaticCaps raw_audio_caps = GST_STATIC_CAPS ("audio/x-raw(ANY)");
static GstStaticCaps raw_video_caps = GST_STATIC_CAPS ("video/x-raw(ANY)");

/* unref the caps after usage */
static GstCaps *
get_template_caps (GstElementFactory * factory, GstPadDirection direction)
{
  const GList *templates;
  GstStaticPadTemplate *templ = NULL;
  GList *walk;

  templates = gst_element_factory_get_static_pad_templates (factory);
  for (walk = (GList *) templates; walk; walk = g_list_next (walk)) {
    templ = walk->data;
    if (templ->direction == direction)
      break;
  }
  if (templ)
    return gst_static_caps_get (&templ->static_caps);
  else
    return NULL;
}

static gboolean
is_included (GList * list, GstCapsFeatures * cf)
{
  for (; list; list = list->next) {
    if (gst_caps_features_is_equal ((GstCapsFeatures *) list->data, cf))
      return TRUE;
  }
  return FALSE;
}

/* compute the number of common caps features */
guint
gst_playback_utils_get_n_common_capsfeatures (GstElementFactory * fact1,
    GstElementFactory * fact2, GstPlayFlags flags, gboolean isaudioelement)
{
  GstCaps *fact1_tmpl_caps, *fact2_tmpl_caps;
  GstCapsFeatures *fact1_features, *fact2_features;
  GstStructure *fact1_struct, *fact2_struct;
  GList *cf_list = NULL;
  guint fact1_caps_size, fact2_caps_size;
  guint i, j, n_common_cf = 0;
  GstCaps *raw_caps =
      (isaudioelement) ? gst_static_caps_get (&raw_audio_caps) :
      gst_static_caps_get (&raw_video_caps);
  GstStructure *raw_struct = gst_caps_get_structure (raw_caps, 0);
  gboolean native_raw =
      (isaudioelement ? !!(flags & GST_PLAY_FLAG_NATIVE_AUDIO) : !!(flags &
          GST_PLAY_FLAG_NATIVE_VIDEO));

  fact1_tmpl_caps = get_template_caps (fact1, GST_PAD_SRC);
  fact2_tmpl_caps = get_template_caps (fact2, GST_PAD_SINK);
  if (!fact1_tmpl_caps || !fact2_tmpl_caps) {
    GST_ERROR ("Failed to get template caps from decoder or sink");
    if (fact1_tmpl_caps)
      gst_caps_unref (fact1_tmpl_caps);
    else if (fact2_tmpl_caps)
      gst_caps_unref (fact2_tmpl_caps);
    return 0;
  }

  fact1_caps_size = gst_caps_get_size (fact1_tmpl_caps);
  fact2_caps_size = gst_caps_get_size (fact2_tmpl_caps);

  for (i = 0; i < fact1_caps_size; i++) {
    fact1_features =
        gst_caps_get_features ((const GstCaps *) fact1_tmpl_caps, i);
    if (gst_caps_features_is_any (fact1_features))
      continue;
    fact1_struct =
        gst_caps_get_structure ((const GstCaps *) fact1_tmpl_caps, i);
    for (j = 0; j < fact2_caps_size; j++) {

      fact2_features =
          gst_caps_get_features ((const GstCaps *) fact2_tmpl_caps, j);
      if (gst_caps_features_is_any (fact2_features))
        continue;
      fact2_struct =
          gst_caps_get_structure ((const GstCaps *) fact2_tmpl_caps, j);

      /* A common caps feature is given if the caps features are equal
       * and the structures can intersect. If the NATIVE_AUDIO/NATIVE_VIDEO
       * flags are not set we also allow if both structures are raw caps with
       * system memory caps features, because in that case we have converters in
       * place.
       */
      if (gst_caps_features_is_equal (fact1_features, fact2_features) &&
          (gst_structure_can_intersect (fact1_struct, fact2_struct) ||
              (!native_raw
                  && gst_caps_features_is_equal (fact1_features,
                      GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY)
                  && gst_structure_can_intersect (raw_struct, fact1_struct)
                  && gst_structure_can_intersect (raw_struct, fact2_struct)))
          && !is_included (cf_list, fact2_features)) {
        cf_list = g_list_prepend (cf_list, fact2_features);
        n_common_cf++;
      }
    }
  }
  if (cf_list)
    g_list_free (cf_list);

  gst_caps_unref (fact1_tmpl_caps);
  gst_caps_unref (fact2_tmpl_caps);

  return n_common_cf;
}

gint
gst_playback_utils_compare_factories_func (gconstpointer p1, gconstpointer p2)
{
  GstPluginFeature *f1, *f2;
  gboolean is_parser1, is_parser2;

  f1 = (GstPluginFeature *) p1;
  f2 = (GstPluginFeature *) p2;

  is_parser1 = gst_element_factory_list_is_type (GST_ELEMENT_FACTORY_CAST (f1),
      GST_ELEMENT_FACTORY_TYPE_PARSER);
  is_parser2 = gst_element_factory_list_is_type (GST_ELEMENT_FACTORY_CAST (f2),
      GST_ELEMENT_FACTORY_TYPE_PARSER);


  /* We want all parsers first as we always want to plug parsers
   * before decoders */
  if (is_parser1 && !is_parser2)
    return -1;
  else if (!is_parser1 && is_parser2)
    return 1;

  /* And if it's a both a parser we first sort by rank
   * and then by factory name */
  return gst_plugin_feature_rank_compare_func (p1, p2);
}
