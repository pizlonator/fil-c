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
#include "gstmpdsegmentlistnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSegmentListNode, gst_mpd_segment_list_node,
    GST_TYPE_MPD_MULT_SEGMENT_BASE_NODE);

/* GObject VMethods */

static void
gst_mpd_segment_list_node_finalize (GObject * object)
{
  GstMPDSegmentListNode *self = GST_MPD_SEGMENT_LIST_NODE (object);

  g_list_free_full (self->SegmentURL,
      (GDestroyNotify) gst_mpd_segment_url_node_free);
  if (self->xlink_href)
    xmlFree (self->xlink_href);

  G_OBJECT_CLASS (gst_mpd_segment_list_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_segment_list_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr segment_list_xml_node = NULL;
  GstMPDSegmentListNode *self = GST_MPD_SEGMENT_LIST_NODE (node);

  segment_list_xml_node = xmlNewNode (NULL, (xmlChar *) "SegmentList");

  g_list_foreach (self->SegmentURL, gst_mpd_node_get_list_item,
      segment_list_xml_node);

  if (self->xlink_href)
    gst_xml_helper_set_prop_string (segment_list_xml_node, "xlink_href",
        self->xlink_href);

  return segment_list_xml_node;
}

static void
gst_mpd_segment_list_node_class_init (GstMPDSegmentListNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_segment_list_node_finalize;

  m_klass->get_xml_node = gst_mpd_segment_list_get_xml_node;
}

static void
gst_mpd_segment_list_node_init (GstMPDSegmentListNode * self)
{
  self->SegmentURL = NULL;
  self->xlink_href = NULL;
  self->actuate = GST_MPD_XLINK_ACTUATE_ON_REQUEST;
}

GstMPDSegmentListNode *
gst_mpd_segment_list_node_new (void)
{
  GstMPDSegmentListNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SEGMENT_LIST_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_segment_list_node_free (GstMPDSegmentListNode * self)
{
  if (self)
    gst_object_unref (self);
}

void
gst_mpd_segment_list_node_add_segment (GstMPDSegmentListNode * self,
    GstMPDSegmentURLNode * segment_url)
{
  g_return_if_fail (self != NULL);

  self->SegmentURL = g_list_append (self->SegmentURL, segment_url);
}
