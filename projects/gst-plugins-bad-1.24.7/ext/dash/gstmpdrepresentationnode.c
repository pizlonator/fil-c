/* GStreamer
 *
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */
#include "gstmpdrepresentationnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDRepresentationNode, gst_mpd_representation_node,
    GST_TYPE_MPD_REPRESENTATION_BASE_NODE);

enum
{
  PROP_MPD_REPRESENTATION_0,
  PROP_MPD_REPRESENTATION_ID,
  PROP_MPD_REPRESENTATION_BANDWIDTH,
  PROP_MPD_REPRESENTATION_QUALITY_RANKING,
};

/* GObject VMethods */

static void
gst_mpd_representation_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDRepresentationNode *self = GST_MPD_REPRESENTATION_NODE (object);
  switch (prop_id) {
    case PROP_MPD_REPRESENTATION_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_MPD_REPRESENTATION_BANDWIDTH:
      self->bandwidth = g_value_get_uint (value);
      break;
    case PROP_MPD_REPRESENTATION_QUALITY_RANKING:
      self->qualityRanking = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_representation_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDRepresentationNode *self = GST_MPD_REPRESENTATION_NODE (object);
  switch (prop_id) {
    case PROP_MPD_REPRESENTATION_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_MPD_REPRESENTATION_BANDWIDTH:
      g_value_set_uint (value, self->bandwidth);
      break;
    case PROP_MPD_REPRESENTATION_QUALITY_RANKING:
      g_value_set_uint (value, self->qualityRanking);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_representation_node_finalize (GObject * object)
{
  GstMPDRepresentationNode *self = GST_MPD_REPRESENTATION_NODE (object);

  if (self->id)
    xmlFree (self->id);
  g_strfreev (self->dependencyId);
  g_strfreev (self->mediaStreamStructureId);
  g_list_free_full (self->SubRepresentations,
      (GDestroyNotify) gst_mpd_sub_representation_node_free);
  gst_mpd_segment_base_node_free (self->SegmentBase);
  gst_mpd_segment_template_node_free (self->SegmentTemplate);
  gst_mpd_segment_list_node_free (self->SegmentList);
  g_list_free_full (self->BaseURLs, (GDestroyNotify) gst_mpd_baseurl_node_free);

  G_OBJECT_CLASS (gst_mpd_representation_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_representation_get_xml_node (GstMPDNode * node)
{
  gchar *value;
  xmlNodePtr representation_xml_node = NULL;
  GstMPDRepresentationNode *self = GST_MPD_REPRESENTATION_NODE (node);

  representation_xml_node = xmlNewNode (NULL, (xmlChar *) "Representation");

  gst_xml_helper_set_prop_string (representation_xml_node, "id", self->id);
  gst_xml_helper_set_prop_uint (representation_xml_node, "bandwidth",
      self->bandwidth);
  if (self->qualityRanking)
    gst_xml_helper_set_prop_uint (representation_xml_node, "qualityRanking",
        self->qualityRanking);


  if (self->dependencyId) {
    value = g_strjoinv (" ", self->dependencyId);
    gst_xml_helper_set_prop_string (representation_xml_node, "dependencyId",
        value);
    g_free (value);
  }
  if (self->mediaStreamStructureId) {
    value = g_strjoinv (" ", self->mediaStreamStructureId);
    gst_xml_helper_set_prop_string (representation_xml_node,
        "mediaStreamStructureId", value);
    g_free (value);
  }

  g_list_foreach (self->BaseURLs, gst_mpd_node_get_list_item,
      representation_xml_node);
  g_list_foreach (self->SubRepresentations,
      gst_mpd_representation_base_node_get_list_item, representation_xml_node);

  gst_mpd_node_add_child_node (GST_MPD_NODE (self->SegmentBase),
      representation_xml_node);
  gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
      (self->SegmentTemplate), representation_xml_node);
  gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
      (self->SegmentList), representation_xml_node);

  return representation_xml_node;
}

static void
gst_mpd_representation_node_class_init (GstMPDRepresentationNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_representation_node_finalize;
  object_class->set_property = gst_mpd_representation_node_set_property;
  object_class->get_property = gst_mpd_representation_node_get_property;

  m_klass->get_xml_node = gst_mpd_representation_get_xml_node;

  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_BANDWIDTH, g_param_spec_uint ("bandwidth",
          "bandwidth", "representation bandwidth", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_REPRESENTATION_QUALITY_RANKING,
      g_param_spec_uint ("quality-ranking", "quality ranking",
          "representation quality ranking", 0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mpd_representation_node_init (GstMPDRepresentationNode * self)
{
  self->id = NULL;
  self->bandwidth = 0;
  self->qualityRanking = 0;
  self->dependencyId = NULL;
  self->mediaStreamStructureId = NULL;
  self->BaseURLs = NULL;
  self->SubRepresentations = NULL;
  self->SegmentBase = NULL;
  self->SegmentTemplate = NULL;
  self->SegmentList = NULL;
}

GstMPDRepresentationNode *
gst_mpd_representation_node_new (void)
{
  GstMPDRepresentationNode *ret;

  ret = g_object_new (GST_TYPE_MPD_REPRESENTATION_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_representation_node_free (GstMPDRepresentationNode * self)
{
  if (self)
    gst_object_unref (self);
}
