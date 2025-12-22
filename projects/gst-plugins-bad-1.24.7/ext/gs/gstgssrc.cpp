/* GStreamer
 * Copyright (C) 2020 Julien Isorce <jisorce@oblong.com>
 *
 * gstgssrc.c:
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
 * SECTION:element-gssrc
 * @title: gssrc
 * @see_also: #GstGsSrc
 *
 * Read data from a file in a Google Cloud Storage.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 gssrc location=gs://mybucket/myvideo.mkv ! decodebin !
 * glimagesink
 * ```
 * ### Play a video from a Google Cloud Storage.
 * ```
 * gst-launch-1.0 gssrc location=gs://mybucket/myvideo.mkv ! decodebin ! navseek
 * seek-offset=10 ! glimagesink
 * ```
 * ### Play a video from a Google Cloud Storage and seek using the keyboard
 * from the terminal.
 *
 * See also: #GstGsSink
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgscommon.h"
#include "gstgssrc.h"

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(gst_gs_src_debug);
#define GST_CAT_DEFAULT gst_gs_src_debug

enum { LAST_SIGNAL };

// https://github.com/googleapis/google-cloud-cpp/issues/2657
#define DEFAULT_BLOCKSIZE 3 * 1024 * 1024 / 2

enum {
  PROP_0,
  PROP_LOCATION,
  PROP_SERVICE_ACCOUNT_EMAIL,
  PROP_SERVICE_ACCOUNT_CREDENTIALS
};

class GSReadStream;

struct _GstGsSrc {
  GstBaseSrc parent;

  std::unique_ptr<google::cloud::storage::Client> gcs_client;
  std::unique_ptr<GSReadStream> gcs_stream;
  gchar* uri;
  gchar* service_account_email;
  gchar* service_account_credentials;
  std::string bucket_name;
  std::string object_name;
  guint64 read_position;
  guint64 object_size;
};

static void gst_gs_src_finalize(GObject* object);

static void gst_gs_src_set_property(GObject* object,
                                    guint prop_id,
                                    const GValue* value,
                                    GParamSpec* pspec);
static void gst_gs_src_get_property(GObject* object,
                                    guint prop_id,
                                    GValue* value,
                                    GParamSpec* pspec);

static gboolean gst_gs_src_start(GstBaseSrc* basesrc);
static gboolean gst_gs_src_stop(GstBaseSrc* basesrc);

static gboolean gst_gs_src_is_seekable(GstBaseSrc* src);
static gboolean gst_gs_src_get_size(GstBaseSrc* src, guint64* size);
static GstFlowReturn gst_gs_src_fill(GstBaseSrc* src,
                                     guint64 offset,
                                     guint length,
                                     GstBuffer* buf);
static gboolean gst_gs_src_query(GstBaseSrc* src, GstQuery* query);

static void gst_gs_src_uri_handler_init(gpointer g_iface, gpointer iface_data);

#define _do_init                                                            \
  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gst_gs_src_uri_handler_init); \
  GST_DEBUG_CATEGORY_INIT(gst_gs_src_debug, "gssrc", 0, "gssrc element");
#define gst_gs_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstGsSrc, gst_gs_src, GST_TYPE_BASE_SRC, _do_init);
GST_ELEMENT_REGISTER_DEFINE(gssrc, "gssrc", GST_RANK_NONE, GST_TYPE_GS_SRC)

namespace gcs = google::cloud::storage;

class GSReadStream {
 public:
  GSReadStream(GstGsSrc* src,
               const std::int64_t start = 0,
               const std::int64_t end = -1)
      : gcs_stream_(src->gcs_client->ReadObject(src->bucket_name,
                                                src->object_name,
                                                gcs::ReadFromOffset(start))) {}
  ~GSReadStream() { gcs_stream_.Close(); }

  gcs::ObjectReadStream& stream() { return gcs_stream_; }

 private:
  gcs::ObjectReadStream gcs_stream_;
};

static void gst_gs_src_class_init(GstGsSrcClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass* gstelement_class = GST_ELEMENT_CLASS(klass);
  GstBaseSrcClass* gstbasesrc_class = GST_BASE_SRC_CLASS(klass);

  gobject_class->set_property = gst_gs_src_set_property;
  gobject_class->get_property = gst_gs_src_get_property;

  /**
   * GstGsSink:location:
   *
   * Name of the Google Cloud Storage bucket.
   *
   * Since: 1.20
   */
  g_object_class_install_property(
      gobject_class, PROP_LOCATION,
      g_param_spec_string(
          "location", "File Location", "Location of the file to read", NULL,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                        GST_PARAM_MUTABLE_READY)));

  /**
   * GstGsSrc:service-account-email:
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
   * GstGsSrc:service-account-credentials:
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

  gobject_class->finalize = gst_gs_src_finalize;

  gst_element_class_set_static_metadata(
      gstelement_class, "Google Cloud Storage Source", "Source/File",
      "Read from arbitrary point from a file in a Google Cloud Storage",
      "Julien Isorce <jisorce@oblong.com>");
  gst_element_class_add_static_pad_template(gstelement_class, &srctemplate);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_gs_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_gs_src_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR(gst_gs_src_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR(gst_gs_src_get_size);
  gstbasesrc_class->fill = GST_DEBUG_FUNCPTR(gst_gs_src_fill);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR(gst_gs_src_query);
}

static void gst_gs_src_init(GstGsSrc* src) {
  src->gcs_stream = nullptr;
  src->uri = NULL;
  src->service_account_email = NULL;
  src->service_account_credentials = NULL;
  src->read_position = 0;
  src->object_size = 0;

  gst_base_src_set_blocksize(GST_BASE_SRC(src), DEFAULT_BLOCKSIZE);
  gst_base_src_set_dynamic_size(GST_BASE_SRC(src), FALSE);
  gst_base_src_set_live(GST_BASE_SRC(src), FALSE);
}

static void gst_gs_src_finalize(GObject* object) {
  GstGsSrc* src = GST_GS_SRC(object);

  g_free(src->uri);
  src->uri = NULL;
  g_free(src->service_account_email);
  src->service_account_email = NULL;
  g_free(src->service_account_credentials);
  src->service_account_credentials = NULL;
  src->read_position = 0;
  src->object_size = 0;

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static gboolean gst_gs_src_set_location(GstGsSrc* src,
                                        const gchar* location,
                                        GError** err) {
  GstState state = GST_STATE_NULL;
  std::string filepath = location;
  size_t delimiter = std::string::npos;

  // The element must be stopped in order to do this.
  GST_OBJECT_LOCK(src);
  state = GST_STATE(src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK(src);

  g_free(src->uri);
  src->uri = NULL;

  if (location) {
    if (g_str_has_prefix(location, "gs://")) {
      src->uri = g_strdup(location);
      filepath = filepath.substr(5);
    } else {
      src->uri = g_strdup_printf("gs://%s", location);
      filepath = location;
    }

    delimiter = filepath.find_first_of('/');
    if (delimiter == std::string::npos)
      goto wrong_location;

    std::string bucket_name = filepath.substr(0, delimiter);
    src->bucket_name = bucket_name;
    src->object_name = filepath.substr(delimiter + 1);

    GST_INFO_OBJECT(src, "uri is %s", src->uri);
    GST_INFO_OBJECT(src, "bucket name is %s", src->bucket_name.c_str());
    GST_INFO_OBJECT(src, "object name is %s", src->object_name.c_str());
  }
  g_object_notify(G_OBJECT(src), "location");

  return TRUE;

  // ERROR.
wrong_state : {
  g_warning(
      "Changing the `location' property on gssrc when a file is open"
      "is not supported.");
  if (err)
    g_set_error(
        err, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the `location' property on gssrc when a file is open is "
        "not supported.");
  GST_OBJECT_UNLOCK(src);
  return FALSE;
}
wrong_location : {
  if (err)
    g_set_error(err, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
                "Failed to find a bucket name");
  GST_OBJECT_UNLOCK(src);
  return FALSE;
}
}

static gboolean gst_gs_src_set_service_account_email(
    GstGsSrc* src,
    const gchar* service_account_email) {
  if (GST_STATE(src) == GST_STATE_PLAYING ||
      GST_STATE(src) == GST_STATE_PAUSED) {
    GST_WARNING_OBJECT(src,
                       "Setting a new service account email not supported in "
                       "PLAYING or PAUSED state");
    return FALSE;
  }

  GST_OBJECT_LOCK(src);
  g_free(src->service_account_email);
  src->service_account_email = NULL;

  if (service_account_email)
    src->service_account_email = g_strdup(service_account_email);

  GST_OBJECT_UNLOCK(src);

  return TRUE;
}

static gboolean gst_gs_src_set_service_account_credentials(
    GstGsSrc* src,
    const gchar* service_account_credentials) {
  if (GST_STATE(src) == GST_STATE_PLAYING ||
      GST_STATE(src) == GST_STATE_PAUSED) {
    GST_WARNING_OBJECT(
        src,
        "Setting a new service account credentials not supported in "
        "PLAYING or PAUSED state");
    return FALSE;
  }

  GST_OBJECT_LOCK(src);
  g_free(src->service_account_credentials);
  src->service_account_credentials = NULL;

  if (service_account_credentials)
    src->service_account_credentials = g_strdup(service_account_credentials);

  GST_OBJECT_UNLOCK(src);

  return TRUE;
}

static void gst_gs_src_set_property(GObject* object,
                                    guint prop_id,
                                    const GValue* value,
                                    GParamSpec* pspec) {
  GstGsSrc* src = GST_GS_SRC(object);

  g_return_if_fail(GST_IS_GS_SRC(object));

  switch (prop_id) {
    case PROP_LOCATION:
      gst_gs_src_set_location(src, g_value_get_string(value), NULL);
      break;
    case PROP_SERVICE_ACCOUNT_EMAIL:
      gst_gs_src_set_service_account_email(src, g_value_get_string(value));
      break;
    case PROP_SERVICE_ACCOUNT_CREDENTIALS:
      gst_gs_src_set_service_account_credentials(src,
                                                 g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_gs_src_get_property(GObject* object,
                                    guint prop_id,
                                    GValue* value,
                                    GParamSpec* pspec) {
  GstGsSrc* src = GST_GS_SRC(object);

  g_return_if_fail(GST_IS_GS_SRC(object));

  switch (prop_id) {
    case PROP_LOCATION:
      GST_OBJECT_LOCK(src);
      g_value_set_string(value, src->uri);
      GST_OBJECT_UNLOCK(src);
      break;
    case PROP_SERVICE_ACCOUNT_EMAIL:
      GST_OBJECT_LOCK(src);
      g_value_set_string(value, src->service_account_email);
      GST_OBJECT_UNLOCK(src);
      break;
    case PROP_SERVICE_ACCOUNT_CREDENTIALS:
      GST_OBJECT_LOCK(src);
      g_value_set_string(value, src->service_account_credentials);
      GST_OBJECT_UNLOCK(src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static gint gst_gs_read_stream(GstGsSrc* src,
                               guint8* data,
                               const guint64 offset,
                               const guint length) {
  gint gcount = 0;
  gchar* sdata = reinterpret_cast<gchar*>(data);

  gcs::ObjectReadStream& stream = src->gcs_stream->stream();

  while (!stream.eof()) {
    stream.read(sdata, length);
    if (stream.status().ok())
      break;

    GST_ERROR_OBJECT(src, "Restart after (%s)",
                     stream.status().message().c_str());
    src->gcs_stream = std::make_unique<GSReadStream>(src, offset);
  }

  gcount = stream.gcount();

  GST_INFO_OBJECT(src, "Client read %d bytes", gcount);

  return gcount;
}

static GstFlowReturn gst_gs_src_fill(GstBaseSrc* basesrc,
                                     guint64 offset,
                                     guint length,
                                     GstBuffer* buf) {
  GstGsSrc* src = GST_GS_SRC(basesrc);
  guint to_read = 0;
  guint bytes_read = 0;
  gint ret = 0;
  GstMapInfo info = {};
  guint8* data = NULL;

  if (G_UNLIKELY(offset != (guint64)-1 && src->read_position != offset)) {
    src->gcs_stream = std::make_unique<GSReadStream>(src, offset);
    src->read_position = offset;
  }

  if (!gst_buffer_map(buf, &info, GST_MAP_WRITE))
    goto buffer_write_fail;

  data = info.data;

  bytes_read = 0;
  to_read = length;
  while (to_read > 0) {
    GST_INFO_OBJECT(src, "Reading %d bytes at offset 0x%" G_GINT64_MODIFIER "x",
                    to_read, offset + bytes_read);

    ret = gst_gs_read_stream(src, data + bytes_read, offset, to_read);
    if (G_UNLIKELY(ret < 0))
      goto could_not_read;

    if (G_UNLIKELY(ret == 0)) {
      // Push any remaining data.
      if (bytes_read > 0)
        break;
      goto eos;
    }

    to_read -= ret;
    bytes_read += ret;

    src->read_position += ret;
  }

  GST_INFO_OBJECT(
      src, "Read %" G_GUINT32_FORMAT " bytes of %" G_GUINT32_FORMAT " length",
      bytes_read, length);

  gst_buffer_unmap(buf, &info);
  if (bytes_read != length)
    gst_buffer_resize(buf, 0, bytes_read);

  GST_BUFFER_OFFSET(buf) = offset;
  GST_BUFFER_OFFSET_END(buf) = offset + bytes_read;

  return GST_FLOW_OK;

  // ERROR.
could_not_read : {
  GST_ELEMENT_ERROR(src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
  gst_buffer_unmap(buf, &info);
  gst_buffer_resize(buf, 0, 0);
  return GST_FLOW_ERROR;
}
eos : {
  GST_INFO_OBJECT(src, "EOS");
  gst_buffer_unmap(buf, &info);
  gst_buffer_resize(buf, 0, 0);
  return GST_FLOW_EOS;
}
buffer_write_fail : {
  GST_ELEMENT_ERROR(src, RESOURCE, WRITE, (NULL), ("Can't write to buffer"));
  return GST_FLOW_ERROR;
}
}

static gboolean gst_gs_src_is_seekable(GstBaseSrc* basesrc) {
  return TRUE;
}

static gboolean gst_gs_src_get_size(GstBaseSrc* basesrc, guint64* size) {
  GstGsSrc* src = GST_GS_SRC(basesrc);

  *size = src->object_size;

  return TRUE;
}

static gboolean gst_gs_src_start(GstBaseSrc* basesrc) {
  GstGsSrc* src = GST_GS_SRC(basesrc);
  GError* err = NULL;

  src->read_position = 0;
  src->object_size = 0;

  if (src->uri == NULL || src->uri[0] == '\0') {
    GST_ELEMENT_ERROR(src, RESOURCE, NOT_FOUND,
                      ("No uri specified for reading."), (NULL));
    return FALSE;
  }

  GST_INFO_OBJECT(src, "Opening file %s", src->uri);

  src->gcs_client = gst_gs_create_client(
      src->service_account_email, src->service_account_credentials, &err);
  if (err) {
    GST_ELEMENT_ERROR(src, RESOURCE, OPEN_READ,
                      ("Could not create client (%s)", err->message),
                      GST_ERROR_SYSTEM);
    g_clear_error(&err);
    return FALSE;
  }

  GST_INFO_OBJECT(src, "Parsed bucket name (%s) and object name (%s)",
                  src->bucket_name.c_str(), src->object_name.c_str());

  google::cloud::StatusOr<gcs::ObjectMetadata> object_metadata =
      src->gcs_client->GetObjectMetadata(src->bucket_name, src->object_name);
  if (!object_metadata) {
    GST_ELEMENT_ERROR(src, RESOURCE, OPEN_READ,
                      ("Could not get object metadata (%s)",
                       object_metadata.status().message().c_str()),
                      GST_ERROR_SYSTEM);
    return FALSE;
  }

  src->object_size = object_metadata->size();

  GST_INFO_OBJECT(src, "Object size %" G_GUINT64_FORMAT "\n", src->object_size);

  src->gcs_stream = std::make_unique<GSReadStream>(src);

  return TRUE;
}

static gboolean gst_gs_src_stop(GstBaseSrc* basesrc) {
  GstGsSrc* src = GST_GS_SRC(basesrc);

  src->gcs_stream = nullptr;
  src->read_position = 0;
  src->object_size = 0;

  return TRUE;
}

static gboolean gst_gs_src_query(GstBaseSrc* src, GstQuery* query) {
  gboolean ret;

  switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_SCHEDULING: {
      // A pushsrc can by default never operate in pull mode override
      // if you want something different.
      gst_query_set_scheduling(query, GST_SCHEDULING_FLAG_SEQUENTIAL, 1, -1, 0);
      gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);

      ret = TRUE;
      break;
    }
    default:
      ret = GST_BASE_SRC_CLASS(parent_class)->query(src, query);
      break;
  }
  return ret;
}

static GstURIType gst_gs_src_uri_get_type(GType type) {
  return GST_URI_SRC;
}

static const gchar* const* gst_gs_src_uri_get_protocols(GType type) {
  static const gchar* protocols[] = {"gs", NULL};

  return protocols;
}

static gchar* gst_gs_src_uri_get_uri(GstURIHandler* handler) {
  GstGsSrc* src = GST_GS_SRC(handler);

  return g_strdup(src->uri);
}

static gboolean gst_gs_src_uri_set_uri(GstURIHandler* handler,
                                       const gchar* uri,
                                       GError** err) {
  GstGsSrc* src = GST_GS_SRC(handler);

  if (strcmp(uri, "gs://") == 0) {
    // Special case for "gs://" as this is used by some applications
    // to test with gst_element_make_from_uri if there's an element
    // that supports the URI protocol.
    gst_gs_src_set_location(src, NULL, NULL);
    return TRUE;
  }

  return gst_gs_src_set_location(src, uri, err);
}

static void gst_gs_src_uri_handler_init(gpointer g_iface, gpointer iface_data) {
  GstURIHandlerInterface* iface = (GstURIHandlerInterface*)g_iface;

  iface->get_type = gst_gs_src_uri_get_type;
  iface->get_protocols = gst_gs_src_uri_get_protocols;
  iface->get_uri = gst_gs_src_uri_get_uri;
  iface->set_uri = gst_gs_src_uri_set_uri;
}
