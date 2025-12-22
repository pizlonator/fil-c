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
#ifndef __GSTMPDDESCRIPTORTYPENODE_H__
#define __GSTMPDDESCRIPTORTYPENODE_H__

#include <gst/gst.h>
#include "gstmpdhelper.h"

G_BEGIN_DECLS

#define GST_TYPE_MPD_DESCRIPTOR_TYPE_NODE gst_mpd_descriptor_type_node_get_type ()
G_DECLARE_FINAL_TYPE (GstMPDDescriptorTypeNode, gst_mpd_descriptor_type_node, GST, MPD_DESCRIPTOR_TYPE_NODE, GstMPDNode)


struct _GstMPDDescriptorTypeNode
{
  GstObject     parent_instance;
  gchar *node_name;
  gchar *schemeIdUri;
  gchar *value;
};

GstMPDDescriptorTypeNode * gst_mpd_descriptor_type_node_new (const gchar* name);
void gst_mpd_descriptor_type_node_free (GstMPDDescriptorTypeNode* self);

G_END_DECLS

#endif /* __GSTMPDDESCRIPTORTYPENODE_H__ */
