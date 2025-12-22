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
#ifndef __GSTMPDREPRESENTATIONNODE_H__
#define __GSTMPDREPRESENTATIONNODE_H__

#include <gst/gst.h>
#include "gstmpdhelper.h"
#include "gstmpdrepresentationbasenode.h"
#include "gstmpdsegmentlistnode.h"
#include "gstmpdsegmenttemplatenode.h"

G_BEGIN_DECLS

#define GST_TYPE_MPD_REPRESENTATION_NODE gst_mpd_representation_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDRepresentationNode, gst_mpd_representation_node, GST, MPD_REPRESENTATION_NODE, GstMPDRepresentationBaseNode)

struct _GstMPDRepresentationNode
{
  GstMPDRepresentationBaseNode parent_instance;
  gchar *id;
  guint bandwidth;
  guint qualityRanking;
  gchar **dependencyId;              /* StringVectorType */
  gchar **mediaStreamStructureId;    /* StringVectorType */
  /* list of BaseURL nodes */
  GList *BaseURLs;
  /* list of SubRepresentation nodes */
  GList *SubRepresentations;
  /* SegmentBase node */
  GstMPDSegmentBaseNode *SegmentBase;
  /* SegmentTemplate node */
  GstMPDSegmentTemplateNode *SegmentTemplate;
  /* SegmentList node */
  GstMPDSegmentListNode *SegmentList;
};


GstMPDRepresentationNode * gst_mpd_representation_node_new (void);
void gst_mpd_representation_node_free (GstMPDRepresentationNode* self);

G_END_DECLS

#endif /* __GSTMPDREPRESENTATIONNODE_H__ */
