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
#include "gstmpdadaptationsetnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDAdaptationSetNode, gst_mpd_adaptation_set_node,
    GST_TYPE_MPD_REPRESENTATION_BASE_NODE);

enum
{
  PROP_MPD_ADAPTATION_SET_0,
  PROP_MPD_ADAPTATION_SET_ID,
  PROP_MPD_ADAPTATION_SET_CONTENT_TYPE,
};

/* GObject VMethods */

static void
gst_mpd_adaptation_set_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDAdaptationSetNode *self = GST_MPD_ADAPTATION_SET_NODE (object);
  switch (prop_id) {
    case PROP_MPD_ADAPTATION_SET_ID:
      self->id = g_value_get_int (value);
      break;
    case PROP_MPD_ADAPTATION_SET_CONTENT_TYPE:
      g_free (self->contentType);
      self->contentType = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_adaptation_set_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDAdaptationSetNode *self = GST_MPD_ADAPTATION_SET_NODE (object);
  switch (prop_id) {
    case PROP_MPD_ADAPTATION_SET_ID:
      g_value_set_int (value, self->id);
      break;
    case PROP_MPD_ADAPTATION_SET_CONTENT_TYPE:
      g_value_set_string (value, self->contentType);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_adaptation_set_node_finalize (GObject * object)
{
  GstMPDAdaptationSetNode *self = GST_MPD_ADAPTATION_SET_NODE (object);

  if (self->lang)
    xmlFree (self->lang);
  if (self->contentType)
    xmlFree (self->contentType);
  g_slice_free (GstXMLRatio, self->par);
  g_slice_free (GstXMLConditionalUintType, self->segmentAlignment);
  g_slice_free (GstXMLConditionalUintType, self->subsegmentAlignment);
  g_list_free_full (self->Accessibility,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Role,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Rating,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  g_list_free_full (self->Viewpoint,
      (GDestroyNotify) gst_mpd_descriptor_type_node_free);
  gst_mpd_segment_base_node_free (self->SegmentBase);
  gst_mpd_segment_list_node_free (self->SegmentList);
  gst_mpd_segment_template_node_free (self->SegmentTemplate);
  g_list_free_full (self->BaseURLs, (GDestroyNotify) gst_mpd_baseurl_node_free);
  g_list_free_full (self->Representations,
      (GDestroyNotify) gst_mpd_representation_node_free);
  g_list_free_full (self->ContentComponents,
      (GDestroyNotify) gst_mpd_content_component_node_free);
  if (self->xlink_href)
    xmlFree (self->xlink_href);

  G_OBJECT_CLASS (gst_mpd_adaptation_set_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_adaptation_set_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr adaptation_set_xml_node = NULL;
  GstMPDAdaptationSetNode *self = GST_MPD_ADAPTATION_SET_NODE (node);

  adaptation_set_xml_node = xmlNewNode (NULL, (xmlChar *) "AdaptationSet");

  if (self->id)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "id", self->id);
  if (self->group)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "group",
        self->group);

  if (self->lang)
    gst_xml_helper_set_prop_string (adaptation_set_xml_node, "lang",
        self->lang);

  if (self->contentType)
    gst_xml_helper_set_prop_string (adaptation_set_xml_node, "contentType",
        self->contentType);

  if (self->minBandwidth)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "minBandwidth",
        self->minBandwidth);
  if (self->maxBandwidth)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "maxBandwidth",
        self->maxBandwidth);
  if (self->minWidth)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "minWidth",
        self->minWidth);
  if (self->maxWidth)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "maxWidth",
        self->maxWidth);
  if (self->minHeight)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "minHeight",
        self->minHeight);
  if (self->maxHeight)
    gst_xml_helper_set_prop_uint (adaptation_set_xml_node, "maxHeight",
        self->maxHeight);

  if (self->par)
    gst_xml_helper_set_prop_ratio (adaptation_set_xml_node, "par", self->par);

  gst_xml_helper_set_prop_cond_uint (adaptation_set_xml_node,
      "segmentAlignment", self->segmentAlignment);
  gst_xml_helper_set_prop_cond_uint (adaptation_set_xml_node,
      "subsegmentAlignment", self->subsegmentAlignment);
  gst_xml_helper_set_prop_uint (adaptation_set_xml_node,
      "subsegmentStartsWithSAP", self->subsegmentStartsWithSAP);
  gst_xml_helper_set_prop_boolean (adaptation_set_xml_node,
      "bitstreamSwitching", self->bitstreamSwitching);

  g_list_foreach (self->Accessibility, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);
  g_list_foreach (self->Role, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);
  g_list_foreach (self->Rating, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);
  g_list_foreach (self->Viewpoint, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);

  gst_mpd_node_add_child_node (GST_MPD_NODE (self->SegmentBase),
      adaptation_set_xml_node);
  gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
      (self->SegmentList), adaptation_set_xml_node);
  gst_mpd_mult_segment_base_node_add_child_node (GST_MPD_NODE
      (self->SegmentTemplate), adaptation_set_xml_node);

  g_list_foreach (self->BaseURLs, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);
  g_list_foreach (self->Representations,
      gst_mpd_representation_base_node_get_list_item, adaptation_set_xml_node);
  g_list_foreach (self->ContentComponents, gst_mpd_node_get_list_item,
      adaptation_set_xml_node);

  if (self->xlink_href)
    gst_xml_helper_set_prop_string (adaptation_set_xml_node, "xlink_href",
        self->xlink_href);
  if (self->actuate == GST_MPD_XLINK_ACTUATE_ON_LOAD)
    gst_xml_helper_set_prop_string (adaptation_set_xml_node, "actuate",
        (gchar *) GST_MPD_XLINK_ACTUATE_ON_LOAD_STR);
  return adaptation_set_xml_node;
}

static void
gst_mpd_adaptation_set_node_class_init (GstMPDAdaptationSetNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_adaptation_set_node_finalize;
  object_class->set_property = gst_mpd_adaptation_set_node_set_property;
  object_class->get_property = gst_mpd_adaptation_set_node_get_property;

  m_klass->get_xml_node = gst_mpd_adaptation_set_get_xml_node;

  g_object_class_install_property (object_class, PROP_MPD_ADAPTATION_SET_ID,
      g_param_spec_int ("id", "id",
          "adaptation set id", 0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ADAPTATION_SET_CONTENT_TYPE, g_param_spec_string ("content-type",
          "content type", "content type of the adaptation set", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mpd_adaptation_set_node_init (GstMPDAdaptationSetNode * self)
{
  self->id = 0;
  self->group = 0;
  self->lang = NULL;            /* LangVectorType RFC 5646 */
  self->contentType = NULL;
  self->par = 0;
  self->minBandwidth = 0;
  self->maxBandwidth = 0;
  self->minWidth = 0;
  self->maxWidth = 0;
  self->minHeight = 0;
  self->maxHeight = 0;
  self->segmentAlignment = NULL;
  self->subsegmentAlignment = NULL;
  self->subsegmentStartsWithSAP = GST_SAP_TYPE_0;
  self->bitstreamSwitching = FALSE;
  /* list of Accessibility DescriptorType nodes */
  self->Accessibility = NULL;
  /* list of Role DescriptorType nodes */
  self->Role = NULL;
  /* list of Rating DescriptorType nodes */
  self->Rating = NULL;
  /* list of Viewpoint DescriptorType nodes */
  self->Viewpoint = NULL;
  /* SegmentBase node */
  self->SegmentBase = NULL;
  /* SegmentList node */
  self->SegmentList = NULL;
  /* SegmentTemplate node */
  self->SegmentTemplate = NULL;
  /* list of BaseURL nodes */
  self->BaseURLs = NULL;
  /* list of Representation nodes */
  self->Representations = NULL;
  /* list of ContentComponent nodes */
  self->ContentComponents = NULL;

  self->xlink_href = NULL;
  self->actuate = GST_MPD_XLINK_ACTUATE_ON_REQUEST;
}

GstMPDAdaptationSetNode *
gst_mpd_adaptation_set_node_new (void)
{
  GstMPDAdaptationSetNode *ret;

  ret = g_object_new (GST_TYPE_MPD_ADAPTATION_SET_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_adaptation_set_node_free (GstMPDAdaptationSetNode * self)
{
  if (self)
    gst_object_unref (self);
}
