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
#include "gstmpdsegmenturlnode.h"
#include "gstmpdparser.h"
#include "gstmpdhelper.h"

G_DEFINE_TYPE (GstMPDSegmentURLNode, gst_mpd_segment_url_node,
    GST_TYPE_MPD_NODE);

enum
{
  PROP_MPD_SEGMENT_URL_0,
  PROP_MPD_SEGMENT_URL_MEDIA,
};

/* GObject VMethods */

static void
gst_mpd_segment_url_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDSegmentURLNode *self = GST_MPD_SEGMENT_URL_NODE (object);
  switch (prop_id) {
    case PROP_MPD_SEGMENT_URL_MEDIA:
      g_free (self->media);
      self->media = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_segment_url_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDSegmentURLNode *self = GST_MPD_SEGMENT_URL_NODE (object);
  switch (prop_id) {
    case PROP_MPD_SEGMENT_URL_MEDIA:
      g_value_set_string (value, self->media);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_segment_url_node_finalize (GObject * object)
{
  GstMPDSegmentURLNode *self = GST_MPD_SEGMENT_URL_NODE (object);

  if (self->media)
    xmlFree (self->media);
  g_slice_free (GstXMLRange, self->mediaRange);
  if (self->index)
    xmlFree (self->index);
  g_slice_free (GstXMLRange, self->indexRange);

  G_OBJECT_CLASS (gst_mpd_segment_url_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_segment_url_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr segment_url_xml_node = NULL;
  GstMPDSegmentURLNode *self = GST_MPD_SEGMENT_URL_NODE (node);

  segment_url_xml_node = xmlNewNode (NULL, (xmlChar *) "SegmentURL");

  if (self->media)
    gst_xml_helper_set_prop_string (segment_url_xml_node, "media", self->media);

  if (self->mediaRange)
    gst_xml_helper_set_prop_range (segment_url_xml_node, "mediaRange",
        self->mediaRange);

  if (self->index)
    gst_xml_helper_set_prop_string (segment_url_xml_node, "index", self->index);

  if (self->indexRange)
    gst_xml_helper_set_prop_range (segment_url_xml_node, "indexRange",
        self->indexRange);

  return segment_url_xml_node;
}

static void
gst_mpd_segment_url_node_class_init (GstMPDSegmentURLNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_segment_url_node_finalize;
  object_class->set_property = gst_mpd_segment_url_node_set_property;
  object_class->get_property = gst_mpd_segment_url_node_get_property;

  m_klass->get_xml_node = gst_mpd_segment_url_get_xml_node;

  g_object_class_install_property (object_class,
      PROP_MPD_SEGMENT_URL_MEDIA, g_param_spec_string ("media",
          "media", "media description", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mpd_segment_url_node_init (GstMPDSegmentURLNode * self)
{
  self->media = NULL;
  self->mediaRange = NULL;
  self->index = NULL;
  self->indexRange = NULL;
}

GstMPDSegmentURLNode *
gst_mpd_segment_url_node_new (void)
{
  GstMPDSegmentURLNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SEGMENT_URL_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_segment_url_node_free (GstMPDSegmentURLNode * self)
{
  if (self)
    gst_object_unref (self);
}

GstMPDSegmentURLNode *
gst_mpd_segment_url_node_clone (GstMPDSegmentURLNode * seg_url)
{
  GstMPDSegmentURLNode *clone = NULL;

  if (seg_url) {
    clone = gst_mpd_segment_url_node_new ();
    clone->media = xmlMemStrdup (seg_url->media);
    clone->mediaRange = gst_xml_helper_clone_range (seg_url->mediaRange);
    clone->index = xmlMemStrdup (seg_url->index);
    clone->indexRange = gst_xml_helper_clone_range (seg_url->indexRange);
  }

  return clone;
}
