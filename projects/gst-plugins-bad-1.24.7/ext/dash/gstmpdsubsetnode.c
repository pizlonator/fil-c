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
#include "gstmpdsubsetnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDSubsetNode, gst_mpd_subset_node, GST_TYPE_MPD_NODE);

/* GObject VMethods */

static void
gst_mpd_subset_node_finalize (GObject * object)
{
  GstMPDSubsetNode *self = GST_MPD_SUBSET_NODE (object);

  if (self->contains)
    xmlFree (self->contains);

  G_OBJECT_CLASS (gst_mpd_subset_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_subset_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr subset_xml_node = NULL;
  GstMPDSubsetNode *self = GST_MPD_SUBSET_NODE (node);

  subset_xml_node = xmlNewNode (NULL, (xmlChar *) "Subset");

  if (self->contains)
    gst_xml_helper_set_prop_uint_vector_type (subset_xml_node, "contains",
        self->contains, self->contains_size);

  return subset_xml_node;
}

static void
gst_mpd_subset_node_class_init (GstMPDSubsetNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_subset_node_finalize;

  m_klass->get_xml_node = gst_mpd_subset_get_xml_node;
}

static void
gst_mpd_subset_node_init (GstMPDSubsetNode * self)
{
  self->contains_size = 0;
  self->contains = NULL;
}

GstMPDSubsetNode *
gst_mpd_subset_node_new (void)
{
  GstMPDSubsetNode *ret;

  ret = g_object_new (GST_TYPE_MPD_SUBSET_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_subset_node_free (GstMPDSubsetNode * self)
{
  if (self)
    gst_object_unref (self);
}
