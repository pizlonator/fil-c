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
#include "gstmpdbaseurlnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDBaseURLNode, gst_mpd_baseurl_node, GST_TYPE_MPD_NODE);

enum
{
  PROP_MPD_BASEURL_0,
  PROP_MPD_BASEURL_URL,
  PROP_MPD_BASEURL_SERVICE_LOCATION,
  PROP_MPD_BASEURL_BYTE_RANGE,
};

/* GObject VMethods */

static void
gst_mpd_baseurl_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDBaseURLNode *self = GST_MPD_BASEURL_NODE (object);
  switch (prop_id) {
    case PROP_MPD_BASEURL_URL:
      g_free (self->baseURL);
      self->baseURL = g_value_dup_string (value);
      break;
    case PROP_MPD_BASEURL_SERVICE_LOCATION:
      g_free (self->serviceLocation);
      self->serviceLocation = g_value_dup_string (value);
      break;
    case PROP_MPD_BASEURL_BYTE_RANGE:
      g_free (self->byteRange);
      self->byteRange = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_baseurl_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDBaseURLNode *self = GST_MPD_BASEURL_NODE (object);
  switch (prop_id) {
    case PROP_MPD_BASEURL_URL:
      g_value_set_string (value, self->baseURL);
      break;
    case PROP_MPD_BASEURL_SERVICE_LOCATION:
      g_value_set_string (value, self->serviceLocation);
      break;
    case PROP_MPD_BASEURL_BYTE_RANGE:
      g_value_set_string (value, self->byteRange);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_baseurl_node_finalize (GObject * object)
{
  GstMPDBaseURLNode *self = GST_MPD_BASEURL_NODE (object);

  g_free (self->baseURL);
  g_free (self->serviceLocation);
  g_free (self->byteRange);

  G_OBJECT_CLASS (gst_mpd_baseurl_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_baseurl_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr baseurl_xml_node = NULL;
  GstMPDBaseURLNode *self = GST_MPD_BASEURL_NODE (node);

  baseurl_xml_node = xmlNewNode (NULL, (xmlChar *) "BaseURL");

  if (self->serviceLocation)
    gst_xml_helper_set_prop_string (baseurl_xml_node, "serviceLocation",
        self->serviceLocation);

  if (self->byteRange)
    gst_xml_helper_set_prop_string (baseurl_xml_node, "byteRange",
        self->byteRange);

  if (self->baseURL)
    gst_xml_helper_set_content (baseurl_xml_node, self->baseURL);

  return baseurl_xml_node;
}

static void
gst_mpd_baseurl_node_class_init (GstMPDBaseURLNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_baseurl_node_finalize;
  object_class->set_property = gst_mpd_baseurl_node_set_property;
  object_class->get_property = gst_mpd_baseurl_node_get_property;

  m_klass->get_xml_node = gst_mpd_baseurl_get_xml_node;

  g_object_class_install_property (object_class, PROP_MPD_BASEURL_URL,
      g_param_spec_string ("url", "base url",
          "url of the base url", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_BASEURL_SERVICE_LOCATION,
      g_param_spec_string ("service-location", "service location",
          "service location", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_BASEURL_BYTE_RANGE,
      g_param_spec_string ("byte-range", "byte range", "byte range", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_mpd_baseurl_node_init (GstMPDBaseURLNode * self)
{
  self->baseURL = NULL;
  self->serviceLocation = NULL;
  self->byteRange = NULL;
}

GstMPDBaseURLNode *
gst_mpd_baseurl_node_new (void)
{
  GstMPDBaseURLNode *ret;

  ret = g_object_new (GST_TYPE_MPD_BASEURL_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_baseurl_node_free (GstMPDBaseURLNode * self)
{
  if (self)
    gst_object_unref (self);
}
