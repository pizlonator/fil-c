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
#include "gstmpdsnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSNode, gst_mpd_s_node, GST_TYPE_MPD_NODE);

/* Base class */

static xmlNodePtr
gst_mpd_s_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr s_xml_node = NULL;
  GstMPDSNode *self = GST_MPD_S_NODE (node);

  s_xml_node = xmlNewNode (NULL, (xmlChar *) "S");

  if (self->t)
    gst_xml_helper_set_prop_uint64 (s_xml_node, "t", self->t);

  if (self->d)
    gst_xml_helper_set_prop_uint64 (s_xml_node, "d", self->d);

  if (self->r)
    gst_xml_helper_set_prop_int (s_xml_node, "r", self->r);

  return s_xml_node;
}

static void
gst_mpd_s_node_class_init (GstMPDSNodeClass * klass)
{
  GstMPDNodeClass *m_klass;

  m_klass = GST_MPD_NODE_CLASS (klass);

  m_klass->get_xml_node = gst_mpd_s_get_xml_node;
}

static void
gst_mpd_s_node_init (GstMPDSNode * self)
{
  self->t = 0;
  self->d = 0;
  self->r = 0;
}

GstMPDSNode *
gst_mpd_s_node_new (void)
{
  GstMPDSNode *ret;

  ret = g_object_new (GST_TYPE_MPD_S_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_s_node_free (GstMPDSNode * self)
{
  if (self)
    gst_object_unref (self);
}

GstMPDSNode *
gst_mpd_s_node_clone (GstMPDSNode * s_node)
{
  GstMPDSNode *clone = NULL;

  if (s_node) {
    clone = gst_mpd_s_node_new ();
    clone->t = s_node->t;
    clone->d = s_node->d;
    clone->r = s_node->r;
  }

  return clone;
}
