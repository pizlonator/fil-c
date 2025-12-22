/* GStreamer
 *
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Stéphane Cerveau <scerveau@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GSTMPDMULTSEGMENTBASENODE_H__
#define __GSTMPDMULTSEGMENTBASENODE_H__

#include <gst/gst.h>
#include "gstmpdhelper.h"
G_BEGIN_DECLS

#define GST_TYPE_MPD_MULT_SEGMENT_BASE_NODE gst_mpd_mult_segment_base_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDMultSegmentBaseNode, gst_mpd_mult_segment_base_node, GST, MPD_MULT_SEGMENT_BASE_NODE, GstMPDNode)

struct _GstMPDMultSegmentBaseNode
{
  GstObject     base;
  guint duration;                  /* in seconds */
  guint startNumber;
  /* SegmentBaseType extension */
  GstMPDSegmentBaseNode *SegmentBase;
  /* SegmentTimeline node */
  GstMPDSegmentTimelineNode *SegmentTimeline;
  /* BitstreamSwitching node */
  GstMPDURLTypeNode *BitstreamSwitching;
};


void gst_mpd_mult_segment_base_node_add_child_node (GstMPDNode* node, xmlNodePtr parent_xml_node);

G_END_DECLS
#endif /* __GSTMPDMULTSEGMENTBASENODE_H__ */
