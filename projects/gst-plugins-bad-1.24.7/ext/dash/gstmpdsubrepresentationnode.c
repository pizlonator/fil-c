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
#include "gstmpdsubrepresentationnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSubRepresentationNode, gst_mpd_sub_representation_node,
    GST_TYPE_MPD_REPRESENTATION_BASE_NODE);

/* GObject VMethods */

static void
gst_mpd_sub_representation_node_finalize (GObject * object)
{
  GstMPDSubRepresentationNode *self = GST_MPD_SUB_REPRESENTATION_NODE (object);

  if (self->dependencyLevel)
    xmlFree (self->dependencyLevel);
  g_strfreev (self->contentComponent);

  G_OBJECT_CLASS (gst_mpd_sub_representation_node_parent_class)->finalize
      (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_sub_representation_get_xml_node (GstMPDNode * node)
{
  gchar *value = NULL;
  xmlNodePtr sub_representation_xml_node = NULL;
  GstMPDSubRepresentationNode *self = GST_MPD_SUB_REPRESENTATION_NODE (node);

  sub_representation_xml_node =
      xmlNewNode (NULL, (xmlChar *) "SubRepresentation");

  gst_xml_helper_set_prop_uint (sub_representation_xml_node, "level",
      self->level);

  gst_xml_helper_set_prop_uint_vector_type (sub_representation_xml_node,
      "dependencyLevel", self->dependencyLevel, self->dependencyLevel_size);

  gst_xml_helper_set_prop_uint (sub_representation_xml_node, "bandwidth",
      self->level);

  if (self->contentComponent) {
    value = g_strjoinv (" ", self->contentComponent);
    gst_xml_helper_set_prop_string (sub_representation_xml_node,
        "contentComponent", value);
    g_free (value);
  }

  return sub_representation_xml_node;
}

static void
gst_mpd_sub_representation_node_class_init (GstMPDSubRepresentationNodeClass *
    klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_sub_representation_node_finalize;

  m_klass->get_xml_node = gst_mpd_sub_representation_get_xml_node;
}

static void
gst_mpd_sub_representation_node_init (GstMPDSubRepresentationNode * self)
{
  self->level = 0;
  self->dependencyLevel = NULL;
  self->dependencyLevel_size = 0;
  self->bandwidth = 0;
  self->contentComponent = NULL;
}

GstMPDSubRepresentationNode *
gst_mpd_sub_representation_node_new (void)
{
  GstMPDSubRepresentationNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SUB_REPRESENTATION_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_sub_representation_node_free (GstMPDSubRepresentationNode * self)
{
  if (self)
    gst_object_unref (self);
}
