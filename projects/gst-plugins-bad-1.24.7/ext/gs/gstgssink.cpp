/* GStreamer
 * Copyright (C) 2020 Julien Isorce <jisorce@oblong.com>
 *
 * gstgssink.cpp:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:element-gssink
 * @title: gssink
 * @see_also: #GstGsSrc
 *
 * Write incoming data to a series of sequentially-named remote files on a
 * Google Cloud Storage bucket.
 *
 * The object-name property should contain a string with a \%d placeholder
 * that will be substituted with the index for each filename.
 *
 * If the #GstGsSink:post-messages property is %TRUE, it sends an application
 * message named `GstGsSink` after writing each buffer.
 *
 * The message's structure contains these fields:
 *
 * * #gchararray `filename`: the filename where the buffer was written.
 * * #gchararray `date`: the date of the current buffer, NULL if no start date
 * is provided.
 * * #gint `index`: index of the buffer.
 * * #GstClockTime `timestamp`: the timestamp of the buffer.
 * * #GstClockTime `stream-time`: the stream time of the buffer.
 * * #GstClockTime `running-time`: the running_time of the buffer.
 * * #GstClockTime `duration`: the duration of the buffer.
 * * #guint64 `offset`: the offset of the buffer that triggered the message.
 * * #guint64 `offset-end`: the offset-end of the buffer that triggered the
 * message.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=15 ! pngenc ! gssink
 * object-name="mypath/myframes/frame%05d.png" bucket-name="mybucket"
 * next-file=buffer post-messages=true
 * ```
 * ### Upload 15 png images into gs://mybucket/mypath/myframes/ where the file
 * names are frame00000.png, frame00001.png, ..., frame00014.png
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=6 ! video/x-raw, framerate=2/1 !
 * pngenc ! gssink start-date="2020-04-16T08:55:03Z"
 * object-name="mypath/myframes/im_%s_%03d.png" bucket-name="mybucket"
 * next-file=buffer post-messages=true
 * ```
 * ### Upload png 6 images into gs://mybucket/mypath/myframes/ where the file
 * names are im_2020-04-16T08:55:03Z_000.png, im_2020-04-16T08:55:03Z_001.png,
 * im_2020-04-16T08:55:04Z_002.png, im_2020-04-16T08:55:04Z_003.png,
 * im_2020-04-16T08:55:05Z_004.png, im_2020-04-16T08:55:05Z_005.png.
 * ```
 * gst-launch-1.0 filesrc location=some_video.mp4 ! gssink
 * object-name="mypath/myvideos/video.mp4" bucket-name="mybucket" next-file=none
 * ```
 * ### Upload any stream as a single file into Google Cloud Storage. Similar as
 * filesink in this case. The file is then accessible from:
 * gs://mybucket/mypath/myvideos/video.mp4
 *
 * See also: #GstGsSrc
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgscommon.h"
#include "gstgssink.h"

#include <algorithm>

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(gst_gs_sink_debug);
#define GST_CAT_DEFAULT gst_gs_sink_debug

#define DEFAULT_INDEX 0
#define DEFAULT_NEXT_FILE GST_GS_SINK_NEXT_BUFFER
#define DEFAULT_OBJECT_NAME "%s_%05d"
#define DEFAULT_POST_MESSAGES FALSE

namespace gcs = google::cloud::storage;

enum {
  PROP_0,
  PROP_BUCKET_NAME,
  PROP_OBJECT_NAME,
  PROP_INDEX,
  PROP_POST_MESSAGES,
  PROP_NEXT_FILE,
  PROP_SERVICE_ACCOUNT_EMAIL,
  PROP_START_DATE,
  PROP_SERVICE_ACCOUNT_CREDENTIALS,
  PROP_METADATA,
  PROP_CONTENT_TYPE,
};

class GSWriteStream;

struct _GstGsSink {
  GstBaseSink parent;

  std::unique_ptr<google::cloud::storage::Client> gcs_client;
  std::unique_ptr<GSWriteStream> gcs_stream;
  gchar* service_account_email;
  gchar* service_account_credentials;
  gchar* bucket_name;
  gchar* object_name;
  gchar* start_date_str;
  GDateTime* start_date;
  gint index;
  gboolean post_messages;
  GstGsSinkNext next_file;
  const gchar* content_type;
  gchar* content_type_prop;
  size_t nb_percent_format;
  gboolean percent_s_is_first;
  GstStructure* metadata;
};

static void gst_gs_sink_finalize(GObject* object);

static void gst_gs_sink_set_property(GObject* object,
                                     guint prop_id,
                                     const GValue* value,
                                     GParamSpec* pspec);
static void gst_gs_sink_get_property(GObject* object,
                                     guint prop_id,
                                     GValue* value,
                                     GParamSpec* pspec);

static gboolean gst_gs_sink_start(GstBaseSink* bsink);
static gboolean gst_gs_sink_stop(GstBaseSink* sink);
static GstFlowReturn gst_gs_sink_render(GstBaseSink* sink, GstBuffer* buffer);
static GstFlowReturn gst_gs_sink_render_list(GstBaseSink* sink,
                                             GstBufferList* buffer_list);
static gboolean gst_gs_sink_set_caps(GstBaseSink* sink, GstCaps* caps);
static gboolean gst_gs_sink_event(GstBaseSink* sink, GstEvent* event);

#define GST_TYPE_GS_SINK_NEXT (gst_gs_sink_next_get_type())
static GType gst_gs_sink_next_get_type(void) {
  static GType gs_sink_next_type = 0;
  static const GEnumValue next_types[] = {
      {GST_GS_SINK_NEXT_BUFFER, "New file for each buffer", "buffer"},
      {GST_GS_SINK_NEXT_NONE, "Only one file, no next file", "none"},
      {0, NULL, NULL}};

  if (!gs_sink_next_type) {
    gs_sink_next_type = g_enum_register_static("GstGsSinkNext", next_types);
  }

  return gs_sink_next_type;
}

#define gst_gs_sink_parent_class parent_class
G_DEFINE_TYPE(GstGsSink, gst_gs_sink, GST_TYPE_BASE_SINK);
GST_ELEMENT_REGISTER_DEFINE(gssink, "gssink", GST_RANK_NONE, GST_TYPE_GS_SINK)

class GSWriteStream {
 public:
  GSWriteStream(google::cloud::storage::Client& client,
                const char* bucket_name,
                const char* object_name,
                gcs::ObjectMetadata metadata)
      : gcs_stream_(client.WriteObject(bucket_name,
                                       object_name,
                                       gcs::WithObjectMetadata(metadata))) {}
  ~GSWriteStream() { gcs_stream_.Close(); }

  gcs::ObjectWriteStream& stream() { return gcs_stream_; }

 private:
  gcs::ObjectWriteStream gcs_stream_;
};

static void gst_gs_sink_class_init(GstGsSinkClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass* gstelement_class = GST_ELEMENT_CLASS(klass);
  GstBaseSinkClass* gstbasesink_class = GST_BASE_SINK_CLASS(klass);

  gobject_class->set_property = gst_gs_sink_set_property;
  gobject_class->get_property = gst_gs_sink_get_property;

  /**
   * GstGsSink:bucket-name:
   *
   * Name of the Google Cloud Storage bucket.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_BUCKET_NAME,
      g_param_spec_string(
          "bucket-name", "Bucket Name", "Google Cloud Storage Bucket Name",
          NULL, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstGsSink:object-name:
   *
   * Full path name of the remote file.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_OBJECT_NAME,
      g_param_spec_string(
          "object-name", "Object Name", "Full path name of the remote file",
          DEFAULT_OBJECT_NAME,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstGsSink:index:
   *
   * Index to use with location property to create file names.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_INDEX,
      g_param_spec_int(
          "index", "Index",
          "Index to use with location property to create file names.  The "
          "index is incremented by one for each buffer written.",
          0, G_MAXINT, DEFAULT_INDEX,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstGsSink:post-messages:
   *
   * Post a message on the GstBus for each file.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_POST_MESSAGES,
      g_param_spec_boolean(
          "post-messages", "Post Messages",
          "Post a message for each file with information of the buffer",
          DEFAULT_POST_MESSAGES,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  /**
   * GstGsSink:next-file:
   *
   * A #GstGsSinkNext that specifies when to start a new file.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_NEXT_FILE,
      g_param_spec_enum(
          "next-file", "Next File", "When to start a new file",
          GST_TYPE_GS_SINK_NEXT, DEFAULT_NEXT_FILE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstGsSink:service-account-email:
   *
   * Service Account Email to use for credentials.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_SERVICE_ACCOUNT_EMAIL,
      g_param_spec_string(
          "service-account-email", "Service Account Email",
          "Service Account Email to use for credentials", NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  /**
   * GstGsSink:service-account-credentials:
   *
   * Service Account Credentials as a JSON string to use for credentials.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_SERVICE_ACCOUNT_CREDENTIALS,
      g_param_spec_string(
          "service-account-credentials", "Service Account Credentials",
          "Service Account Credentials as a JSON string to use for credentials",
          NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  /**
   * GstGsSink:start-date:
   *
   * Start date in iso8601 format.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_START_DATE,
      g_param_spec_string(
          "start-date", "Start Date", "Start date in iso8601 format", NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  /**
   * GstGsSink:metadata:
   *
   * A map of metadata to store with the object; field values need to be
   * convertible to strings.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_METADATA,
      g_param_spec_boxed(
          "metadata", "Metadata",
          "A map of metadata to store with the object; field values need to be "
          "convertible to strings.",
          GST_TYPE_STRUCTURE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  /**
   * GstGsSink:content-type:
   *
   * The Content-Type of the object.
   * If not set we use the name of the input caps.
   *
   * Since: 1.22
   */
  g_object_class_install_property(
      gobject_class, PROP_CONTENT_TYPE,
      g_param_spec_string(
          "content-type", "Content-Type",
          "The Content-Type of the object",
          NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  gobject_class->finalize = gst_gs_sink_finalize;

  gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_gs_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_gs_sink_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_gs_sink_render);
  gstbasesink_class->render_list = GST_DEBUG_FUNCPTR(gst_gs_sink_render_list);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_gs_sink_set_caps);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_gs_sink_event);

  GST_DEBUG_CATEGORY_INIT(gst_gs_sink_debug, "gssink", 0, "gssink element");

  gst_element_class_add_static_pad_template(gstelement_class, &sinktemplate);
  gst_element_class_set_static_metadata(
      gstelement_class, "Google Cloud Storage Sink", "Sink/File",
      "Write buffers to a sequentially named set of files on Google Cloud "
      "Storage",
      "Julien Isorce <jisorce@oblong.com>");
}

static void gst_gs_sink_init(GstGsSink* sink) {
  sink->gcs_client = nullptr;
  sink->gcs_stream = nullptr;
  sink->index = DEFAULT_INDEX;
  sink->post_messages = DEFAULT_POST_MESSAGES;
  sink->service_account_email = NULL;
  sink->service_account_credentials = NULL;
  sink->bucket_name = NULL;
  sink->object_name = g_strdup(DEFAULT_OBJECT_NAME);
  sink->start_date_str = NULL;
  sink->start_date = NULL;
  sink->next_file = DEFAULT_NEXT_FILE;
  sink->content_type = NULL;
  sink->content_type_prop = NULL;
  sink->nb_percent_format = 0;
  sink->percent_s_is_first = FALSE;

  gst_base_sink_set_sync(GST_BASE_SINK(sink), FALSE);
}

static void gst_gs_sink_finalize(GObject* object) {
  GstGsSink* sink = GST_GS_SINK(object);

  sink->gcs_client = nullptr;
  sink->gcs_stream = nullptr;
  g_free(sink->service_account_email);
  sink->service_account_email = NULL;
  g_free(sink->service_account_credentials);
  sink->service_account_credentials = NULL;
  g_free(sink->bucket_name);
  sink->bucket_name = NULL;
  g_free(sink->object_name);
  sink->object_name = NULL;
  g_free(sink->start_date_str);
  sink->start_date_str = NULL;
  if (sink->start_date) {
    g_date_time_unref(sink->start_date);
    sink->start_date = NULL;
  }
  sink->content_type = NULL;
  g_clear_pointer(&sink->content_type_prop, g_free);
  g_clear_pointer(&sink->metadata, gst_structure_free);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean gst_gs_sink_set_object_name(GstGsSink* sink,
                                            const gchar* object_name) {
  g_free(sink->object_name);
  sink->object_name = NULL;
  sink->nb_percent_format = 0;
  sink->percent_s_is_first = FALSE;

  if (!object_name) {
    GST_ERROR_OBJECT(sink, "Object name is null");
    return FALSE;
  }

  const std::string name(object_name);
  sink->nb_percent_format = std::count(name.begin(), name.end(), '%');
  if (sink->nb_percent_format > 2) {
    GST_ERROR_OBJECT(sink, "Object name has too many formats");
    return FALSE;
  }

  const size_t delimiter_percent_s = name.find("%s");
  if (delimiter_percent_s == std::string::npos) {
    if (sink->nb_percent_format == 2) {
      GST_ERROR_OBJECT(sink, "Object name must have just one number format");
      return FALSE;
    }
    sink->object_name = g_strdup(object_name);
    return TRUE;
  }

  const size_t delimiter_percent = name.find_first_of('%');
  if (delimiter_percent_s == delimiter_percent) {
    sink->percent_s_is_first = TRUE;

    if (name.find("%s", delimiter_percent_s + 1) != std::string::npos) {
      GST_ERROR_OBJECT(sink, "Object name expect max one string format");
      return FALSE;
    }
  }

  sink->object_name = g_strdup(object_name);

  return TRUE;
}

static void gst_gs_sink_set_property(GObject* object,
                                     guint prop_id,
                                     const GValue* value,
                                     GParamSpec* pspec) {
  GstGsSink* sink = GST_GS_SINK(object);

  switch (prop_id) {
    case PROP_BUCKET_NAME:
      g_free(sink->bucket_name);
      sink->bucket_name = g_strdup(g_value_get_string(value));
      break;
    case PROP_OBJECT_NAME:
      gst_gs_sink_set_object_name(sink, g_value_get_string(value));
      break;
    case PROP_INDEX:
      sink->index = g_value_get_int(value);
      break;
    case PROP_POST_MESSAGES:
      sink->post_messages = g_value_get_boolean(value);
      break;
    case PROP_NEXT_FILE:
      sink->next_file = (GstGsSinkNext)g_value_get_enum(value);
      break;
    case PROP_SERVICE_ACCOUNT_EMAIL:
      g_free(sink->service_account_email);
      sink->service_account_email = g_strdup(g_value_get_string(value));
      break;
    case PROP_SERVICE_ACCOUNT_CREDENTIALS:
      g_free(sink->service_account_credentials);
      sink->service_account_credentials = g_strdup(g_value_get_string(value));
      break;
    case PROP_START_DATE:
      g_free(sink->start_date_str);
      if (sink->start_date)
        g_date_time_unref(sink->start_date);
      sink->start_date_str = g_strdup(g_value_get_string(value));
      sink->start_date =
          g_date_time_new_from_iso8601(sink->start_date_str, NULL);
      if (!sink->start_date) {
        GST_ERROR_OBJECT(sink, "Failed to parse start date %s",
                         sink->start_date_str);
        g_free(sink->start_date_str);
        sink->start_date_str = NULL;
      }
      break;
    case PROP_METADATA:
      g_clear_pointer(&sink->metadata, gst_structure_free);
      sink->metadata = (GstStructure*)g_value_dup_boxed(value);
      break;
    case PROP_CONTENT_TYPE:
      g_clear_pointer(&sink->content_type_prop, g_free);
      sink->content_type_prop = g_value_dup_string(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_gs_sink_get_property(GObject* object,
                                     guint prop_id,
                                     GValue* value,
                                     GParamSpec* pspec) {
  GstGsSink* sink = GST_GS_SINK(object);

  switch (prop_id) {
    case PROP_BUCKET_NAME:
      g_value_set_string(value, sink->bucket_name);
      break;
    case PROP_OBJECT_NAME:
      g_value_set_string(value, sink->object_name);
      break;
    case PROP_INDEX:
      g_value_set_int(value, sink->index);
      break;
    case PROP_POST_MESSAGES:
      g_value_set_boolean(value, sink->post_messages);
      break;
    case PROP_NEXT_FILE:
      g_value_set_enum(value, sink->next_file);
      break;
    case PROP_SERVICE_ACCOUNT_EMAIL:
      g_value_set_string(value, sink->service_account_email);
      break;
    case PROP_SERVICE_ACCOUNT_CREDENTIALS:
      g_value_set_string(value, sink->service_account_credentials);
      break;
    case PROP_START_DATE:
      g_value_set_string(value, sink->start_date_str);
      break;
    case PROP_METADATA:
      g_value_set_boxed(value, sink->metadata);
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string(value, sink->content_type_prop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static gboolean gst_gs_sink_start(GstBaseSink* bsink) {
  GstGsSink* sink = GST_GS_SINK(bsink);
  GError* err = NULL;

  if (!sink->bucket_name) {
    GST_ELEMENT_ERROR(sink, RESOURCE, SETTINGS, ("Bucket name is required"),
                      GST_ERROR_SYSTEM);
    return FALSE;
  }

  if (!sink->object_name) {
    GST_ELEMENT_ERROR(sink, RESOURCE, SETTINGS, ("Object name is required"),
                      GST_ERROR_SYSTEM);
    return FALSE;
  }

  sink->content_type = "";

  sink->gcs_client = gst_gs_create_client(
      sink->service_account_email, sink->service_account_credentials, &err);
  if (err) {
    GST_ELEMENT_ERROR(sink, RESOURCE, OPEN_READ,
                      ("Could not create client (%s)", err->message),
                      GST_ERROR_SYSTEM);
    g_clear_error(&err);
    return FALSE;
  }

  GST_INFO_OBJECT(sink, "Using bucket name (%s) and object name (%s)",
                  sink->bucket_name, sink->object_name);

  return TRUE;
}

static gboolean gst_gs_sink_stop(GstBaseSink* bsink) {
  GstGsSink* sink = GST_GS_SINK(bsink);

  sink->gcs_client = nullptr;
  sink->gcs_stream = nullptr;
  sink->content_type = NULL;

  return TRUE;
}

static void gst_gs_sink_post_message_full(GstGsSink* sink,
                                          GstClockTime timestamp,
                                          GstClockTime duration,
                                          GstClockTime offset,
                                          GstClockTime offset_end,
                                          GstClockTime running_time,
                                          GstClockTime stream_time,
                                          const char* filename,
                                          const gchar* date) {
  GstStructure* s;

  if (!sink->post_messages)
    return;

  s = gst_structure_new("GstGsSink", "filename", G_TYPE_STRING, filename,
                        "date", G_TYPE_STRING, date, "index", G_TYPE_INT,
                        sink->index, "timestamp", G_TYPE_UINT64, timestamp,
                        "stream-time", G_TYPE_UINT64, stream_time,
                        "running-time", G_TYPE_UINT64, running_time, "duration",
                        G_TYPE_UINT64, duration, "offset", G_TYPE_UINT64,
                        offset, "offset-end", G_TYPE_UINT64, offset_end, NULL);

  gst_element_post_message(GST_ELEMENT_CAST(sink),
                           gst_message_new_element(GST_OBJECT_CAST(sink), s));
}

static void gst_gs_sink_post_message_from_time(GstGsSink* sink,
                                               GstClockTime timestamp,
                                               GstClockTime duration,
                                               const char* filename) {
  GstClockTime running_time, stream_time;
  guint64 offset, offset_end;
  GstSegment* segment;
  GstFormat format;

  if (!sink->post_messages)
    return;

  segment = &GST_BASE_SINK(sink)->segment;
  format = segment->format;

  offset = -1;
  offset_end = -1;

  running_time = gst_segment_to_running_time(segment, format, timestamp);
  stream_time = gst_segment_to_stream_time(segment, format, timestamp);

  gst_gs_sink_post_message_full(sink, timestamp, duration, offset, offset_end,
                                running_time, stream_time, filename, NULL);
}

static void gst_gs_sink_post_message(GstGsSink* sink,
                                     GstBuffer* buffer,
                                     const char* filename,
                                     const char* date) {
  GstClockTime duration, timestamp;
  GstClockTime running_time, stream_time;
  guint64 offset, offset_end;
  GstSegment* segment;
  GstFormat format;

  if (!sink->post_messages)
    return;

  segment = &GST_BASE_SINK(sink)->segment;
  format = segment->format;

  timestamp = GST_BUFFER_PTS(buffer);
  duration = GST_BUFFER_DURATION(buffer);
  offset = GST_BUFFER_OFFSET(buffer);
  offset_end = GST_BUFFER_OFFSET_END(buffer);

  running_time = gst_segment_to_running_time(segment, format, timestamp);
  stream_time = gst_segment_to_stream_time(segment, format, timestamp);

  gst_gs_sink_post_message_full(sink, timestamp, duration, offset, offset_end,
                                running_time, stream_time, filename, date);
}

struct AddMetadataIter {
  GstGsSink* sink;
  gcs::ObjectMetadata* metadata;
};

static gboolean add_metadata_foreach(GQuark field_id,
                                     const GValue* value,
                                     gpointer user_data) {
  struct AddMetadataIter* it = (struct AddMetadataIter*)user_data;
  GValue svalue = G_VALUE_INIT;

  g_value_init(&svalue, G_TYPE_STRING);

  if (g_value_transform(value, &svalue)) {
    const gchar* key = g_quark_to_string(field_id);
    const gchar* value = g_value_get_string(&svalue);

    GST_LOG_OBJECT(it->sink, "metadata '%s' -> '%s'", key, value);
    it->metadata->upsert_metadata(key, value);
  } else {
    GST_WARNING_OBJECT(it->sink, "Failed to convert metadata '%s' to string",
                       g_quark_to_string(field_id));
  }

  g_value_unset(&svalue);
  return TRUE;
}

static GstFlowReturn gst_gs_sink_write_buffer(GstGsSink* sink,
                                              GstBuffer* buffer) {
  GstMapInfo map = {0};
  gchar* object_name = NULL;
  gchar* buffer_date = NULL;
  const gchar* content_type;

  if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
    return GST_FLOW_ERROR;

  if (sink->content_type_prop)
    content_type = sink->content_type_prop;
  else
    content_type = sink->content_type;

  gcs::ObjectMetadata metadata =
      gcs::ObjectMetadata().set_content_type(content_type);

  if (sink->metadata) {
    struct AddMetadataIter it = {sink, &metadata};

    gst_structure_foreach(sink->metadata, add_metadata_foreach, &it);
  }

  switch (sink->next_file) {
    case GST_GS_SINK_NEXT_BUFFER: {
      // Get buffer date if needed.
      if (sink->start_date) {
        if (sink->nb_percent_format != 2) {
          GST_ERROR_OBJECT(sink, "Object name expects date and index");
          gst_buffer_unmap(buffer, &map);
          return GST_FLOW_ERROR;
        }

        if (!gst_gs_get_buffer_date(buffer, sink->start_date, &buffer_date)) {
          GST_ERROR_OBJECT(sink, "Could not get buffer date %s", object_name);
          gst_buffer_unmap(buffer, &map);
          return GST_FLOW_ERROR;
        }

        if (sink->percent_s_is_first) {
          object_name =
              g_strdup_printf(sink->object_name, buffer_date, sink->index);
        } else {
          object_name =
              g_strdup_printf(sink->object_name, sink->index, buffer_date);
        }
      } else {
        if (sink->nb_percent_format != 1) {
          GST_ERROR_OBJECT(sink, "Object name expects only an index");
          gst_buffer_unmap(buffer, &map);
          return GST_FLOW_ERROR;
        }

        object_name = g_strdup_printf(sink->object_name, sink->index);
      }

      GST_INFO_OBJECT(sink, "Writing %" G_GSIZE_FORMAT " bytes", map.size);

      gcs::ObjectWriteStream gcs_stream = sink->gcs_client->WriteObject(
          sink->bucket_name, object_name, gcs::WithObjectMetadata(metadata));

      gcs_stream.write(reinterpret_cast<const char*>(map.data), map.size);
      if (gcs_stream.fail()) {
        GST_WARNING_OBJECT(sink, "Failed to write to %s", object_name);
      }
      gcs_stream.Close();

      google::cloud::StatusOr<gcs::ObjectMetadata> object_metadata =
          sink->gcs_client->GetObjectMetadata(sink->bucket_name, object_name);
      if (!object_metadata) {
        GST_ERROR_OBJECT(
            sink, "Could not get object metadata for object %s (%s)",
            object_name, object_metadata.status().message().c_str());
        gst_buffer_unmap(buffer, &map);
        g_free(object_name);
        g_free(buffer_date);
        return GST_FLOW_ERROR;
      }

      GST_INFO_OBJECT(sink, "Wrote object %s of size %" G_GUINT64_FORMAT "\n",
                      object_name, object_metadata->size());

      gst_gs_sink_post_message(sink, buffer, object_name, buffer_date);
      g_free(object_name);
      g_free(buffer_date);
      ++sink->index;
      break;
    }
    case GST_GS_SINK_NEXT_NONE: {
      if (!sink->gcs_stream) {
        GST_INFO_OBJECT(sink, "Opening %s", sink->object_name);
        sink->gcs_stream = std::make_unique<GSWriteStream>(
            *sink->gcs_client.get(), sink->bucket_name, sink->object_name,
            metadata);

        if (!sink->gcs_stream->stream().IsOpen()) {
          GST_ELEMENT_ERROR(
              sink, RESOURCE, OPEN_READ,
              ("Could not create write stream (%s)",
               sink->gcs_stream->stream().last_status().message().c_str()),
              GST_ERROR_SYSTEM);
          gst_buffer_unmap(buffer, &map);
          return GST_FLOW_OK;
        }
      }

      GST_INFO_OBJECT(sink, "Writing %" G_GSIZE_FORMAT " bytes", map.size);

      gcs::ObjectWriteStream& stream = sink->gcs_stream->stream();
      stream.write(reinterpret_cast<const char*>(map.data), map.size);
      if (stream.fail()) {
        GST_WARNING_OBJECT(sink, "Failed to write to %s", object_name);
      }
      break;
    }
    default:
      g_assert_not_reached();
  }

  gst_buffer_unmap(buffer, &map);
  return GST_FLOW_OK;
}

static GstFlowReturn gst_gs_sink_render(GstBaseSink* bsink, GstBuffer* buffer) {
  GstGsSink* sink = GST_GS_SINK(bsink);
  GstFlowReturn flow = GST_FLOW_OK;

  flow = gst_gs_sink_write_buffer(sink, buffer);
  return flow;
}

static gboolean buffer_list_copy_data(GstBuffer** buf,
                                      guint idx,
                                      gpointer data) {
  GstBuffer* dest = GST_BUFFER_CAST(data);
  guint num, i;

  if (idx == 0)
    gst_buffer_copy_into(dest, *buf, GST_BUFFER_COPY_METADATA, 0, -1);

  num = gst_buffer_n_memory(*buf);
  for (i = 0; i < num; ++i) {
    GstMemory* mem;

    mem = gst_buffer_get_memory(*buf, i);
    gst_buffer_append_memory(dest, mem);
  }

  return TRUE;
}

/* Our assumption for now is that the buffers in a buffer list should always
 * end up in the same file. If someone wants different behaviour, they'll just
 * have to add a property for that. */
static GstFlowReturn gst_gs_sink_render_list(GstBaseSink* sink,
                                             GstBufferList* list) {
  GstBuffer* buf;
  guint size;

  size = gst_buffer_list_calculate_size(list);
  GST_LOG_OBJECT(sink, "total size of buffer list %p: %u", list, size);

  /* copy all buffers in the list into one single buffer, so we can use
   * the normal render function (FIXME: optimise to avoid the memcpy) */
  buf = gst_buffer_new();
  gst_buffer_list_foreach(list, buffer_list_copy_data, buf);
  g_assert(gst_buffer_get_size(buf) == size);

  gst_gs_sink_render(sink, buf);
  gst_buffer_unref(buf);

  return GST_FLOW_OK;
}

static gboolean gst_gs_sink_set_caps(GstBaseSink* bsink, GstCaps* caps) {
  GstGsSink* sink = GST_GS_SINK(bsink);
  GstStructure* s = gst_caps_get_structure(caps, 0);

  sink->content_type = gst_structure_get_name(s);

  /* TODO: we could automatically convert some caps such as 'video/quicktime,variant=iso' to 'video/mp4' */

  GST_INFO_OBJECT(sink, "Content-Type: caps: %s property: %s", sink->content_type, sink->content_type_prop);

  return TRUE;
}

static gboolean gst_gs_sink_event(GstBaseSink* bsink, GstEvent* event) {
  GstGsSink* sink = GST_GS_SINK(bsink);

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_EOS:
      if (sink->gcs_stream) {
        sink->gcs_stream = nullptr;
        gst_gs_sink_post_message_from_time(
            sink, GST_BASE_SINK(sink)->segment.position, -1, sink->object_name);
      }
      break;
    default:
      break;
  }

  return GST_BASE_SINK_CLASS(parent_class)->event(bsink, event);
}
