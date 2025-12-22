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
#include "gstmpdsegmentbasenode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSegmentBaseNode, gst_mpd_segment_base_node,
    GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_segment_base_node_finalize (GObject * object)
{
  GstMPDSegmentBaseNode *self = GST_MPD_SEGMENT_BASE_NODE (object);

  if (self->indexRange)
    g_slice_free (GstXMLRange, self->indexRange);
  gst_mpd_url_type_node_free (self->Initialization);
  gst_mpd_url_type_node_free (self->RepresentationIndex);

  G_OBJECT_CLASS (gst_mpd_segment_base_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_segment_base_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr segment_base_xml_node = NULL;
  GstMPDSegmentBaseNode *self = GST_MPD_SEGMENT_BASE_NODE (node);

  segment_base_xml_node = xmlNewNode (NULL, (xmlChar *) "SegmentBase");

  if (self->timescale)
    gst_xml_helper_set_prop_uint (segment_base_xml_node, "timescale",
        self->timescale);
  if (self->presentationTimeOffset)
    gst_xml_helper_set_prop_uint64 (segment_base_xml_node,
        "presentationTimeOffset", self->presentationTimeOffset);
  if (self->indexRange) {
    gst_xml_helper_set_prop_range (segment_base_xml_node, "indexRange",
        self->indexRange);
    gst_xml_helper_set_prop_boolean (segment_base_xml_node, "indexRangeExact",
        self->indexRangeExact);
  }
  if (self->Initialization)
    gst_mpd_node_add_child_node (GST_MPD_NODE (self->Initialization),
        segment_base_xml_node);
  if (self->RepresentationIndex)
    gst_mpd_node_add_child_node (GST_MPD_NODE (self->RepresentationIndex),
        segment_base_xml_node);

  return segment_base_xml_node;
}

static void
gst_mpd_segment_base_node_class_init (GstMPDSegmentBaseNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_segment_base_node_finalize;

  m_klass->get_xml_node = gst_mpd_segment_base_get_xml_node;
}

static void
gst_mpd_segment_base_node_init (GstMPDSegmentBaseNode * self)
{
  self->timescale = 0;
  self->presentationTimeOffset = 0;
  self->indexRange = NULL;
  self->indexRangeExact = FALSE;
  /* Initialization node */
  self->Initialization = NULL;
  /* RepresentationIndex node */
  self->RepresentationIndex = NULL;
}

GstMPDSegmentBaseNode *
gst_mpd_segment_base_node_new (void)
{
  GstMPDSegmentBaseNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SEGMENT_BASE_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_segment_base_node_free (GstMPDSegmentBaseNode * self)
{
  if (self)
    gst_object_unref (self);
}
