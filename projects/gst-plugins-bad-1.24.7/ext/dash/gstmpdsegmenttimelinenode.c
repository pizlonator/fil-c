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
#include "gstmpdsegmenttimelinenode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSegmentTimelineNode, gst_mpd_segment_timeline_node,
    GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_segment_timeline_node_finalize (GObject * object)
{
  GstMPDSegmentTimelineNode *self = GST_MPD_SEGMENT_TIMELINE_NODE (object);

  g_queue_foreach (&self->S, (GFunc) gst_mpd_s_node_free, NULL);
  g_queue_clear (&self->S);

  G_OBJECT_CLASS (gst_mpd_segment_timeline_node_parent_class)->finalize
      (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_segment_timeline_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr segment_timeline_xml_node = NULL;
  GstMPDSegmentTimelineNode *self = GST_MPD_SEGMENT_TIMELINE_NODE (node);

  segment_timeline_xml_node = xmlNewNode (NULL, (xmlChar *) "SegmentTimeline");

  g_queue_foreach (&self->S, (GFunc) gst_mpd_node_get_list_item,
      segment_timeline_xml_node);

  return segment_timeline_xml_node;
}

static void
gst_mpd_segment_timeline_node_class_init (GstMPDSegmentTimelineNodeClass *
    klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_segment_timeline_node_finalize;

  m_klass->get_xml_node = gst_mpd_segment_timeline_get_xml_node;
}

static void
gst_mpd_segment_timeline_node_init (GstMPDSegmentTimelineNode * self)
{
  g_queue_init (&self->S);
}

GstMPDSegmentTimelineNode *
gst_mpd_segment_timeline_node_new (void)
{
  GstMPDSegmentTimelineNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SEGMENT_TIMELINE_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_segment_timeline_node_free (GstMPDSegmentTimelineNode * self)
{
  if (self)
    gst_object_unref (self);
}

GstMPDSegmentTimelineNode *
gst_mpd_segment_timeline_node_clone (GstMPDSegmentTimelineNode *
    segment_timeline)
{
  GstMPDSegmentTimelineNode *clone = NULL;
  GList *list;

  if (segment_timeline) {
    clone = gst_mpd_segment_timeline_node_new ();
    for (list = g_queue_peek_head_link (&segment_timeline->S); list;
        list = g_list_next (list)) {
      GstMPDSNode *s_node;
      s_node = (GstMPDSNode *) list->data;
      if (s_node) {
        g_queue_push_tail (&clone->S, gst_mpd_s_node_clone (s_node));
      }
    }
  }

  return clone;
}
