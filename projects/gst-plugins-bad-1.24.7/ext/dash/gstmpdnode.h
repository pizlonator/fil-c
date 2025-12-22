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
#ifndef __GSTMPDNODE_H__
#define __GSTMPDNODE_H__

#include <gst/gst.h>
#include "gstxmlhelper.h"
G_BEGIN_DECLS

#define GST_TYPE_MPD_NODE gst_mpd_node_get_type ()
G_DECLARE_DERIVABLE_TYPE (GstMPDNode, gst_mpd_node,GST, MPD_NODE, GstObject)

typedef gboolean (*GstMPDGetXMLBuffer) (GstMPDNode * n, gchar ** doc_content, int *doc_size);
typedef xmlNodePtr (*GstMPDGetXMLNode) (GstMPDNode * n);

struct _GstMPDNodeClass {
    GstObjectClass base;

    GstMPDGetXMLBuffer get_xml_buffer;
    GstMPDGetXMLNode get_xml_node;
};

gboolean gst_mpd_node_get_xml_buffer (GstMPDNode * node, gchar ** xml_content, int * xml_size);
xmlNodePtr gst_mpd_node_get_xml_pointer (GstMPDNode * node);

void gst_mpd_node_get_list_item (gpointer data, gpointer user_data);
void gst_mpd_node_add_child_node (GstMPDNode* data, xmlNodePtr user_data);

G_END_DECLS
#endif /* __GSTMPDNODE_H__ */
