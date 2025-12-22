/* GStreamer
 *
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
 *  @author: Thibault Saunier <tsaunier@igalia.com>
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
 * SECTION:element-testsrcbin
 * @title: testsrc
 *
 * This is a simple GstBin source that wraps audiotestsrc/videotestsrc following
 * specification passed in the URI (it implements the #GstURIHandler interface)
 * in the form of `testbin://audio+video` or setting the "stream-types" property
 * with the same format.
 *
 * This element also provides GstStream and GstStreamCollection and thus the
 * element is useful for testing the new playbin3 infrastructure.
 *
 * ## The `uri` format
 *
 * `testbin://<stream1 definition>[+<stream2 definition>]`
 *
 * With **<stream definition>**:
 *
 *  `<media-type>,<element-properties>,[caps=<media caps>]`
 *
 * where:
 *
 * - `<media-type>`: Adds a new source of type `<media-type>`. Supported
 *   values:
 *      * `video`: A #videotestsrc element will be used
 *      * `audio`: An #audiotestsrc will be used
 *   you can use it as many time as wanted to expose new streams.
 * - `<element-properties>`: `key=value` list of properties to be set on the
 *   source element. See #videotestsrc properties for the video case and
 *   #audiotestsrc properties for the audio case.
 * - `<media caps>`: Caps to be set in the #capsfilter that follows source elements
 *   for example to force the video source to output a full HD stream, you can use
 *   `video/x-raw,width=1920,height=1080`.
 *
 * Note that stream definitions are interpreted as serialized #GstStructure.
 *
 * ## Examples pipeline:
 *
 * ### One audio stream with volume=0.5 and a white video stream in full HD at 30fps
 *
 * ```
 * gst-launch-1.0 playbin3 uri="testbin://audio,volume=0.5+video,pattern=white,caps=[video/x-raw,width=1920,height=1080,framerate=30/1]"
 * ```

 * ### Single full HD stream
 *
 * ```
 * gst-launch-1.0 playbin3 uri="testbin://video,pattern=green,caps=[video/x-raw,width=1920,height=1080,framerate=30/1]"
 * ```
 *
 * ### Two audio streams
 *
 * ```
 * gst-launch-1.0 playbin3 uri="testbin://audio+audio"
 * ```
 */
#include <gst/gst.h>
#include <gst/base/gstflowcombiner.h>
#include <gst/app/gstappsink.h>
#include "gstdebugutilsbadelements.h"

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("video_src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static GstStaticPadTemplate audio_src_template =
    GST_STATIC_PAD_TEMPLATE ("audio_src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("audio/x-raw(ANY);"));

#define GST_TYPE_TEST_SRC_BIN  gst_test_src_bin_get_type()
#define GST_TEST_SRC_BIN(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GST_TYPE_TEST_SRC_BIN, GstTestSrcBin))

typedef struct _GstTestSrcBin GstTestSrcBin;
typedef struct _GstTestSrcBinClass GstTestSrcBinClass;

/* *INDENT-OFF* */
GType gst_test_src_bin_get_type (void) G_GNUC_CONST;
/* *INDENT-ON* */

struct _GstTestSrcBinClass
{
  GstBinClass parent_class;
};

struct _GstTestSrcBin
{
  GstBin parent;

  gchar *uri;
  gint group_id;
  GstFlowCombiner *flow_combiner;
  GstCaps *streams_def;
  GstCaps *next_streams_def;
  gboolean expose_sources_async;
};

enum
{
  PROP_0,
  PROP_STREAM_TYPES,
  PROP_EXPOSE_SOURCES_ASYNC,
  PROP_LAST
};

#define DEFAULT_TYPES GST_STREAM_TYPE_AUDIO & GST_STREAM_TYPE_VIDEO

static GstURIType
gst_test_src_bin_uri_handler_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_test_src_bin_uri_handler_get_protocols (GType type)
{
  static const gchar *protocols[] = { "testbin", NULL };

  return protocols;
}

static gchar *
gst_test_src_bin_uri_handler_get_uri (GstURIHandler * handler)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (handler);
  gchar *uri;

  GST_OBJECT_LOCK (self);
  uri = g_strdup (self->uri);
  GST_OBJECT_UNLOCK (self);

  return uri;
}

typedef struct
{
  GstEvent *stream_start;
  GstStreamCollection *collection;
} ProbeData;

static ProbeData *
_probe_data_new (GstEvent * stream_start, GstStreamCollection * collection)
{
  ProbeData *data = g_malloc0 (sizeof (ProbeData));

  data->stream_start = stream_start;
  data->collection = gst_object_ref (collection);

  return data;
}

static void
_probe_data_free (ProbeData * data)
{
  gst_event_replace (&data->stream_start, NULL);
  gst_object_replace ((GstObject **) & data->collection, NULL);

  g_free (data);
}

static GstPadProbeReturn
src_pad_probe_cb (GstPad * pad, GstPadProbeInfo * info, ProbeData * data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:{
      gst_event_unref (event);
      info->data = gst_event_ref (data->stream_start);
      return GST_PAD_PROBE_OK;
    }
    case GST_EVENT_CAPS:{
      if (data->collection) {
        GstStreamCollection *collection = data->collection;
        /* Make sure the collection is NULL so that when caps get unstickied
         * we let them pass through. */
        data->collection = NULL;
        gst_pad_push_event (pad, gst_event_new_stream_collection (collection));
        gst_object_unref (collection);
      }
      return GST_PAD_PROBE_REMOVE;
    }
    default:
      break;
  }

  return GST_PAD_PROBE_OK;
}

static GstFlowReturn
gst_test_src_bin_chain (GstPad * pad, GstObject * object, GstBuffer * buffer)
{
  GstFlowReturn res, chain_res;

  GstTestSrcBin *self = GST_TEST_SRC_BIN (gst_object_get_parent (object));

  chain_res = gst_proxy_pad_chain_default (pad, GST_OBJECT (self), buffer);
  GST_OBJECT_LOCK (self);
  res = gst_flow_combiner_update_pad_flow (self->flow_combiner, pad, chain_res);
  GST_OBJECT_UNLOCK (self);
  gst_object_unref (self);

  if (res == GST_FLOW_FLUSHING)
    return chain_res;

  if (res == GST_FLOW_NOT_LINKED)
    GST_WARNING_OBJECT (pad,
        "all testsrcbin pads not linked, returning not-linked.");

  return res;
}

static gboolean
gst_test_src_bin_set_element_property (GQuark property_id, const GValue * value,
    GObject * element)
{
  if (property_id == g_quark_from_static_string ("__streamobj__"))
    return TRUE;

  if (property_id == g_quark_from_static_string ("caps"))
    return TRUE;

  if (G_VALUE_HOLDS_STRING (value))
    gst_util_set_object_arg (element, g_quark_to_string (property_id),
        g_value_get_string (value));
  else
    g_object_set_property (element, g_quark_to_string (property_id), value);

  return TRUE;
}

typedef struct
{
  GstEvent *event;
  gboolean res;
  GstObject *parent;
} ForwardEventData;


static gboolean
forward_seeks (GstElement * element, GstPad * pad, ForwardEventData * data)
{
  data->res &=
      gst_pad_event_default (pad, data->parent, gst_event_ref (data->event));

  return TRUE;
}

static gboolean
gst_test_src_event_function (GstPad * pad, GstObject * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_RECONFIGURE:{
      GstTestSrcBin *self = GST_TEST_SRC_BIN (parent);
      GST_OBJECT_LOCK (self);
      gst_flow_combiner_reset (self->flow_combiner);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    case GST_EVENT_SEEK:{
      ForwardEventData data = { event, TRUE, parent };

      gst_element_foreach_src_pad (GST_ELEMENT (parent),
          (GstElementForeachPadFunc) forward_seeks, &data);
      return data.res;
    }
    default:
      break;
  }
  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_test_src_bin_setup_src (GstTestSrcBin * self, const gchar * srcfactory,
    GstStaticPadTemplate * template, GstStreamType stype,
    GstStreamCollection * collection, gint * n_stream, GstStructure * props,
    GError ** error)
{
  GstElement *src;
  GstElement *capsfilter;
  GstPad *proxypad, *ghost, *pad;
  gchar *stream_id;
  gchar *pad_name;
  GstCaps *caps = NULL;
  GstStream *stream;
  GstEvent *stream_start;
  GstPadTemplate *templ;
  const GValue *caps_value = gst_structure_get_value (props, "caps");

  if (caps_value) {
    if (GST_VALUE_HOLDS_CAPS (caps_value)) {
      caps = gst_caps_copy (gst_value_get_caps (caps_value));
    } else if (GST_VALUE_HOLDS_STRUCTURE (caps_value)) {
      caps =
          gst_caps_new_full (gst_structure_copy (gst_value_get_structure
              (caps_value)), NULL);
    } else if (G_VALUE_HOLDS_STRING (caps_value)) {
      caps = gst_caps_from_string (g_value_get_string (caps_value));
      if (!caps) {
        if (error) {
          *error =
              g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
              "Invalid caps string: %s", g_value_get_string (caps_value));
        }

        return FALSE;
      }
    } else {
      if (error) {
        *error =
            g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
            "Invalid type %s for `caps`", G_VALUE_TYPE_NAME (caps_value));
      }

      return FALSE;
    }
  }

  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  if (caps) {
    g_object_set (capsfilter, "caps", caps, NULL);
  }

  src = gst_element_factory_make (srcfactory, NULL);
  pad = gst_element_get_static_pad (src, "src");
  stream_id = g_strdup_printf ("%s_stream_%d", srcfactory, *n_stream);
  stream = gst_stream_new (stream_id, caps, stype,
      (*n_stream == 0) ? GST_STREAM_FLAG_SELECT : GST_STREAM_FLAG_UNSELECT);
  stream_start = gst_event_new_stream_start (gst_stream_get_stream_id (stream));

  gst_structure_foreach (props,
      (GstStructureForeachFunc) gst_test_src_bin_set_element_property, src);

  gst_event_set_stream (stream_start, stream);
  gst_event_set_group_id (stream_start, self->group_id);

  gst_structure_set (props, "__streamobj__", GST_TYPE_STREAM, stream, NULL);
  gst_stream_collection_add_stream (collection, stream);

  gst_pad_add_probe (pad, (GstPadProbeType) GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) src_pad_probe_cb, _probe_data_new (stream_start,
          collection), (GDestroyNotify) _probe_data_free);

  g_free (stream_id);

  gst_bin_add_many (GST_BIN (self), src, capsfilter, NULL);
  if (!gst_element_link (src, capsfilter)) {
    g_error ("Could not link src with capsfilter?!");
  }

  gst_object_unref (pad);

  pad = gst_element_get_static_pad (capsfilter, "src");
  pad_name = g_strdup_printf (template->name_template, *n_stream);
  templ = gst_static_pad_template_get (template);
  ghost = gst_ghost_pad_new_from_template (pad_name, pad, templ);
  gst_object_unref (templ);
  g_free (pad_name);
  gst_object_unref (pad);

  proxypad = GST_PAD (gst_proxy_pad_get_internal (GST_PROXY_PAD (ghost)));
  gst_flow_combiner_add_pad (self->flow_combiner, ghost);
  gst_pad_set_chain_function (proxypad,
      (GstPadChainFunction) gst_test_src_bin_chain);
  gst_pad_set_event_function (ghost,
      (GstPadEventFunction) gst_test_src_event_function);
  gst_object_unref (proxypad);
  gst_pad_store_sticky_event (ghost, stream_start);
  gst_element_add_pad (GST_ELEMENT (self), ghost);
  gst_element_sync_state_with_parent (capsfilter);
  gst_element_sync_state_with_parent (src);
  *n_stream += 1;

  gst_structure_set (props, "__src__", GST_TYPE_OBJECT, src, NULL);

  gst_clear_caps (&caps);

  return TRUE;
}

static void
gst_test_src_bin_remove_child (GstElement * self, GstElement * child)
{
  GstPad *pad = gst_element_get_static_pad (child, "src");
  GstPad *ghost =
      GST_PAD (gst_proxy_pad_get_internal (GST_PROXY_PAD (gst_pad_get_peer
              (pad))));


  gst_element_set_locked_state (child, FALSE);
  gst_element_set_state (child, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self), child);
  gst_element_remove_pad (self, ghost);
}

static GstStream *
gst_test_check_prev_stream_def (GstTestSrcBin * self, GstCaps * prev_streams,
    GstStructure * stream_def)
{
  gint i;

  if (!prev_streams)
    return NULL;

  for (i = 0; i < gst_caps_get_size (prev_streams); i++) {
    GstStructure *prev_stream = gst_caps_get_structure (prev_streams, i);
    GstElement *e = NULL;
    GstStream *stream = NULL;

    gst_structure_get (prev_stream, "__src__", GST_TYPE_OBJECT, &e,
        "__streamobj__", GST_TYPE_STREAM, &stream, NULL);
    gst_structure_remove_fields (prev_stream, "__src__", "__streamobj__", NULL);
    if (gst_structure_is_equal (prev_stream, stream_def)) {
      g_assert (stream);

      gst_caps_remove_structure (prev_streams, i);
      gst_structure_set (stream_def, "__src__", GST_TYPE_OBJECT, e,
          "__streamobj__", GST_TYPE_STREAM, stream, NULL);

      g_assert (stream);
      return stream;
    }

    gst_structure_set (stream_def, "__src__", GST_TYPE_OBJECT, e,
        "__streamobj__", GST_TYPE_STREAM, stream, NULL);
  }

  return NULL;
}

static gboolean
gst_test_src_bin_create_sources (GstTestSrcBin * self)
{
  gint i, n_audio = 0, n_video = 0;
  GError *error = NULL;
  GstStreamCollection *collection = gst_stream_collection_new (NULL);
  GstCaps *streams_def, *prev_streams_def;

  GST_OBJECT_LOCK (self);
  streams_def = self->next_streams_def;
  prev_streams_def = self->streams_def;
  self->next_streams_def = NULL;
  self->streams_def = NULL;
  GST_OBJECT_UNLOCK (self);

  GST_INFO_OBJECT (self, "Create sources %" GST_PTR_FORMAT,
      self->next_streams_def);

  self->group_id = gst_util_group_id_next ();
  for (i = 0; i < gst_caps_get_size (streams_def); i++) {
    GstStream *stream;
    GstStructure *stream_def = gst_caps_get_structure (streams_def, i);

    if ((stream =
            gst_test_check_prev_stream_def (self, prev_streams_def,
                stream_def))) {
      GST_INFO_OBJECT (self,
          "Reusing already existing stream: %" GST_PTR_FORMAT, stream_def);
      gst_stream_collection_add_stream (collection, stream);
      if (gst_structure_has_name (stream_def, "video"))
        n_video++;
      else
        n_audio++;
      continue;
    }

    if (gst_structure_has_name (stream_def, "video")) {
      if (!gst_test_src_bin_setup_src (self, "videotestsrc",
              &video_src_template, GST_STREAM_TYPE_VIDEO, collection, &n_video,
              stream_def, &error)) {
        goto failed;
      }
    } else if (gst_structure_has_name (stream_def, "audio")) {
      if (!gst_test_src_bin_setup_src (self, "audiotestsrc",
              &audio_src_template, GST_STREAM_TYPE_AUDIO, collection, &n_audio,
              stream_def, &error)) {
        goto failed;
      }
    } else {
      GST_ERROR_OBJECT (self, "Unknown type %s",
          gst_structure_get_name (stream_def));
    }
  }

  if (prev_streams_def) {
    for (i = 0; i < gst_caps_get_size (prev_streams_def); i++) {
      GstStructure *prev_stream = gst_caps_get_structure (prev_streams_def, i);
      GstElement *child;

      gst_structure_get (prev_stream, "__src__", GST_TYPE_OBJECT, &child, NULL);
      gst_test_src_bin_remove_child (GST_ELEMENT (self), child);
    }
    gst_clear_caps (&prev_streams_def);
  }

  if (!n_video && !n_audio) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("No audio or video stream defined."), (NULL));
    goto failed;
  }

  GST_OBJECT_LOCK (self);
  self->streams_def = streams_def;
  GST_OBJECT_UNLOCK (self);

  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_stream_collection (GST_OBJECT (self), collection));
  gst_object_unref (collection);

  gst_element_no_more_pads (GST_ELEMENT (self));

  return TRUE;

failed:
  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_error (GST_OBJECT (self), error, NULL)
      );
  return FALSE;
}

static gboolean
gst_test_src_bin_uri_handler_set_uri (GstURIHandler * handler,
    const gchar * uri, GError ** error)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (handler);
  gchar *tmp, *location = gst_uri_get_location (uri);
  GstCaps *streams_def;

  for (tmp = location; *tmp != '\0'; tmp++)
    if (*tmp == '+')
      *tmp = ';';

  streams_def = gst_caps_from_string (location);
  g_free (location);

  if (!streams_def)
    goto failed;

  GST_OBJECT_LOCK (self);
  gst_clear_caps (&self->next_streams_def);
  self->next_streams_def = streams_def;
  g_free (self->uri);
  self->uri = g_strdup (uri);

  if (GST_STATE (self) >= GST_STATE_PAUSED) {

    if (self->expose_sources_async) {
      GST_OBJECT_UNLOCK (self);

      gst_element_call_async (GST_ELEMENT (self),
          (GstElementCallAsyncFunc) gst_test_src_bin_create_sources,
          NULL, NULL);
    } else {
      GST_OBJECT_UNLOCK (self);

      gst_test_src_bin_create_sources (self);
    }
  } else {
    GST_OBJECT_UNLOCK (self);
  }

  return TRUE;

failed:

  return FALSE;
}

static void
gst_test_src_bin_uri_handler_init (gpointer g_iface, gpointer unused)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_test_src_bin_uri_handler_get_type;
  iface->get_protocols = gst_test_src_bin_uri_handler_get_protocols;
  iface->get_uri = gst_test_src_bin_uri_handler_get_uri;
  iface->set_uri = gst_test_src_bin_uri_handler_set_uri;
}

/* *INDENT-OFF* */
G_DEFINE_TYPE_WITH_CODE (GstTestSrcBin, gst_test_src_bin, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_test_src_bin_uri_handler_init))
/* *INDENT-ON* */

GST_ELEMENT_REGISTER_DEFINE (testsrcbin, "testsrcbin",
    GST_RANK_NONE, gst_test_src_bin_get_type ());

static void
gst_test_src_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (object);

  switch (prop_id) {
    case PROP_STREAM_TYPES:
    {
      gchar *uri = g_strdup_printf ("testbin://%s", g_value_get_string (value));

      g_assert (gst_uri_handler_set_uri (GST_URI_HANDLER (self), uri, NULL));
      g_free (uri);
      break;
    }
    case PROP_EXPOSE_SOURCES_ASYNC:
    {
      GST_OBJECT_LOCK (self);
      self->expose_sources_async = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_test_src_bin_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (object);

  switch (prop_id) {
    case PROP_STREAM_TYPES:
    {
      gchar *uri = gst_uri_handler_get_uri (GST_URI_HANDLER (self));
      if (uri) {
        gchar *types = gst_uri_get_location (uri);
        g_value_set_string (value, types);
        g_free (uri);
        g_free (types);
      }
      break;
    }
    case PROP_EXPOSE_SOURCES_ASYNC:
    {
      GST_OBJECT_LOCK (self);
      g_value_set_boolean (value, self->expose_sources_async);
      GST_OBJECT_UNLOCK (self);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_test_src_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      if (self->expose_sources_async) {
        gst_element_call_async (element,
            (GstElementCallAsyncFunc) gst_test_src_bin_create_sources,
            NULL, NULL);
      } else {
        gst_test_src_bin_create_sources (self);
      }
      break;
    }
    default:
      break;
  }

  result =
      GST_ELEMENT_CLASS (gst_test_src_bin_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      gst_flow_combiner_reset (self->flow_combiner);
      break;
    }
    default:
      break;
  }

  return result;
}

static void
gst_test_src_bin_finalize (GObject * object)
{
  GstTestSrcBin *self = GST_TEST_SRC_BIN (object);

  G_OBJECT_CLASS (gst_test_src_bin_parent_class)->finalize (object);

  g_free (self->uri);
  gst_clear_caps (&self->streams_def);
  gst_flow_combiner_free (self->flow_combiner);

}

static void
gst_test_src_bin_init (GstTestSrcBin * self)
{
  self->flow_combiner = gst_flow_combiner_new ();
}

static void
gst_test_src_bin_class_init (GstTestSrcBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_klass = (GstElementClass *) klass;

  gobject_class->finalize = gst_test_src_bin_finalize;
  gobject_class->get_property = gst_test_src_bin_get_property;
  gobject_class->set_property = gst_test_src_bin_set_property;

  /**
   * GstTestSrcBin:stream-types:
   *
   * String describing the stream types to expose, eg. "video+audio".
   */
  g_object_class_install_property (gobject_class, PROP_STREAM_TYPES,
      g_param_spec_string ("stream-types", "Stream types",
          "String describing the stream types to expose, eg. \"video+audio\".",
          NULL, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstTestSrcBin:expose-sources-async:
   *
   * Whether to expose sources at random time to simulate a source that is
   * reading a file and exposing the srcpads later.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_EXPOSE_SOURCES_ASYNC,
      g_param_spec_boolean ("expose-sources-async", "Expose Sources Async",
          " Whether to expose sources at random time to simulate a source that is"
          " reading a file and exposing the srcpads later.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gstelement_klass->change_state =
      GST_DEBUG_FUNCPTR (gst_test_src_bin_change_state);
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&video_src_template));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&audio_src_template));
}
