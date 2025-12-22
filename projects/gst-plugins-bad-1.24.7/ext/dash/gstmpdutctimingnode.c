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
#include "gstmpdutctimingnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDUTCTimingNode, gst_mpd_utctiming_node, GST_TYPE_MPD_NODE);

static const struct GstMPDUTCTimingMethod gst_mpd_utctiming_methods[] = {
  {"urn:mpeg:dash:utc:ntp:2014", GST_MPD_UTCTIMING_TYPE_NTP},
  {"urn:mpeg:dash:utc:sntp:2014", GST_MPD_UTCTIMING_TYPE_SNTP},
  {"urn:mpeg:dash:utc:http-head:2014", GST_MPD_UTCTIMING_TYPE_HTTP_HEAD},
  {"urn:mpeg:dash:utc:http-xsdate:2014", GST_MPD_UTCTIMING_TYPE_HTTP_XSDATE},
  {"urn:mpeg:dash:utc:http-iso:2014", GST_MPD_UTCTIMING_TYPE_HTTP_ISO},
  {"urn:mpeg:dash:utc:http-ntp:2014", GST_MPD_UTCTIMING_TYPE_HTTP_NTP},
  {"urn:mpeg:dash:utc:direct:2014", GST_MPD_UTCTIMING_TYPE_DIRECT},
  /*
   * Early working drafts used the :2012 namespace and this namespace is
   * used by some DASH packagers. To work-around these packagers, we also
   * accept the early draft scheme names.
   */
  {"urn:mpeg:dash:utc:ntp:2012", GST_MPD_UTCTIMING_TYPE_NTP},
  {"urn:mpeg:dash:utc:sntp:2012", GST_MPD_UTCTIMING_TYPE_SNTP},
  {"urn:mpeg:dash:utc:http-head:2012", GST_MPD_UTCTIMING_TYPE_HTTP_HEAD},
  {"urn:mpeg:dash:utc:http-xsdate:2012", GST_MPD_UTCTIMING_TYPE_HTTP_XSDATE},
  {"urn:mpeg:dash:utc:http-iso:2012", GST_MPD_UTCTIMING_TYPE_HTTP_ISO},
  {"urn:mpeg:dash:utc:http-ntp:2012", GST_MPD_UTCTIMING_TYPE_HTTP_NTP},
  {"urn:mpeg:dash:utc:direct:2012", GST_MPD_UTCTIMING_TYPE_DIRECT},
  {NULL, 0}
};

/* GObject VMethods */

static void
gst_mpd_utctiming_node_finalize (GObject * object)
{
  GstMPDUTCTimingNode *self = GST_MPD_UTCTIMING_NODE (object);

  g_strfreev (self->urls);

  G_OBJECT_CLASS (gst_mpd_utctiming_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_utc_timing_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr utc_timing_xml_node = NULL;
  gchar *value = NULL;
  GstMPDUTCTimingNode *self = GST_MPD_UTCTIMING_NODE (node);

  utc_timing_xml_node = xmlNewNode (NULL, (xmlChar *) "UTCTiming");

  if (self->method) {
    gst_xml_helper_set_prop_string (utc_timing_xml_node, "schemeiduri",
        (gchar *) gst_mpd_utctiming_get_scheme_id_uri (self->method));
  }
  if (self->urls) {
    value = g_strjoinv (" ", self->urls);
    gst_xml_helper_set_prop_string (utc_timing_xml_node, "value", value);
    g_free (value);
  }

  return utc_timing_xml_node;
}

static void
gst_mpd_utctiming_node_class_init (GstMPDUTCTimingNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;

  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_utctiming_node_finalize;

  m_klass->get_xml_node = gst_mpd_utc_timing_get_xml_node;
}

static void
gst_mpd_utctiming_node_init (GstMPDUTCTimingNode * self)
{
  self->method = 0;
  self->urls = NULL;
}

GstMPDUTCTimingNode *
gst_mpd_utctiming_node_new (void)
{
  GstMPDUTCTimingNode *ret;

  ret = g_object_new (GST_TYPE_MPD_UTCTIMING_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_utctiming_node_free (GstMPDUTCTimingNode * self)
{
  if (self)
    gst_object_unref (self);
}

const gchar *
gst_mpd_utctiming_get_scheme_id_uri (GstMPDUTCTimingType type)
{
  int i;
  for (i = 0; gst_mpd_utctiming_methods[i].name; ++i) {
    if (type == gst_mpd_utctiming_methods[i].method)
      return gst_mpd_utctiming_methods[i].name;
  }
  return NULL;
}

GstMPDUTCTimingType
gst_mpd_utctiming_get_method (gchar * schemeIDURI)
{
  int i;
  for (i = 0; gst_mpd_utctiming_methods[i].name; ++i) {
    if (g_ascii_strncasecmp (gst_mpd_utctiming_methods[i].name,
            schemeIDURI, strlen (gst_mpd_utctiming_methods[i].name)) == 0)
      return gst_mpd_utctiming_methods[i].method;
  }
  return GST_MPD_UTCTIMING_TYPE_UNKNOWN;
}
