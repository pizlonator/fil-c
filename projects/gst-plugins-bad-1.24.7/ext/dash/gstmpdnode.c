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
#include "gstmpdnode.h"

G_DEFINE_TYPE (GstMPDNode, gst_mpd_node, GST_TYPE_OBJECT);

/* GObject VMethods */

static void
gst_mpd_node_class_init (GstMPDNodeClass * klass)
{
}

static void
gst_mpd_node_init (GstMPDNode * self)
{
}

void
gst_mpd_node_get_list_item (gpointer data, gpointer user_data)
{
  GstMPDNode *node = (GstMPDNode *) data;
  xmlNodePtr parent_xml_node = (xmlNodePtr) user_data;
  xmlNodePtr new_xml_node = gst_mpd_node_get_xml_pointer (node);

  xmlAddChild (parent_xml_node, new_xml_node);
}

void
gst_mpd_node_add_child_node (GstMPDNode * child, xmlNodePtr parent)
{
  xmlNodePtr new_xml_node = gst_mpd_node_get_xml_pointer (child);
  xmlAddChild (parent, new_xml_node);
}

gboolean
gst_mpd_node_get_xml_buffer (GstMPDNode * node, gchar ** xml_content,
    int *xml_size)
{
  GstMPDNodeClass *klass;

  klass = GST_MPD_NODE_GET_CLASS (node);
  if (klass->get_xml_buffer)
    return klass->get_xml_buffer (node, xml_content, xml_size);
  else
    return FALSE;
}

xmlNodePtr
gst_mpd_node_get_xml_pointer (GstMPDNode * node)
{
  GstMPDNodeClass *klass;
  if (!node)
    return NULL;
  klass = GST_MPD_NODE_GET_CLASS (node);
  if (klass->get_xml_node)
    return klass->get_xml_node (node);
  else
    return NULL;
}
