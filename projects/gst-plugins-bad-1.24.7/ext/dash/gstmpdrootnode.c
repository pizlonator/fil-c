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
#include "gstmpdrootnode.h"
#include "gstmpdparser.h"

G_DEFINE_TYPE (GstMPDRootNode, gst_mpd_root_node, GST_TYPE_MPD_NODE);

enum
{
  PROP_MPD_ROOT_0,
  PROP_MPD_ROOT_DEFAULT_NAMESPACE,
  PROP_MPD_ROOT_NAMESPACE_XSI,
  PROP_MPD_ROOT_NAMESPACE_EXT,
  PROP_MPD_ROOT_SCHEMA_LOCATION,
  PROP_MPD_ROOT_ID,
  PROP_MPD_ROOT_PROFILES,
  PROP_MPD_ROOT_TYPE,
  PROP_MPD_ROOT_PUBLISH_TIME,
  PROP_MPD_ROOT_AVAILABILTY_START_TIME,
  PROP_MPD_ROOT_AVAILABILTY_END_TIME,
  PROP_MPD_ROOT_MEDIA_PRESENTATION_DURATION,
  PROP_MPD_ROOT_MINIMUM_UPDATE_PERIOD,
  PROP_MPD_ROOT_MIN_BUFFER_TIME,
  PROP_MPD_ROOT_TIMESHIFT_BUFFER_DEPTH,
  PROP_MPD_ROOT_SUGGESTED_PRESENTATION_DELAY,
  PROP_MPD_ROOT_MAX_SEGMENT_DURATION,
  PROP_MPD_ROOT_MAX_SUBSEGMENT_DURATION,
};

/* GObject VMethods */

static void
gst_mpd_root_node_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMPDRootNode *self = GST_MPD_ROOT_NODE (object);
  switch (prop_id) {
    case PROP_MPD_ROOT_DEFAULT_NAMESPACE:
      g_free (self->default_namespace);
      self->default_namespace = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_NAMESPACE_XSI:
      g_free (self->namespace_xsi);
      self->namespace_xsi = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_NAMESPACE_EXT:
      g_free (self->namespace_ext);
      self->namespace_ext = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_SCHEMA_LOCATION:
      g_free (self->schemaLocation);
      self->schemaLocation = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_PROFILES:
      g_free (self->profiles);
      self->profiles = g_value_dup_string (value);
      break;
    case PROP_MPD_ROOT_TYPE:
      self->type = (GstMPDFileType) g_value_get_int (value);
      break;
    case PROP_MPD_ROOT_AVAILABILTY_START_TIME:
      if (self->availabilityStartTime)
        gst_date_time_unref (self->availabilityStartTime);
      self->availabilityStartTime = g_value_dup_boxed (value);
      break;
    case PROP_MPD_ROOT_AVAILABILTY_END_TIME:
      if (self->availabilityEndTime)
        gst_date_time_unref (self->availabilityEndTime);
      self->availabilityEndTime = g_value_dup_boxed (value);
      break;
    case PROP_MPD_ROOT_PUBLISH_TIME:
      if (self->publishTime)
        gst_date_time_unref (self->publishTime);
      self->publishTime = g_value_dup_boxed (value);
      break;
    case PROP_MPD_ROOT_MEDIA_PRESENTATION_DURATION:
      self->mediaPresentationDuration = g_value_get_uint64 (value);
      break;
    case PROP_MPD_ROOT_MINIMUM_UPDATE_PERIOD:
      self->minimumUpdatePeriod = g_value_get_uint64 (value);
      break;
    case PROP_MPD_ROOT_SUGGESTED_PRESENTATION_DELAY:
      self->suggestedPresentationDelay = g_value_get_uint64 (value);
      break;
    case PROP_MPD_ROOT_MIN_BUFFER_TIME:
      self->minBufferTime = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_root_node_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMPDRootNode *self = GST_MPD_ROOT_NODE (object);
  switch (prop_id) {
    case PROP_MPD_ROOT_DEFAULT_NAMESPACE:
      g_value_set_string (value, self->default_namespace);
      break;
    case PROP_MPD_ROOT_NAMESPACE_XSI:
      g_value_set_string (value, self->namespace_xsi);
      break;
    case PROP_MPD_ROOT_NAMESPACE_EXT:
      g_value_set_string (value, self->namespace_ext);
      break;
    case PROP_MPD_ROOT_SCHEMA_LOCATION:
      g_value_set_string (value, self->schemaLocation);
      break;
    case PROP_MPD_ROOT_ID:
      g_value_set_string (value, self->id);
      break;
    case PROP_MPD_ROOT_PROFILES:
      g_value_set_string (value, self->profiles);
      break;
    case PROP_MPD_ROOT_TYPE:
      g_value_set_int (value, self->type);
      break;
    case PROP_MPD_ROOT_AVAILABILTY_START_TIME:
      g_value_set_boxed (value, self->availabilityStartTime);
      break;
    case PROP_MPD_ROOT_AVAILABILTY_END_TIME:
      g_value_set_boxed (value, self->availabilityEndTime);
      break;
    case PROP_MPD_ROOT_PUBLISH_TIME:
      g_value_set_boxed (value, self->publishTime);
      break;
    case PROP_MPD_ROOT_MEDIA_PRESENTATION_DURATION:
      g_value_set_uint64 (value, self->mediaPresentationDuration);
      break;
    case PROP_MPD_ROOT_MINIMUM_UPDATE_PERIOD:
      g_value_set_uint64 (value, self->minimumUpdatePeriod);
      break;
    case PROP_MPD_ROOT_SUGGESTED_PRESENTATION_DELAY:
      g_value_set_uint64 (value, self->suggestedPresentationDelay);
      break;
    case PROP_MPD_ROOT_MIN_BUFFER_TIME:
      g_value_set_uint64 (value, self->minBufferTime);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mpd_root_node_finalize (GObject * object)
{
  GstMPDRootNode *self = GST_MPD_ROOT_NODE (object);

  g_free (self->default_namespace);
  g_free (self->namespace_xsi);
  g_free (self->namespace_ext);
  g_free (self->schemaLocation);
  g_free (self->id);
  g_free (self->profiles);

  if (self->availabilityStartTime)
    gst_date_time_unref (self->availabilityStartTime);
  if (self->availabilityEndTime)
    gst_date_time_unref (self->availabilityEndTime);
  if (self->publishTime)
    gst_date_time_unref (self->publishTime);

  g_list_free_full (self->ProgramInfos,
      (GDestroyNotify) gst_mpd_program_information_node_free);
  g_list_free_full (self->BaseURLs, (GDestroyNotify) gst_mpd_baseurl_node_free);
  g_list_free_full (self->Locations,
      (GDestroyNotify) gst_mpd_location_node_free);
  g_list_free_full (self->Periods, (GDestroyNotify) gst_mpd_period_node_free);
  g_list_free_full (self->Metrics, (GDestroyNotify) gst_mpd_metrics_node_free);
  g_list_free_full (self->UTCTimings,
      (GDestroyNotify) gst_mpd_utctiming_node_free);


  G_OBJECT_CLASS (gst_mpd_root_node_parent_class)->finalize (object);
}

/* Base class */

static xmlNodePtr
gst_mpd_root_get_xml_node (GstMPDNode * node)
{
  xmlNodePtr root_xml_node;
  GstMPDRootNode *self = GST_MPD_ROOT_NODE (node);

  root_xml_node = xmlNewNode (NULL, (xmlChar *) "MPD");

  gst_xml_helper_set_prop_string (root_xml_node, "xmlns",
      self->default_namespace);
  gst_xml_helper_set_prop_string (root_xml_node, "profiles", self->profiles);
  gst_xml_helper_set_prop_string (root_xml_node, "schemaLocation",
      self->schemaLocation);
  gst_xml_helper_set_prop_string (root_xml_node, "xmlns:xsi",
      self->namespace_xsi);
  gst_xml_helper_set_prop_string (root_xml_node, "xmlns:ext",
      self->namespace_ext);
  gst_xml_helper_set_prop_string (root_xml_node, "id", self->id);

  if (self->type == GST_MPD_FILE_TYPE_STATIC)
    gst_xml_helper_set_prop_string (root_xml_node, "type", (gchar *) "static");
  else
    gst_xml_helper_set_prop_string (root_xml_node, "type", (gchar *) "dynamic");


  gst_xml_helper_set_prop_date_time (root_xml_node, "availabilityStartTime",
      self->availabilityStartTime);

  gst_xml_helper_set_prop_date_time (root_xml_node, "availabilityEndTime",
      self->availabilityEndTime);
  gst_xml_helper_set_prop_date_time (root_xml_node, "publishTime",
      self->publishTime);

  if (self->mediaPresentationDuration)
    gst_xml_helper_set_prop_duration (root_xml_node,
        "mediaPresentationDuration", self->mediaPresentationDuration);
  if (self->minimumUpdatePeriod)
    gst_xml_helper_set_prop_duration (root_xml_node, "minimumUpdatePeriod",
        self->minimumUpdatePeriod);
  if (self->minBufferTime)
    gst_xml_helper_set_prop_duration (root_xml_node, "minBufferTime",
        self->minBufferTime);
  if (self->timeShiftBufferDepth)
    gst_xml_helper_set_prop_duration (root_xml_node, "timeShiftBufferDepth",
        self->timeShiftBufferDepth);
  if (self->suggestedPresentationDelay)
    gst_xml_helper_set_prop_duration (root_xml_node,
        "suggestedPresentationDelay", self->suggestedPresentationDelay);
  if (self->maxSegmentDuration)
    gst_xml_helper_set_prop_duration (root_xml_node, "maxSegmentDuration",
        self->maxSegmentDuration);
  if (self->maxSubsegmentDuration)
    gst_xml_helper_set_prop_duration (root_xml_node, "maxSubsegmentDuration",
        self->maxSubsegmentDuration);

  g_list_foreach (self->BaseURLs, gst_mpd_node_get_list_item, root_xml_node);
  g_list_foreach (self->Locations, gst_mpd_node_get_list_item, root_xml_node);
  g_list_foreach (self->ProgramInfos, gst_mpd_node_get_list_item,
      root_xml_node);
  g_list_foreach (self->Periods, gst_mpd_node_get_list_item, root_xml_node);
  g_list_foreach (self->Metrics, gst_mpd_node_get_list_item, root_xml_node);
  g_list_foreach (self->UTCTimings, gst_mpd_node_get_list_item, root_xml_node);

  return root_xml_node;
}

static gboolean
gst_mpd_root_get_xml_buffer (GstMPDNode * node, gchar ** doc_content,
    gint * doc_size)
{
  xmlDocPtr doc;
  xmlNodePtr root_xml_node;
  xmlChar *xmlbody;

  doc = xmlNewDoc ((xmlChar *) "1.0");
  root_xml_node = gst_mpd_root_get_xml_node (node);
  xmlDocSetRootElement (doc, root_xml_node);

  xmlDocDumpMemory (doc, &xmlbody, doc_size);
  *doc_content = g_strndup ((gchar *) xmlbody, *doc_size);
  xmlFree (xmlbody);

  xmlFreeDoc (doc);
  return TRUE;
}

static void
gst_mpd_root_node_class_init (GstMPDRootNodeClass * klass)
{
  GObjectClass *object_class;
  GstMPDNodeClass *m_klass;


  object_class = G_OBJECT_CLASS (klass);
  m_klass = GST_MPD_NODE_CLASS (klass);

  object_class->finalize = gst_mpd_root_node_finalize;
  object_class->set_property = gst_mpd_root_node_set_property;
  object_class->get_property = gst_mpd_root_node_get_property;

  m_klass->get_xml_buffer = gst_mpd_root_get_xml_buffer;
  m_klass->get_xml_node = gst_mpd_root_get_xml_node;

  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_DEFAULT_NAMESPACE, g_param_spec_string ("default-namespace",
          "default namespace", "default namespace", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_NAMESPACE_XSI,
      g_param_spec_string ("namespace-xsi", "namespace xsi", "namespace xsi",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_NAMESPACE_EXT,
      g_param_spec_string ("namespace-ext", "namespace ext", "namespace ext",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_SCHEMA_LOCATION,
      g_param_spec_string ("schema-location", "schema location",
          "schema location for period", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_ID,
      g_param_spec_string ("id", "id", "unique id for period", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_PROFILES,
      g_param_spec_string ("profiles", "profiles", "profiles", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MPD_ROOT_TYPE,
      g_param_spec_int ("type", "MPD type",
          "MPD type",
          GST_MPD_FILE_TYPE_STATIC, GST_MPD_FILE_TYPE_DYNAMIC,
          GST_MPD_FILE_TYPE_STATIC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_AVAILABILTY_START_TIME,
      g_param_spec_boxed ("availability-start-time", "Availability start time",
          "MPD availability start time", GST_TYPE_DATE_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_AVAILABILTY_END_TIME,
      g_param_spec_boxed ("availability-end-time", "Availability end time",
          "MPD availability end time", GST_TYPE_DATE_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_PUBLISH_TIME,
      g_param_spec_boxed ("publish-time", "publish time",
          "MPD publish time", GST_TYPE_DATE_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_MEDIA_PRESENTATION_DURATION,
      g_param_spec_uint64 ("media-presentation-duration",
          "media presentation duration", "media presentation duration", 0,
          G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_MINIMUM_UPDATE_PERIOD,
      g_param_spec_uint64 ("minimum-update-period",
          "minimum update period", "minimum update period", 0,
          G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_SUGGESTED_PRESENTATION_DELAY,
      g_param_spec_uint64 ("suggested-presentation-delay",
          "suggested presentation delay", "suggested presentation delay", 0,
          G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
      PROP_MPD_ROOT_MIN_BUFFER_TIME,
      g_param_spec_uint64 ("min-buffer-time", "mininim buffer time",
          "mininim buffer time", 0,
          G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mpd_root_node_init (GstMPDRootNode * self)
{
  self->default_namespace = NULL;
  self->namespace_xsi = NULL;
  self->namespace_ext = NULL;
  self->schemaLocation = NULL;
  self->id = NULL;
  self->profiles = NULL;
  self->type = GST_MPD_FILE_TYPE_STATIC;
  self->availabilityStartTime = NULL;
  self->availabilityEndTime = NULL;
  self->publishTime = NULL;
  self->mediaPresentationDuration = 0;  /* [ms] */
  self->minimumUpdatePeriod = 0;        /* [ms] */
  self->minBufferTime = 2000;   /* [ms] */
  self->timeShiftBufferDepth = 0;       /* [ms] */
  self->suggestedPresentationDelay = 0; /* [ms] */
  self->maxSegmentDuration = 0; /* [ms] */
  self->maxSubsegmentDuration = 0;      /* [ms] */
  /* list of BaseURL nodes */
  self->BaseURLs = NULL;
  /* list of Location nodes */
  self->Locations = NULL;
  /* List of ProgramInformation nodes */
  self->ProgramInfos = NULL;
  /* list of Periods nodes */
  self->Periods = NULL;
  /* list of Metrics nodes */
  self->Metrics = NULL;
  /* list of GstUTCTimingNode nodes */
  self->UTCTimings = NULL;
}

GstMPDRootNode *
gst_mpd_root_node_new (void)
{
  GstMPDRootNode *ret;

  ret = g_object_new (GST_TYPE_MPD_ROOT_NODE, NULL);
  gst_object_ref_sink (ret);
  return ret;
}

void
gst_mpd_root_node_free (GstMPDRootNode * self)
{
  if (self)
    gst_object_unref (self);
}
