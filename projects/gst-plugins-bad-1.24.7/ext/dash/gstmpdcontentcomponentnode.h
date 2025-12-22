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
#ifndef __GSTMPDCONTENTCOMPONENTNODE_H__
#define __GSTMPDCONTENTCOMPONENTNODE_H__

#include <gst/gst.h>
#include "gstmpdhelper.h"

G_BEGIN_DECLS

#define GST_TYPE_MPD_CONTENT_COMPONENT_NODE gst_mpd_content_component_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDContentComponentNode, gst_mpd_content_component_node, GST, MPD_CONTENT_COMPONENT_NODE, GstMPDNode)

struct _GstMPDContentComponentNode
{
  GstObject parent_instance;
  guint id;
  gchar *lang; /* LangVectorType RFC 5646 */
  gchar *contentType;
  GstXMLRatio *par;
  /* list of Accessibility DescriptorType nodes */
  GList *Accessibility;
  /* list of Role DescriptorType nodes */
  GList *Role;
  /* list of Rating DescriptorType nodes */
  GList *Rating;
  /* list of Viewpoint DescriptorType nodes */
  GList *Viewpoint;
};

GstMPDContentComponentNode * gst_mpd_content_component_node_new (void);
void gst_mpd_content_component_node_free (GstMPDContentComponentNode* self);

G_END_DECLS

#endif /* __GSTMPDCONTENTCOMPONENTNODE_H__ */
