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
#ifndef __GSTMPDREPRESENTATIONBASENODE_H__
#define __GSTMPDREPRESENTATIONBASENODE_H__

#include <gst/gst.h>
#include "gstmpdhelper.h"
G_BEGIN_DECLS

#define GST_TYPE_MPD_REPRESENTATION_BASE_NODE gst_mpd_representation_base_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDRepresentationBaseNode, gst_mpd_representation_base_node, GST, MPD_REPRESENTATION_BASE_NODE, GstMPDNode)


struct _GstMPDRepresentationBaseNode
{
  GstObject     base;
  gchar *profiles;
  guint width;
  guint height;
  GstXMLRatio *sar;
  GstXMLFrameRate *minFrameRate;
  GstXMLFrameRate *maxFrameRate;
  GstXMLFrameRate *frameRate;
  gchar *audioSamplingRate;
  gchar *mimeType;
  gchar *segmentProfiles;
  gchar *codecs;
  gdouble maximumSAPPeriod;
  GstMPDSAPType startWithSAP;
  gdouble maxPlayoutRate;
  gboolean codingDependency;
  gchar *scanType;
  /* list of FramePacking DescriptorType nodes */
  GList *FramePacking;
  /* list of AudioChannelConfiguration DescriptorType nodes */
  GList *AudioChannelConfiguration;
  /* list of ContentProtection DescriptorType nodes */
  GList *ContentProtection;
};


void gst_mpd_representation_base_node_get_list_item (gpointer data, gpointer user_data);

G_END_DECLS
#endif /* __GSTMPDREPRESENTATIONBASENODE_H__ */
