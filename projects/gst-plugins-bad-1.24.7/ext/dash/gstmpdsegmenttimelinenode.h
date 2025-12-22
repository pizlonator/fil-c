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
#ifndef __GSTMPDSEGMENTTIMELINENODE_H__
#define __GSTMPDSEGMENTTIMELINENODE_H__

#include <gst/gst.h>
#include "gstmpdnode.h"

G_BEGIN_DECLS

#define GST_TYPE_MPD_SEGMENT_TIMELINE_NODE gst_mpd_segment_timeline_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDSegmentTimelineNode, gst_mpd_segment_timeline_node, GST, MPD_SEGMENT_TIMELINE_NODE, GstMPDNode)

struct _GstMPDSegmentTimelineNode
{
  GstObject parent_instance;
  /* list of S nodes */
  GQueue S;
};

GstMPDSegmentTimelineNode * gst_mpd_segment_timeline_node_new (void);
void gst_mpd_segment_timeline_node_free (GstMPDSegmentTimelineNode* self);

GstMPDSegmentTimelineNode *gst_mpd_segment_timeline_node_clone (GstMPDSegmentTimelineNode * pointer);

G_END_DECLS

#endif /* __GSTMPDSEGMENTTIMELINENODE_H__ */
