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
#include "gstmpdlocationnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDLocationNode, gst_mpd_location_node, GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_location_node_finalize (GObject * object)
{
  GstMPDLocationNode *self = GST_MPD_LOCATION_NODE (object);

  g_free (self->location);

  G_OBJECT_CLASS (gst_mpd_location_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_location_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr location_xml_node = NULL;
  GstMPDLocationNode *self = GST_MPD_LOCATION_NODE (node);

  location_xml_node = xmlNewNode (NULL, (xmlChar *) "Location");

  if (self->location)
    gst_xml_helper_set_content (location_xml_node, self->location);

  return location_xml_node;
}

static void
gst_mpd_location_node_class_init (GstMPDLocationNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gst_mpd_location_node_finalize;

  m_klass = GST_MPD_NODE_CLASS (klass);
  m_klass->get_xml_node = gst_mpd_location_get_xml_node;
}

static void
gst_mpd_location_node_init (GstMPDLocationNode * self)
{
  self->location = NULL;
}

GstMPDLocationNode *
gst_mpd_location_node_new (void)
{
  GstMPDLocationNode *ret;

  ret = g_object_new (GST_TYPE_MPD_LOCATION_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_location_node_free (GstMPDLocationNode * self)
{
  if (self)
    gst_object_unref (self);
}
