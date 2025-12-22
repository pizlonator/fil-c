/* ASF muxer plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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

/* based on:
 * - avimux (by Ronald Bultje and Mark Nauwelaerts)
 * - qtmux (by Thiago Santos and Mark Nauwelaerts)
 */

/**
 * SECTION:element-asfmux
 * @title: asfmux
 *
 * Muxes media into an ASF file/stream.
 *
 * Pad names are either video_xx or audio_xx, where 'xx' is the
 * stream number of the stream that goes through that pad. Stream numbers
 * are assigned sequentially, starting from 1.
 *
 * ## Example launch lines
 *
 * (write everything in one line, without the backslash characters)
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=250 \
 * ! "video/x-raw,format=(string)I420,framerate=(fraction)25/1" ! avenc_wmv2 \
 * ! asfmux name=mux ! filesink location=test.asf \
 * audiotestsrc num-buffers=440 ! audioconvert \
 * ! "audio/x-raw,rate=44100" ! avenc_wmav2 ! mux.
 * ]| This creates an ASF file containing an WMV video stream
 * with a test picture and WMA audio stream of a test sound.
 *
 * ## Live streaming
 * asfmux and rtpasfpay are capable of generating a live asf stream.
 * asfmux has to set its 'streamable' property to true, because in this
 * mode it won't try to seek back to the start of the file to replace
 * some fields that couldn't be known at the file start. In this mode,
 * it won't also send indexes at the end of the data packets (the actual
 * media content)
 * the following pipelines are an example of this usage.
 * (write everything in one line, without the backslash characters)
 * Server (sender)
 * |[
 * gst-launch-1.0 -ve videotestsrc ! avenc_wmv2 ! asfmux name=mux streamable=true \
 * ! rtpasfpay ! udpsink host=127.0.0.1 port=3333 \
 * audiotestsrc ! avenc_wmav2 ! mux.
 * ]|
 * Client (receiver)
 * |[
 * gst-launch-1.0 udpsrc port=3333 ! "caps_from_rtpasfpay_at_sender" \
 * ! rtpasfdepay ! decodebin name=d ! queue \
 * ! videoconvert ! autovideosink \
 * d. ! queue ! audioconvert ! autoaudiosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <glib/gi18n-lib.h>
#include "gstasfmux.h"

#define DEFAULT_SIMPLE_INDEX_TIME_INTERVAL G_GUINT64_CONSTANT (10000000)
#define MAX_PAYLOADS_IN_A_PACKET 63

GST_DEBUG_CATEGORY_STATIC (asfmux_debug);
#define GST_CAT_DEFAULT asfmux_debug

enum
{
  PROP_0,
  PROP_PACKET_SIZE,
  PROP_PREROLL,
  PROP_MERGE_STREAM_TAGS,
  PROP_PADDING,
  PROP_STREAMABLE
};

/* Stores a tag list for the available/known tags
 * in an ASF file
 * Also stores the sizes those entries would use in a
 * content description object and extended content
 * description object
 */
typedef struct
{
  GstTagList *tags;
  guint64 cont_desc_size;
  guint64 ext_cont_desc_size;
} GstAsfTags;

/* Helper struct to be used as user data
 * in gst_tag_foreach function for writing
 * each tag for the metadata objects
 *
 * stream_num is used only for stream dependent tags
 */
typedef struct
{
  GstAsfMux *asfmux;
  guint8 *buf;
  guint16 count;
  guint64 size;
  guint16 stream_num;
} GstAsfExtContDescData;

typedef GstAsfExtContDescData GstAsfMetadataObjData;

#define DEFAULT_PACKET_SIZE 4800
#define DEFAULT_PREROLL 5000
#define DEFAULT_MERGE_STREAM_TAGS TRUE
#define DEFAULT_PADDING 0
#define DEFAULT_STREAMABLE FALSE

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf, " "parsed = (boolean) true")
    );

static GstStaticPadTemplate video_sink_factory =
GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/x-wmv, wmvversion = (int) [1,3]"));

static GstStaticPadTemplate audio_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("audio/x-wma, wmaversion = (int) [1,3]; "
        "audio/mpeg, layer = (int) 3, mpegversion = (int) 1, "
        "channels = (int) [1,2], rate = (int) [8000,96000]"));

static gboolean gst_asf_mux_audio_set_caps (GstPad * pad, GstCaps * caps);
static gboolean gst_asf_mux_video_set_caps (GstPad * pad, GstCaps * caps);

static GstPad *gst_asf_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_asf_mux_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_asf_mux_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_asf_mux_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_asf_mux_sink_event (GstCollectPads * pads,
    GstCollectData * cdata, GstEvent * event, GstAsfMux * asfmux);

static void gst_asf_mux_pad_reset (GstAsfPad * data);
static GstFlowReturn gst_asf_mux_collected (GstCollectPads * collect,
    gpointer data);

static GstElementClass *parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (GstAsfMux, gst_asf_mux, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_TAG_SETTER, NULL));
GST_ELEMENT_REGISTER_DEFINE (asfmux, "asfmux",
    GST_RANK_PRIMARY, GST_TYPE_ASF_MUX);

static void
gst_asf_mux_reset (GstAsfMux * asfmux)
{
  asfmux->state = GST_ASF_MUX_STATE_NONE;
  asfmux->stream_number = 0;
  asfmux->data_object_size = 0;
  asfmux->data_object_position = 0;
  asfmux->file_properties_object_position = 0;
  asfmux->total_data_packets = 0;
  asfmux->file_size = 0;
  asfmux->packet_size = 0;
  asfmux->first_ts = GST_CLOCK_TIME_NONE;

  if (asfmux->payloads) {
    GSList *walk;
    for (walk = asfmux->payloads; walk; walk = g_slist_next (walk)) {
      gst_asf_payload_free ((AsfPayload *) walk->data);
      walk->data = NULL;
    }
    g_slist_free (asfmux->payloads);
  }
  asfmux->payloads = NULL;
  asfmux->payload_data_size = 0;

  asfmux->file_id.v1 = 0;
  asfmux->file_id.v2 = 0;
  asfmux->file_id.v3 = 0;
  asfmux->file_id.v4 = 0;

  gst_tag_setter_reset_tags (GST_TAG_SETTER (asfmux));
}

static void
gst_asf_mux_finalize (GObject * object)
{
  GstAsfMux *asfmux;

  asfmux = GST_ASF_MUX (object);

  gst_asf_mux_reset (asfmux);
  gst_object_unref (asfmux->collect);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_asf_mux_class_init (GstAsfMuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_asf_mux_get_property;
  gobject_class->set_property = gst_asf_mux_set_property;
  gobject_class->finalize = gst_asf_mux_finalize;

  g_object_class_install_property (gobject_class, PROP_PACKET_SIZE,
      g_param_spec_uint ("packet-size", "Packet size",
          "The ASF packets size (bytes)",
          ASF_MULTIPLE_PAYLOAD_HEADER_SIZE + 1, G_MAXUINT32,
          DEFAULT_PACKET_SIZE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PREROLL,
      g_param_spec_uint64 ("preroll", "Preroll",
          "The preroll time (milliseconds)",
          0, G_MAXUINT64,
          DEFAULT_PREROLL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MERGE_STREAM_TAGS,
      g_param_spec_boolean ("merge-stream-tags", "Merge Stream Tags",
          "If the stream metadata (received as events in the sink) should be "
          "merged to the main file metadata.",
          DEFAULT_MERGE_STREAM_TAGS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PADDING,
      g_param_spec_uint64 ("padding", "Padding",
          "Size of the padding object to be added to the end of the header. "
          "If this less than 24 (the smaller size of an ASF object), "
          "no padding is added.",
          0, G_MAXUINT64,
          DEFAULT_PADDING,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STREAMABLE,
      g_param_spec_boolean ("streamable", "Streamable",
          "If set to true, the output should be as if it is to be streamed "
          "and hence no indexes written or duration written.",
          DEFAULT_STREAMABLE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_asf_mux_request_new_pad);
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_asf_mux_change_state);

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &video_sink_factory);

  gst_element_class_set_static_metadata (gstelement_class, "ASF muxer",
      "Codec/Muxer",
      "Muxes audio and video into an ASF stream",
      "Thiago Santos <thiagoss@embedded.ufcg.edu.br>");

  GST_DEBUG_CATEGORY_INIT (asfmux_debug, "asfmux", 0, "Muxer for ASF streams");
}

static void
gst_asf_mux_init (GstAsfMux * asfmux)
{
  asfmux->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps (asfmux->srcpad);
  gst_element_add_pad (GST_ELEMENT (asfmux), asfmux->srcpad);

  asfmux->collect = gst_collect_pads_new ();
  gst_collect_pads_set_function (asfmux->collect,
      (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (gst_asf_mux_collected),
      asfmux);
  gst_collect_pads_set_event_function (asfmux->collect,
      (GstCollectPadsEventFunction) GST_DEBUG_FUNCPTR (gst_asf_mux_sink_event),
      asfmux);

  asfmux->payloads = NULL;
  asfmux->prop_packet_size = DEFAULT_PACKET_SIZE;
  asfmux->prop_preroll = DEFAULT_PREROLL;
  asfmux->prop_merge_stream_tags = DEFAULT_MERGE_STREAM_TAGS;
  asfmux->prop_padding = DEFAULT_PADDING;
  asfmux->prop_streamable = DEFAULT_STREAMABLE;
  gst_asf_mux_reset (asfmux);
}

static gboolean
gst_asf_mux_sink_event (GstCollectPads * pads, GstCollectData * cdata,
    GstEvent * event, GstAsfMux * asfmux)
{
  GstAsfPad *asfpad = (GstAsfPad *) cdata;
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      if (asfpad->is_audio)
        ret = gst_asf_mux_audio_set_caps (cdata->pad, caps);
      else
        ret = gst_asf_mux_video_set_caps (cdata->pad, caps);
      gst_event_unref (event);
      event = NULL;
      break;
    }
    case GST_EVENT_TAG:{
      GST_DEBUG_OBJECT (asfmux, "received tag event");
      /* we discard tag events that come after we started
       * writing the headers, because tags are to be in
       * the headers
       */
      if (asfmux->state == GST_ASF_MUX_STATE_NONE) {
        GstTagList *list = NULL;
        gst_event_parse_tag (event, &list);
        if (asfmux->merge_stream_tags) {
          GstTagSetter *setter = GST_TAG_SETTER (asfmux);
          const GstTagMergeMode mode =
              gst_tag_setter_get_tag_merge_mode (setter);
          gst_tag_setter_merge_tags (setter, list, mode);
        } else {
          if (asfpad->taglist == NULL) {
            asfpad->taglist = gst_tag_list_new_empty ();
          }
          gst_tag_list_insert (asfpad->taglist, list, GST_TAG_MERGE_REPLACE);
        }
      }
      break;
    }
    default:
      break;
  }

  if (event != NULL)
    return gst_collect_pads_event_default (pads, cdata, event, FALSE);

  return ret;
}

/**
 * gst_asf_mux_push_buffer:
 * @asfmux: #GstAsfMux that should push the buffer
 * @buf: #GstBuffer to be pushed
 *
 * Pushes a buffer downstream and adds its size to the total file size
 *
 * Returns: the result of #gst_pad_push on the buffer
 */
static GstFlowReturn
gst_asf_mux_push_buffer (GstAsfMux * asfmux, GstBuffer * buf, gsize bufsize)
{
  GstFlowReturn ret;

  ret = gst_pad_push (asfmux->srcpad, buf);

  if (ret == GST_FLOW_OK)
    asfmux->file_size += bufsize;

  return ret;
}

/**
 * content_description_calc_size_for_tag:
 * @taglist: the #GstTagList that contains the tag
 * @tag: the tag's name
 * @user_data: a #GstAsfTags struct for putting the results
 *
 * Function that has the #GstTagForEach signature and
 * is used to calculate the size in bytes for each tag
 * that can be contained in asf's content description object
 * and extended content description object. This size is added
 * to the total size for each of that objects in the #GstAsfTags
 * struct passed in the user_data pointer.
 */
static void
content_description_calc_size_for_tag (const GstTagList * taglist,
    const gchar * tag, gpointer user_data)
{
  const gchar *asftag = gst_asf_get_asf_tag (tag);
  GValue value = { 0 };
  guint type;
  GstAsfTags *asftags = (GstAsfTags *) user_data;
  guint content_size;

  if (asftag == NULL)
    return;

  if (!gst_tag_list_copy_value (&value, taglist, tag)) {
    return;
  }
  type = gst_asf_get_tag_field_type (&value);
  switch (type) {
    case ASF_TAG_TYPE_UNICODE_STR:
    {
      const gchar *text;

      text = g_value_get_string (&value);
      /* +1 -> because of the \0 at the end
       * 2* -> because we have uft8, and asf demands utf16
       */
      content_size = 2 * (1 + g_utf8_strlen (text, -1));

      if (gst_asf_tag_present_in_content_description (tag)) {
        asftags->cont_desc_size += content_size;
      }
    }
      break;
    case ASF_TAG_TYPE_DWORD:
      content_size = 4;
      break;
    default:
      GST_WARNING ("Unhandled asf tag field type %u for tag %s", type, tag);
      g_value_reset (&value);
      return;
  }
  if (asftag) {
    /* size of the tag content in utf16 +
     * size of the tag name +
     * 3 uint16 (size of the tag name string,
     * size of the tag content string and
     * type of content
     */
    asftags->ext_cont_desc_size += content_size +
        (g_utf8_strlen (asftag, -1) + 1) * 2 + 6;
  }
  gst_tag_list_add_value (asftags->tags, GST_TAG_MERGE_REPLACE, tag, &value);
  g_value_reset (&value);
}

/* FIXME
 * it is awful to keep track of the size here
 * and get the same tags in the writing function */
/**
 * gst_asf_mux_get_content_description_tags:
 * @asfmux: #GstAsfMux to have its tags processed
 * @asftags: #GstAsfTags to hold the results
 *
 * Inspects the tags received by the GstTagSetter interface
 * or possibly by sink tag events and calculates the total
 * size needed for the default and extended content description objects.
 * This results and a copy of the #GstTagList
 * are stored in the #GstAsfTags. We store a copy so that
 * the sizes estimated here maintain the same until they are
 * written to the asf file.
 */
static void
gst_asf_mux_get_content_description_tags (GstAsfMux * asfmux,
    GstAsfTags * asftags)
{
  const GstTagList *tags;

  tags = gst_tag_setter_get_tag_list (GST_TAG_SETTER (asfmux));
  if (tags && !gst_tag_list_is_empty (tags)) {
    if (asftags->tags != NULL) {
      gst_tag_list_unref (asftags->tags);
    }
    asftags->tags = gst_tag_list_new_empty ();
    asftags->cont_desc_size = 0;
    asftags->ext_cont_desc_size = 0;

    GST_DEBUG_OBJECT (asfmux, "Processing tags");
    gst_tag_list_foreach (tags, content_description_calc_size_for_tag, asftags);
  } else {
    GST_DEBUG_OBJECT (asfmux, "No tags received");
  }

  if (asftags->cont_desc_size > 0) {
    asftags->cont_desc_size += ASF_CONTENT_DESCRIPTION_OBJECT_SIZE;
  }
  if (asftags->ext_cont_desc_size > 0) {
    asftags->ext_cont_desc_size += ASF_EXT_CONTENT_DESCRIPTION_OBJECT_SIZE;
  }
}

/**
 * add_metadata_tag_size:
 * @taglist: #GstTagList
 * @tag: tag name
 * @user_data: pointer to a guint to store the result
 *
 * GstTagForeachFunc implementation that accounts the size of
 * each tag in the taglist and adds them to the guint pointed
 * by the user_data
 */
static void
add_metadata_tag_size (const GstTagList * taglist, const gchar * tag,
    gpointer user_data)
{
  const gchar *asftag = gst_asf_get_asf_tag (tag);
  GValue value = { 0 };
  guint type;
  guint content_size;
  guint *total_size = (guint *) user_data;

  if (asftag == NULL)
    return;

  if (!gst_tag_list_copy_value (&value, taglist, tag)) {
    return;
  }
  type = gst_asf_get_tag_field_type (&value);
  switch (type) {
    case ASF_TAG_TYPE_UNICODE_STR:
    {
      const gchar *text;

      text = g_value_get_string (&value);
      /* +1 -> because of the \0 at the end
       * 2* -> because we have uft8, and asf demands utf16
       */
      content_size = 2 * (1 + g_utf8_strlen (text, -1));
    }
      break;
    case ASF_TAG_TYPE_DWORD:
      content_size = 4;
      break;
    default:
      GST_WARNING ("Unhandled asf tag field type %u for tag %s", type, tag);
      g_value_reset (&value);
      return;
  }
  /* size of reserved (2) +
   * size of stream number (2) +
   * size of the tag content in utf16 +
   * size of the tag name +
   * 2 uint16 (size of the tag name string and type of content) +
   * 1 uint32 (size of the data)
   */
  *total_size +=
      4 + content_size + (g_utf8_strlen (asftag, -1) + 1) * 2 + 4 + 4;
  g_value_reset (&value);
}

/**
 * gst_asf_mux_get_metadata_object_size:
 * @asfmux: #GstAsfMux
 * @asfpad: pad for which the metadata object size should be calculated
 *
 * Calculates the size of the metadata object for the tags of the stream
 * handled by the asfpad in the parameter
 *
 * Returns: The size calculated
 */
static guint
gst_asf_mux_get_metadata_object_size (GstAsfMux * asfmux, GstAsfPad * asfpad)
{
  guint size = ASF_METADATA_OBJECT_SIZE;
  if (asfpad->taglist == NULL || gst_tag_list_is_empty (asfpad->taglist))
    return 0;

  gst_tag_list_foreach (asfpad->taglist, add_metadata_tag_size, &size);
  return size;
}

/**
 * gst_asf_mux_get_headers_size:
 * @asfmux: #GstAsfMux
 *
 * Calculates the size of the headers of the asf stream
 * to be generated by this #GstAsfMux.
 * Its used for determining the size of the buffer to allocate
 * to exactly fit the headers in.
 * Padding and metadata objects sizes are not included.
 *
 * Returns: the calculated size
 */
static guint
gst_asf_mux_get_headers_size (GstAsfMux * asfmux)
{
  GSList *walk;
  gint stream_num = 0;
  guint size = ASF_HEADER_OBJECT_SIZE +
      ASF_FILE_PROPERTIES_OBJECT_SIZE + ASF_HEADER_EXTENSION_OBJECT_SIZE;

  /* per stream data */
  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    GstAsfPad *asfpad = (GstAsfPad *) walk->data;

    if (asfpad->is_audio)
      size += ASF_AUDIO_SPECIFIC_DATA_SIZE;
    else
      size += ASF_VIDEO_SPECIFIC_DATA_SIZE;

    if (asfpad->codec_data)
      size += gst_buffer_get_size (asfpad->codec_data);

    stream_num++;
  }
  size += stream_num * (ASF_STREAM_PROPERTIES_OBJECT_SIZE +
      ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_SIZE);

  return size;
}

/**
 * gst_asf_mux_write_header_object:
 * @asfmux:
 * @buf: pointer to the data pointer
 * @size: size of the header object
 * @child_objects: number of children objects inside the main header object
 *
 * Writes the main asf header object start. The buffer pointer
 * is incremented to the next writing position.
 */
static void
gst_asf_mux_write_header_object (GstAsfMux * asfmux, guint8 ** buf,
    guint64 size, guint32 child_objects)
{
  gst_asf_put_guid (*buf, guids[ASF_HEADER_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, size);        /* object size */
  GST_WRITE_UINT32_LE (*buf + 24, child_objects);       /* # of child objects */
  GST_WRITE_UINT8 (*buf + 28, 0x01);    /* reserved */
  GST_WRITE_UINT8 (*buf + 29, 0x02);    /* reserved */
  *buf += ASF_HEADER_OBJECT_SIZE;
}

/**
 * gst_asf_mux_write_file_properties:
 * @asfmux:
 * @buf: pointer to the data pointer
 *
 * Writes the file properties object to the buffer. The buffer pointer
 * is incremented to the next writing position.
 */
static void
gst_asf_mux_write_file_properties (GstAsfMux * asfmux, guint8 ** buf)
{
  gst_asf_put_guid (*buf, guids[ASF_FILE_PROPERTIES_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, ASF_FILE_PROPERTIES_OBJECT_SIZE);     /* object size */
  gst_asf_put_guid (*buf + 24, asfmux->file_id);
  GST_WRITE_UINT64_LE (*buf + 40, 0);   /* file size - needs update */
  gst_asf_put_time (*buf + 48, gst_asf_get_current_time ());    /* creation time */
  GST_WRITE_UINT64_LE (*buf + 56, 0);   /* data packets - needs update */
  GST_WRITE_UINT64_LE (*buf + 64, 0);   /* play duration - needs update */
  GST_WRITE_UINT64_LE (*buf + 72, 0);   /* send duration - needs update */
  GST_WRITE_UINT64_LE (*buf + 80, asfmux->preroll);     /* preroll */
  GST_WRITE_UINT32_LE (*buf + 88, 0x1); /* flags - broadcast on */
  GST_WRITE_UINT32_LE (*buf + 92, asfmux->packet_size); /* minimum data packet size */
  GST_WRITE_UINT32_LE (*buf + 96, asfmux->packet_size); /* maximum data packet size */
  GST_WRITE_UINT32_LE (*buf + 100, 0);  /* maximum bitrate TODO */

  *buf += ASF_FILE_PROPERTIES_OBJECT_SIZE;
}

/**
 * gst_asf_mux_write_stream_properties:
 * @asfmux:
 * @buf: pointer to the data pointer
 * @asfpad: Pad that handles the stream
 *
 * Writes the stream properties object in the buffer
 * for the stream handled by the #GstAsfPad passed.
 * The pointer is incremented to the next writing position
 */
static void
gst_asf_mux_write_stream_properties (GstAsfMux * asfmux, guint8 ** buf,
    GstAsfPad * asfpad)
{
  guint32 codec_data_length = 0;
  guint32 media_specific_data_length = 0;
  guint16 flags = 0;

  /* codec specific data length */
  if (asfpad->codec_data)
    codec_data_length = gst_buffer_get_size (asfpad->codec_data);
  if (asfpad->is_audio)
    media_specific_data_length = ASF_AUDIO_SPECIFIC_DATA_SIZE;
  else
    media_specific_data_length = ASF_VIDEO_SPECIFIC_DATA_SIZE;

  GST_DEBUG_OBJECT (asfmux, "Stream %" G_GUINT16_FORMAT " codec data length: %"
      G_GUINT32_FORMAT ", media specific data length: %" G_GUINT32_FORMAT,
      (guint16) asfpad->stream_number, codec_data_length,
      media_specific_data_length);

  gst_asf_put_guid (*buf, guids[ASF_STREAM_PROPERTIES_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, ASF_STREAM_PROPERTIES_OBJECT_SIZE + codec_data_length + media_specific_data_length);  /* object size */

  /* stream type */
  if (asfpad->is_audio)
    gst_asf_put_guid (*buf + 24, guids[ASF_AUDIO_MEDIA_INDEX]);
  else
    gst_asf_put_guid (*buf + 24, guids[ASF_VIDEO_MEDIA_INDEX]);
  /* error correction */
  gst_asf_put_guid (*buf + 40, guids[ASF_NO_ERROR_CORRECTION_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 56, 0);   /* time offset */

  GST_WRITE_UINT32_LE (*buf + 64, codec_data_length + media_specific_data_length);      /* type specific data length */
  GST_WRITE_UINT32_LE (*buf + 68, 0);   /* error correction data length */

  flags = (asfpad->stream_number & 0x7F);
  GST_WRITE_UINT16_LE (*buf + 72, flags);
  GST_WRITE_UINT32_LE (*buf + 74, 0);   /* reserved */

  *buf += ASF_STREAM_PROPERTIES_OBJECT_SIZE;
  /* audio specific data */
  if (asfpad->is_audio) {
    GstAsfAudioPad *audiopad = (GstAsfAudioPad *) asfpad;
    GST_WRITE_UINT16_LE (*buf, audiopad->audioinfo.format);
    GST_WRITE_UINT16_LE (*buf + 2, audiopad->audioinfo.channels);
    GST_WRITE_UINT32_LE (*buf + 4, audiopad->audioinfo.rate);
    GST_WRITE_UINT32_LE (*buf + 8, audiopad->audioinfo.av_bps);
    GST_WRITE_UINT16_LE (*buf + 12, audiopad->audioinfo.blockalign);
    GST_WRITE_UINT16_LE (*buf + 14, audiopad->audioinfo.bits_per_sample);
    GST_WRITE_UINT16_LE (*buf + 16, codec_data_length);

    GST_DEBUG_OBJECT (asfmux,
        "wave formatex values: codec_id=%" G_GUINT16_FORMAT ", channels=%"
        G_GUINT16_FORMAT ", rate=%" G_GUINT32_FORMAT ", bytes_per_sec=%"
        G_GUINT32_FORMAT ", block_alignment=%" G_GUINT16_FORMAT
        ", bits_per_sample=%" G_GUINT16_FORMAT ", codec_data_length=%u",
        audiopad->audioinfo.format, audiopad->audioinfo.channels,
        audiopad->audioinfo.rate, audiopad->audioinfo.av_bps,
        audiopad->audioinfo.blockalign, audiopad->audioinfo.bits_per_sample,
        codec_data_length);


    *buf += ASF_AUDIO_SPECIFIC_DATA_SIZE;
  } else {
    GstAsfVideoPad *videopad = (GstAsfVideoPad *) asfpad;
    GST_WRITE_UINT32_LE (*buf, (guint32) videopad->vidinfo.width);
    GST_WRITE_UINT32_LE (*buf + 4, (guint32) videopad->vidinfo.height);
    GST_WRITE_UINT8 (*buf + 8, 2);

    /* the BITMAPINFOHEADER size + codec_data size */
    GST_WRITE_UINT16_LE (*buf + 9,
        ASF_VIDEO_SPECIFIC_DATA_SIZE + codec_data_length - 11);

    /* BITMAPINFOHEADER */
    GST_WRITE_UINT32_LE (*buf + 11,
        ASF_VIDEO_SPECIFIC_DATA_SIZE + codec_data_length - 11);
    gst_asf_put_i32 (*buf + 15, videopad->vidinfo.width);
    gst_asf_put_i32 (*buf + 19, videopad->vidinfo.height);
    GST_WRITE_UINT16_LE (*buf + 23, 1); /* reserved */
    GST_WRITE_UINT16_LE (*buf + 25, videopad->vidinfo.bit_cnt);
    GST_WRITE_UINT32_LE (*buf + 27, videopad->vidinfo.compression);
    GST_WRITE_UINT32_LE (*buf + 31, videopad->vidinfo.width *
        videopad->vidinfo.height * videopad->vidinfo.bit_cnt);
    GST_WRITE_UINT32_LE (*buf + 35, videopad->vidinfo.xpels_meter);
    GST_WRITE_UINT32_LE (*buf + 39, videopad->vidinfo.ypels_meter);
    GST_WRITE_UINT32_LE (*buf + 43, videopad->vidinfo.num_colors);
    GST_WRITE_UINT32_LE (*buf + 47, videopad->vidinfo.imp_colors);

    *buf += ASF_VIDEO_SPECIFIC_DATA_SIZE;
  }

  if (codec_data_length > 0)
    gst_buffer_extract (asfpad->codec_data, 0, *buf, codec_data_length);

  *buf += codec_data_length;
}

/**
 * gst_asf_mux_write_header_extension:
 * @asfmux:
 * @buf: pointer to the buffer pointer
 * @extension_size: size of the extensions
 *
 * Writes the header of the header extension object. The buffer pointer
 * is incremented to the next writing position (the  header extension object
 * childs should be written from that point)
 */
static void
gst_asf_mux_write_header_extension (GstAsfMux * asfmux, guint8 ** buf,
    guint64 extension_size)
{
  gst_asf_put_guid (*buf, guids[ASF_HEADER_EXTENSION_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, ASF_HEADER_EXTENSION_OBJECT_SIZE + extension_size);   /* object size */
  gst_asf_put_guid (*buf + 24, guids[ASF_RESERVED_1_INDEX]);    /* reserved */
  GST_WRITE_UINT16_LE (*buf + 40, 6);   /* reserved */
  GST_WRITE_UINT32_LE (*buf + 42, extension_size);      /* header extension data size */
  *buf += ASF_HEADER_EXTENSION_OBJECT_SIZE;
}

/**
 * gst_asf_mux_write_extended_stream_properties:
 * @asfmux:
 * @buf: pointer to the buffer pointer
 * @asfpad: Pad that handles the stream of the properties to be written
 *
 * Writes the extended stream properties object (that is part of the
 * header extension objects) for the stream handled by asfpad
 */
static void
gst_asf_mux_write_extended_stream_properties (GstAsfMux * asfmux, guint8 ** buf,
    GstAsfPad * asfpad)
{
  gst_asf_put_guid (*buf, guids[ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_SIZE);
  GST_WRITE_UINT64_LE (*buf + 24, 0);   /* start time */
  GST_WRITE_UINT64_LE (*buf + 32, 0);   /* end time */
  GST_WRITE_UINT32_LE (*buf + 40, asfpad->bitrate);     /* bitrate */
  GST_WRITE_UINT32_LE (*buf + 44, 0);   /* buffer size */
  GST_WRITE_UINT32_LE (*buf + 48, 0);   /* initial buffer fullness */
  GST_WRITE_UINT32_LE (*buf + 52, asfpad->bitrate);     /* alternate data bitrate */
  GST_WRITE_UINT32_LE (*buf + 56, 0);   /* alternate buffer size */
  GST_WRITE_UINT32_LE (*buf + 60, 0);   /* alternate initial buffer fullness */
  GST_WRITE_UINT32_LE (*buf + 64, 0);   /* maximum object size */

  /* flags */
  if (asfpad->is_audio) {
    /* TODO check if audio is seekable */
    GST_WRITE_UINT32_LE (*buf + 68, 0x0);
  } else {
    /* video has indexes, so it is seekable unless we are streaming */
    if (asfmux->prop_streamable)
      GST_WRITE_UINT32_LE (*buf + 68, 0x0);
    else
      GST_WRITE_UINT32_LE (*buf + 68, 0x2);
  }

  GST_WRITE_UINT16_LE (*buf + 72, asfpad->stream_number);
  GST_WRITE_UINT16_LE (*buf + 74, 0);   /* language index */
  GST_WRITE_UINT64_LE (*buf + 76, 0);   /* avg time per frame */
  GST_WRITE_UINT16_LE (*buf + 84, 0);   /* stream name count */
  GST_WRITE_UINT16_LE (*buf + 86, 0);   /* payload extension count */

  *buf += ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_SIZE;
}

/**
 * gst_asf_mux_write_string_with_size:
 * @asfmux:
 * @size_buf: pointer to the memory position to write the size of the string
 * @str_buf: pointer to the memory position to write the string
 * @str: the string to be written (in UTF-8)
 * @use32: if the string size should be written with 32 bits (if true)
 * or with 16 (if false)
 *
 * Writes a string with its size as it is needed in many asf objects.
 * The size is written to size_buf as a WORD field if use32 is false, and
 * as a DWORD if use32 is true. The string is written to str_buf in UTF16-LE.
 * The string should be passed in UTF-8.
 *
 * The string size in UTF16-LE is returned.
 */
static guint64
gst_asf_mux_write_string_with_size (GstAsfMux * asfmux,
    guint8 * size_buf, guint8 * str_buf, const gchar * str, gboolean use32)
{
  GError *error = NULL;
  gsize str_size = 0;
  gchar *str_utf16 = NULL;

  GST_LOG_OBJECT (asfmux, "Writing extended content description string: "
      "%s", str);

  /*
   * Covert the string to utf16
   * Also force the last bytes to null terminated,
   * tags were with extra weird characters without it.
   */
  str_utf16 = g_convert (str, -1, "UTF-16LE", "UTF-8", NULL, &str_size, &error);

  /* sum up the null terminating char */
  str_size += 2;

  if (use32)
    GST_WRITE_UINT32_LE (size_buf, str_size);
  else
    GST_WRITE_UINT16_LE (size_buf, str_size);
  if (error) {
    GST_WARNING_OBJECT (asfmux, "Error converting string "
        "to UTF-16: %s - %s", str, error->message);
    g_error_free (error);
    memset (str_buf, 0, str_size);
  } else {
    /* HACK: g_convert seems to add only a single byte null char to
     * the end of the stream, we force the second one */
    memcpy (str_buf, str_utf16, str_size - 1);
    str_buf[str_size - 1] = 0;
  }
  g_free (str_utf16);
  return str_size;
}

/**
 * gst_asf_mux_write_content_description_entry:
 * @asfmux:
 * @tags:
 * @tagname:
 * @size_buf:
 * @data_buf:
 *
 * Checks if a string tag with tagname exists in the taglist. If it
 * exists it is written as an UTF-16LE to data_buf and its size in bytes
 * is written to size_buf. It is used for writing content description
 * object fields.
 *
 * Returns: the size of the string
 */
static guint16
gst_asf_mux_write_content_description_entry (GstAsfMux * asfmux,
    const GstTagList * tags, const gchar * tagname,
    guint8 * size_buf, guint8 * data_buf)
{
  gchar *text = NULL;
  guint16 text_size = 0;
  if (gst_tag_list_get_string (tags, tagname, &text)) {
    text_size = gst_asf_mux_write_string_with_size (asfmux, size_buf,
        data_buf, text, FALSE);
    g_free (text);
  } else {
    GST_WRITE_UINT16_LE (size_buf, 0);
  }
  return text_size;
}

static guint64
gst_asf_mux_write_ext_content_description_dword_entry (GstAsfMux * asfmux,
    guint8 * buf, const gchar * asf_tag, const guint32 value)
{
  guint64 tag_size;
  GST_DEBUG_OBJECT (asfmux, "Writing extended content description tag: "
      "%s (%u)", asf_tag, value);

  tag_size = gst_asf_mux_write_string_with_size (asfmux, buf, buf + 2,
      asf_tag, FALSE);
  buf += tag_size + 2;
  GST_WRITE_UINT16_LE (buf, ASF_TAG_TYPE_DWORD);
  GST_WRITE_UINT16_LE (buf + 2, 4);
  GST_WRITE_UINT32_LE (buf + 4, value);

  /* tagsize -> string size
   * 2 -> string size field size
   * 4 -> dword entry
   * 4 -> type of entry + entry size
   */
  return tag_size + 2 + 4 + 4;
}

static guint64
gst_asf_mux_write_ext_content_description_string_entry (GstAsfMux * asfmux,
    guint8 * buf, const gchar * asf_tag, const gchar * text)
{
  guint64 tag_size = 0;
  guint64 text_size = 0;

  GST_DEBUG_OBJECT (asfmux, "Writing extended content description tag: "
      "%s (%s)", asf_tag, text);

  tag_size = gst_asf_mux_write_string_with_size (asfmux,
      buf, buf + 2, asf_tag, FALSE);
  GST_WRITE_UINT16_LE (buf + tag_size + 2, ASF_TAG_TYPE_UNICODE_STR);
  buf += tag_size + 2 + 2;
  text_size = gst_asf_mux_write_string_with_size (asfmux,
      buf, buf + 2, text, FALSE);

  /* the size of the strings in utf16-le plus the 3 WORD fields */
  return tag_size + text_size + 6;
}

static void
gst_asf_mux_write_content_description (GstAsfMux * asfmux, guint8 ** buf,
    const GstTagList * tags)
{
  guint8 *values = (*buf) + ASF_CONTENT_DESCRIPTION_OBJECT_SIZE;
  guint64 size = 0;

  GST_DEBUG_OBJECT (asfmux, "Writing content description object");

  gst_asf_put_guid (*buf, guids[ASF_CONTENT_DESCRIPTION_INDEX]);

  values += gst_asf_mux_write_content_description_entry (asfmux, tags,
      GST_TAG_TITLE, *buf + 24, values);
  values += gst_asf_mux_write_content_description_entry (asfmux, tags,
      GST_TAG_ARTIST, *buf + 26, values);
  values += gst_asf_mux_write_content_description_entry (asfmux, tags,
      GST_TAG_COPYRIGHT, *buf + 28, values);
  values += gst_asf_mux_write_content_description_entry (asfmux, tags,
      GST_TAG_DESCRIPTION, *buf + 30, values);

  /* rating is currently not present in gstreamer tags, so we put 0 */
  GST_WRITE_UINT16_LE (*buf + 32, 0);

  size += values - *buf;
  GST_WRITE_UINT64_LE (*buf + 16, size);
  *buf += size;
}

static void
write_ext_content_description_tag (const GstTagList * taglist,
    const gchar * tag, gpointer user_data)
{
  const gchar *asftag = gst_asf_get_asf_tag (tag);
  GValue value = { 0 };
  guint type;
  GstAsfExtContDescData *data = (GstAsfExtContDescData *) user_data;

  if (asftag == NULL)
    return;

  if (!gst_tag_list_copy_value (&value, taglist, tag)) {
    return;
  }

  type = gst_asf_get_tag_field_type (&value);
  switch (type) {
    case ASF_TAG_TYPE_UNICODE_STR:
    {
      const gchar *text;
      text = g_value_get_string (&value);
      data->size +=
          gst_asf_mux_write_ext_content_description_string_entry (data->asfmux,
          data->buf + data->size, asftag, text);
    }
      break;
    case ASF_TAG_TYPE_DWORD:
    {
      guint num = g_value_get_uint (&value);
      data->size +=
          gst_asf_mux_write_ext_content_description_dword_entry (data->asfmux,
          data->buf + data->size, asftag, num);
    }
      break;
    default:
      GST_WARNING_OBJECT (data->asfmux,
          "Unhandled asf tag field type %u for tag %s", type, tag);
      g_value_reset (&value);
      return;
  }
  data->count++;
  g_value_reset (&value);
}

static void
gst_asf_mux_write_ext_content_description (GstAsfMux * asfmux, guint8 ** buf,
    GstTagList * tags)
{
  GstAsfExtContDescData extContDesc;
  extContDesc.asfmux = asfmux;
  extContDesc.buf = *buf;
  extContDesc.count = 0;
  extContDesc.size = ASF_EXT_CONTENT_DESCRIPTION_OBJECT_SIZE;

  GST_DEBUG_OBJECT (asfmux, "Writing extended content description object");
  gst_asf_put_guid (*buf, guids[ASF_EXT_CONTENT_DESCRIPTION_INDEX]);

  gst_tag_list_foreach (tags, write_ext_content_description_tag, &extContDesc);

  GST_WRITE_UINT64_LE (*buf + 16, extContDesc.size);
  GST_WRITE_UINT16_LE (*buf + 24, extContDesc.count);

  *buf += extContDesc.size;
}

static void
write_metadata_tag (const GstTagList * taglist, const gchar * tag,
    gpointer user_data)
{
  const gchar *asftag = gst_asf_get_asf_tag (tag);
  GValue value = { 0 };
  guint type;
  GstAsfMetadataObjData *data = (GstAsfMetadataObjData *) user_data;
  guint16 tag_size;
  guint32 content_size;

  if (asftag == NULL)
    return;

  if (!gst_tag_list_copy_value (&value, taglist, tag)) {
    return;
  }

  type = gst_asf_get_tag_field_type (&value);
  switch (type) {
    case ASF_TAG_TYPE_UNICODE_STR:
    {
      const gchar *text;
      text = g_value_get_string (&value);
      GST_WRITE_UINT16_LE (data->buf + data->size, 0);
      GST_WRITE_UINT16_LE (data->buf + data->size + 2, data->stream_num);
      data->size += 4;

      tag_size = gst_asf_mux_write_string_with_size (data->asfmux,
          data->buf + data->size, data->buf + data->size + 8, asftag, FALSE);
      data->size += 2;

      GST_WRITE_UINT16_LE (data->buf + data->size, type);
      data->size += 2;

      content_size = gst_asf_mux_write_string_with_size (data->asfmux,
          data->buf + data->size, data->buf + data->size + tag_size + 4, text,
          TRUE);
      data->size += tag_size + content_size + 4;
    }
      break;
    case ASF_TAG_TYPE_DWORD:
    {
      guint num = g_value_get_uint (&value);
      GST_WRITE_UINT16_LE (data->buf + data->size, 0);
      GST_WRITE_UINT16_LE (data->buf + data->size + 2, data->stream_num);
      data->size += 4;

      tag_size = gst_asf_mux_write_string_with_size (data->asfmux,
          data->buf + data->size, data->buf + data->size + 8, asftag, FALSE);
      data->size += 2;

      GST_WRITE_UINT16_LE (data->buf + data->size, type);
      data->size += 2;
      /* dword length */
      GST_WRITE_UINT32_LE (data->buf + data->size, 4);
      data->size += 4 + tag_size;

      GST_WRITE_UINT32_LE (data->buf + data->size, num);
      data->size += 4;
    }
      break;
    default:
      GST_WARNING_OBJECT (data->asfmux,
          "Unhandled asf tag field type %u for tag %s", type, tag);
      g_value_reset (&value);
      return;
  }

  data->count++;
  g_value_reset (&value);
}

static void
gst_asf_mux_write_metadata_object (GstAsfMux * asfmux, guint8 ** buf,
    GstAsfPad * asfpad)
{
  GstAsfMetadataObjData metaObjData;
  metaObjData.asfmux = asfmux;
  metaObjData.buf = *buf;
  metaObjData.count = 0;
  metaObjData.size = ASF_METADATA_OBJECT_SIZE;
  metaObjData.stream_num = asfpad->stream_number;

  if (asfpad->taglist == NULL || gst_tag_list_is_empty (asfpad->taglist))
    return;

  GST_DEBUG_OBJECT (asfmux, "Writing metadata object");
  gst_asf_put_guid (*buf, guids[ASF_METADATA_OBJECT_INDEX]);

  gst_tag_list_foreach (asfpad->taglist, write_metadata_tag, &metaObjData);

  GST_WRITE_UINT64_LE (*buf + 16, metaObjData.size);
  GST_WRITE_UINT16_LE (*buf + 24, metaObjData.count);

  *buf += metaObjData.size;
}

static void
gst_asf_mux_write_padding_object (GstAsfMux * asfmux, guint8 ** buf,
    guint64 padding)
{
  if (padding < ASF_PADDING_OBJECT_SIZE) {
    return;
  }

  GST_DEBUG_OBJECT (asfmux, "Writing padding object of size %" G_GUINT64_FORMAT,
      padding);
  gst_asf_put_guid (*buf, guids[ASF_PADDING_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (*buf + 16, padding);
  memset (*buf + 24, 0, padding - ASF_PADDING_OBJECT_SIZE);
  *buf += padding;
}

static void
gst_asf_mux_write_data_object (GstAsfMux * asfmux, guint8 ** buf)
{
  gst_asf_put_guid (*buf, guids[ASF_DATA_OBJECT_INDEX]);

  /* Data object size. This is always >= ASF_DATA_OBJECT_SIZE. The standard
   * specifically accepts the value 0 in live streams, but WMP is not accepting
   * this while streaming using WMSP, so we default to minimum size also for
   * live streams. Otherwise this field must be updated later on when we know
   * the complete stream size.
   */
  GST_WRITE_UINT64_LE (*buf + 16, ASF_DATA_OBJECT_SIZE);

  gst_asf_put_guid (*buf + 24, asfmux->file_id);
  GST_WRITE_UINT64_LE (*buf + 40, 0);   /* total data packets */
  GST_WRITE_UINT16_LE (*buf + 48, 0x0101);      /* reserved */
  *buf += ASF_DATA_OBJECT_SIZE;
}

static void
gst_asf_mux_put_buffer_in_streamheader (GValue * streamheader,
    GstBuffer * buffer)
{
  GValue value = { 0 };
  GstBuffer *buf;

  g_value_init (&value, GST_TYPE_BUFFER);
  buf = gst_buffer_copy (buffer);
  gst_value_set_buffer (&value, buf);
  gst_buffer_unref (buf);
  gst_value_array_append_value (streamheader, &value);
  g_value_unset (&value);
}

static guint
gst_asf_mux_find_payload_parsing_info_size (GstAsfMux * asfmux)
{
  /* Minimum payload parsing information size is 8 bytes */
  guint size = 8;

  if (asfmux->prop_packet_size > 65535)
    size += 4;
  else
    size += 2;

  if (asfmux->prop_padding > 65535)
    size += 4;
  else
    size += 2;

  return size;
}

/**
 * gst_asf_mux_start_file:
 * @asfmux: #GstAsfMux
 *
 * Starts the asf file/stream by creating and pushing
 * the headers downstream.
 */
static GstFlowReturn
gst_asf_mux_start_file (GstAsfMux * asfmux)
{
  GstBuffer *buf = NULL;
  guint8 *bufdata = NULL;
  GSList *walk;
  guint stream_num = g_slist_length (asfmux->collect->data);
  guint metadata_obj_size = 0;
  GstAsfTags *asftags;
  GValue streamheader = { 0 };
  GstCaps *caps;
  GstStructure *structure;
  guint64 padding = asfmux->prop_padding;
  GstSegment segment;
  GstMapInfo map;
  gsize bufsize;
  gchar s_id[32];

  if (padding < ASF_PADDING_OBJECT_SIZE)
    padding = 0;

  /* if not streaming, check if downstream is seekable */
  if (!asfmux->prop_streamable) {
    gboolean seekable;
    GstQuery *query;

    query = gst_query_new_seeking (GST_FORMAT_BYTES);
    if (gst_pad_peer_query (asfmux->srcpad, query)) {
      gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
      GST_INFO_OBJECT (asfmux, "downstream is %sseekable",
          seekable ? "" : "not ");
    } else {
      /* assume seeking is not supported if query not handled downstream */
      GST_WARNING_OBJECT (asfmux, "downstream did not handle seeking query");
      seekable = FALSE;
    }
    if (!seekable) {
      asfmux->prop_streamable = TRUE;
      g_object_notify (G_OBJECT (asfmux), "streamable");
      GST_WARNING_OBJECT (asfmux, "downstream is not seekable, but "
          "streamable=false. Will ignore that and create streamable output "
          "instead");
    }
    gst_query_unref (query);
  }

  /* from this point we started writing the headers */
  GST_INFO_OBJECT (asfmux, "Writing headers");
  asfmux->state = GST_ASF_MUX_STATE_HEADERS;

  /* stream-start (FIXME: create id based on input ids) */
  g_snprintf (s_id, sizeof (s_id), "asfmux-%08x", g_random_int ());
  gst_pad_push_event (asfmux->srcpad, gst_event_new_stream_start (s_id));

  caps = gst_pad_get_pad_template_caps (asfmux->srcpad);
  gst_pad_set_caps (asfmux->srcpad, caps);
  gst_caps_unref (caps);

  /* send a BYTE format segment if we're going to seek to fix up the headers
   * later, otherwise send a TIME segment */
  if (asfmux->prop_streamable)
    gst_segment_init (&segment, GST_FORMAT_TIME);
  else
    gst_segment_init (&segment, GST_FORMAT_BYTES);
  gst_pad_push_event (asfmux->srcpad, gst_event_new_segment (&segment));

  gst_asf_generate_file_id (&asfmux->file_id);

  /* Get the metadata for content description object.
   * We store our own taglist because it might get changed from now
   * to the time we actually add its contents to the file, changing
   * the size of the data we already calculated here.
   */
  asftags = g_new0 (GstAsfTags, 1);
  gst_asf_mux_get_content_description_tags (asfmux, asftags);

  /* get the total metadata objects size */
  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    metadata_obj_size += gst_asf_mux_get_metadata_object_size (asfmux,
        (GstAsfPad *) walk->data);
  }

  /* alloc a buffer for all header objects */
  buf = gst_buffer_new_and_alloc (gst_asf_mux_get_headers_size (asfmux) +
      asftags->cont_desc_size +
      asftags->ext_cont_desc_size +
      metadata_obj_size + padding + ASF_DATA_OBJECT_SIZE);

  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  bufdata = map.data;
  bufsize = map.size;

  gst_asf_mux_write_header_object (asfmux, &bufdata, map.size -
      ASF_DATA_OBJECT_SIZE, 2 + stream_num);

  /* get the position of the file properties object for
   * updating it in gst_asf_mux_stop_file */
  asfmux->file_properties_object_position = bufdata - map.data;
  gst_asf_mux_write_file_properties (asfmux, &bufdata);

  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    gst_asf_mux_write_stream_properties (asfmux, &bufdata,
        (GstAsfPad *) walk->data);
  }

  if (asftags->cont_desc_size) {
    gst_asf_mux_write_content_description (asfmux, &bufdata, asftags->tags);
  }
  if (asftags->ext_cont_desc_size) {
    gst_asf_mux_write_ext_content_description (asfmux, &bufdata, asftags->tags);
  }

  if (asftags->tags)
    gst_tag_list_unref (asftags->tags);
  g_free (asftags);

  /* writing header extension objects */
  gst_asf_mux_write_header_extension (asfmux, &bufdata, stream_num *
      ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_SIZE + metadata_obj_size);
  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    gst_asf_mux_write_extended_stream_properties (asfmux, &bufdata,
        (GstAsfPad *) walk->data);
  }
  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    gst_asf_mux_write_metadata_object (asfmux, &bufdata,
        (GstAsfPad *) walk->data);
  }

  gst_asf_mux_write_padding_object (asfmux, &bufdata, padding);

  /* store data object position for later updating some fields */
  asfmux->data_object_position = bufdata - map.data;
  gst_asf_mux_write_data_object (asfmux, &bufdata);

  /* set streamheader in source pad if 'streamable' */
  if (asfmux->prop_streamable) {
    g_value_init (&streamheader, GST_TYPE_ARRAY);
    gst_asf_mux_put_buffer_in_streamheader (&streamheader, buf);

    caps = gst_pad_get_current_caps (asfmux->srcpad);
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);
    gst_structure_set_value (structure, "streamheader", &streamheader);
    gst_pad_set_caps (asfmux->srcpad, caps);
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);
    g_value_unset (&streamheader);
    gst_caps_unref (caps);
  }

  g_assert (bufdata - map.data == map.size);
  gst_buffer_unmap (buf, &map);
  return gst_asf_mux_push_buffer (asfmux, buf, bufsize);
}

/**
 * gst_asf_mux_add_simple_index_entry:
 * @asfmux:
 * @videopad:
 *
 * Adds a new entry to the simple index of the stream handler by videopad.
 * This functions doesn't check if the time elapsed
 * is larger than the established time interval between entries. The caller
 * is responsible for verifying this.
 */
static void
gst_asf_mux_add_simple_index_entry (GstAsfMux * asfmux,
    GstAsfVideoPad * videopad)
{
  SimpleIndexEntry *entry = NULL;
  GST_DEBUG_OBJECT (asfmux, "Adding new simple index entry "
      "packet number: %" G_GUINT32_FORMAT ", "
      "packet count: %" G_GUINT16_FORMAT,
      videopad->last_keyframe_packet, videopad->last_keyframe_packet_count);
  entry = g_malloc0 (sizeof (SimpleIndexEntry));
  entry->packet_number = videopad->last_keyframe_packet;
  entry->packet_count = videopad->last_keyframe_packet_count;
  if (entry->packet_count > videopad->max_keyframe_packet_count)
    videopad->max_keyframe_packet_count = entry->packet_count;
  videopad->simple_index = g_slist_append (videopad->simple_index, entry);
}

/**
 * gst_asf_mux_send_packet:
 * @asfmux:
 * @buf: The asf data packet
 *
 * Pushes an asf data packet downstream. The total number
 * of packets and bytes of the stream are incremented.
 *
 * Returns: the result of pushing the buffer downstream
 */
static GstFlowReturn
gst_asf_mux_send_packet (GstAsfMux * asfmux, GstBuffer * buf, gsize bufsize)
{
  g_assert (bufsize == asfmux->packet_size);
  asfmux->total_data_packets++;
  GST_LOG_OBJECT (asfmux,
      "Pushing a packet of size %" G_GSIZE_FORMAT " and timestamp %"
      G_GUINT64_FORMAT, bufsize, GST_BUFFER_TIMESTAMP (buf));
  GST_LOG_OBJECT (asfmux, "Total data packets: %" G_GUINT64_FORMAT,
      asfmux->total_data_packets);
  return gst_asf_mux_push_buffer (asfmux, buf, bufsize);
}

/**
 * gst_asf_mux_flush_payloads:
 * @asfmux: #GstAsfMux to flush the payloads from
 *
 * Fills an asf packet with asfmux queued payloads and
 * pushes it downstream.
 *
 * Returns: The result of pushing the packet
 */
static GstFlowReturn
gst_asf_mux_flush_payloads (GstAsfMux * asfmux)
{
  GstBuffer *buf;
  guint8 payloads_count = 0;    /* we only use 6 bits, max is 63 */
  guint i;
  GstClockTime send_ts = GST_CLOCK_TIME_NONE;
  guint64 size_left;
  guint8 *data;
  gsize size;
  GSList *walk;
  GstAsfPad *pad;
  gboolean has_keyframe;
  AsfPayload *payload;
  guint32 payload_size;
  guint offset;
  GstMapInfo map;

  if (asfmux->payloads == NULL)
    return GST_FLOW_OK;         /* nothing to send is ok */

  GST_LOG_OBJECT (asfmux, "Flushing payloads");

  buf = gst_buffer_new_and_alloc (asfmux->packet_size);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  memset (map.data, 0, asfmux->packet_size);

  /* 1 for the multiple payload flags */
  data = map.data + asfmux->payload_parsing_info_size + 1;
  size_left = asfmux->packet_size - asfmux->payload_parsing_info_size - 1;

  has_keyframe = FALSE;
  walk = asfmux->payloads;
  while (walk && payloads_count < MAX_PAYLOADS_IN_A_PACKET) {
    payload = (AsfPayload *) walk->data;
    pad = (GstAsfPad *) payload->pad;
    payload_size = gst_asf_payload_get_size (payload);
    if (size_left < payload_size) {
      break;                    /* next payload doesn't fit fully */
    }

    if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (send_ts))) {
      send_ts = GST_BUFFER_TIMESTAMP (payload->data);
    }

    /* adding new simple index entry (if needed) */
    if (!pad->is_audio
        && GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (payload->data))) {
      GstAsfVideoPad *videopad = (GstAsfVideoPad *) pad;
      if (videopad->has_keyframe) {
        for (; videopad->next_index_time <=
            ASF_MILI_TO_100NANO (payload->presentation_time);
            videopad->next_index_time += videopad->time_interval) {
          gst_asf_mux_add_simple_index_entry (asfmux, videopad);
        }
      }
    }

    /* serialize our payload */
    GST_DEBUG_OBJECT (asfmux, "Serializing payload into packet");
    GST_DEBUG_OBJECT (asfmux, "stream number: %d", pad->stream_number & 0x7F);
    GST_DEBUG_OBJECT (asfmux, "media object number: %d",
        (gint) payload->media_obj_num);
    GST_DEBUG_OBJECT (asfmux, "offset into media object: %" G_GUINT32_FORMAT,
        payload->offset_in_media_obj);
    GST_DEBUG_OBJECT (asfmux, "media object size: %" G_GUINT32_FORMAT,
        payload->media_object_size);
    GST_DEBUG_OBJECT (asfmux, "replicated data length: %d",
        (gint) payload->replicated_data_length);
    GST_DEBUG_OBJECT (asfmux, "payload size: %" G_GSIZE_FORMAT,
        gst_buffer_get_size (payload->data));
    GST_DEBUG_OBJECT (asfmux, "presentation time: %" G_GUINT32_FORMAT " (%"
        GST_TIME_FORMAT ")", payload->presentation_time,
        GST_TIME_ARGS (payload->presentation_time * GST_MSECOND));
    GST_DEBUG_OBJECT (asfmux, "keyframe: %s",
        (payload->stream_number & 0x80 ? "yes" : "no"));
    GST_DEBUG_OBJECT (asfmux, "buffer timestamp: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (payload->data)));
    GST_DEBUG_OBJECT (asfmux, "buffer duration %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_DURATION (payload->data)));

    gst_asf_put_payload (data, payload);
    if (!payload->has_packet_info) {
      payload->has_packet_info = TRUE;
      payload->packet_number = asfmux->total_data_packets;
    }
    GST_DEBUG_OBJECT (asfmux, "packet number: %" G_GUINT32_FORMAT,
        payload->packet_number);

    if (ASF_PAYLOAD_IS_KEYFRAME (payload)) {
      has_keyframe = TRUE;
      if (!pad->is_audio) {
        GstAsfVideoPad *videopad = (GstAsfVideoPad *) pad;
        videopad->last_keyframe_packet = payload->packet_number;
        videopad->last_keyframe_packet_count = payload->packet_count;
        videopad->has_keyframe = TRUE;
      }
    }

    /* update our variables */
    data += payload_size;
    size_left -= payload_size;
    payloads_count++;
    walk = g_slist_next (walk);
  }

  /* remove flushed payloads */
  GST_LOG_OBJECT (asfmux, "Freeing already used payloads");
  for (i = 0; i < payloads_count; i++) {
    GSList *aux = g_slist_nth (asfmux->payloads, 0);
    AsfPayload *payload;
    g_assert (aux);
    payload = (AsfPayload *) aux->data;
    asfmux->payloads = g_slist_remove (asfmux->payloads, payload);
    asfmux->payload_data_size -=
        (gst_buffer_get_size (payload->data) +
        ASF_MULTIPLE_PAYLOAD_HEADER_SIZE);
    gst_asf_payload_free (payload);
  }

  /* check if we can add part of the next payload */
  if (asfmux->payloads && size_left > ASF_MULTIPLE_PAYLOAD_HEADER_SIZE) {
    AsfPayload *payload =
        (AsfPayload *) g_slist_nth (asfmux->payloads, 0)->data;
    guint16 bytes_writen;
    GST_DEBUG_OBJECT (asfmux, "Adding part of a payload to a packet");

    if (ASF_PAYLOAD_IS_KEYFRAME (payload))
      has_keyframe = TRUE;

    if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (send_ts))) {
      send_ts = GST_BUFFER_TIMESTAMP (payload->data);
    }

    bytes_writen = gst_asf_put_subpayload (data, payload, size_left);
    if (!payload->has_packet_info) {
      payload->has_packet_info = TRUE;
      payload->packet_number = asfmux->total_data_packets;
    }
    asfmux->payload_data_size -= bytes_writen;
    size_left -= (bytes_writen + ASF_MULTIPLE_PAYLOAD_HEADER_SIZE);
    payloads_count++;
  }

  GST_LOG_OBJECT (asfmux, "Payload data size: %" G_GUINT32_FORMAT,
      asfmux->payload_data_size);

  /* fill payload parsing info */
  data = map.data;
  size = map.size;

  /* flags */
  GST_WRITE_UINT8 (data, (0x0 << 7) |   /* no error correction */
      (ASF_FIELD_TYPE_DWORD << 5) |     /* packet length type */
      (ASF_FIELD_TYPE_DWORD << 3) |     /* padding length type */
      (ASF_FIELD_TYPE_NONE << 1) |      /* sequence type type */
      0x1);                     /* multiple payloads */
  offset = 1;

  /* property flags - according to the spec, this should not change */
  GST_WRITE_UINT8 (data + offset, (ASF_FIELD_TYPE_BYTE << 6) |  /* stream number length type */
      (ASF_FIELD_TYPE_BYTE << 4) |      /* media obj number length type */
      (ASF_FIELD_TYPE_DWORD << 2) |     /* offset info media object length type */
      (ASF_FIELD_TYPE_BYTE));   /* replicated data length type */
  offset++;

  /* Due to a limitation in WMP while streaming through WMSP we reduce the
   * packet & padding size to 16bit if they are <= 65535 bytes
   */
  if (asfmux->packet_size > 65535) {
    GST_WRITE_UINT32_LE (data + offset, asfmux->packet_size - size_left);
    offset += 4;
  } else {
    *data &= ~(ASF_FIELD_TYPE_MASK << 5);
    *data |= ASF_FIELD_TYPE_WORD << 5;
    GST_WRITE_UINT16_LE (data + offset, asfmux->packet_size - size_left);
    offset += 2;
  }
  if (asfmux->prop_padding > 65535) {
    GST_WRITE_UINT32_LE (data + offset, size_left);
    offset += 4;
  } else {
    *data &= ~(ASF_FIELD_TYPE_MASK << 3);
    *data |= ASF_FIELD_TYPE_WORD << 3;
    GST_WRITE_UINT16_LE (data + offset, size_left);
    offset += 2;
  }

  /* packet send time */
  if (GST_CLOCK_TIME_IS_VALID (send_ts)) {
    GST_WRITE_UINT32_LE (data + offset, (send_ts / GST_MSECOND));
    GST_BUFFER_TIMESTAMP (buf) = send_ts;
  }
  offset += 4;

  /* packet duration */
  GST_WRITE_UINT16_LE (data + offset, 0);       /* FIXME send duration needs to be estimated */
  offset += 2;

  /* multiple payloads flags */
  GST_WRITE_UINT8 (data + offset, 0x2 << 6 | payloads_count);
  gst_buffer_unmap (buf, &map);

  if (payloads_count == 0) {
    GST_WARNING_OBJECT (asfmux, "Sending packet without any payload");
  }
  asfmux->data_object_size += size;

  if (!has_keyframe)
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

  return gst_asf_mux_send_packet (asfmux, buf, size);
}

/**
 * stream_number_compare:
 * @a: a #GstAsfPad
 * @b: another #GstAsfPad
 *
 * Utility function to compare #GstAsfPad by their stream numbers
 *
 * Returns: The difference between their stream numbers
 */
static gint
stream_number_compare (gconstpointer a, gconstpointer b)
{
  GstAsfPad *pad_a = (GstAsfPad *) a;
  GstAsfPad *pad_b = (GstAsfPad *) b;
  return pad_b->stream_number - pad_a->stream_number;
}

static GstFlowReturn
gst_asf_mux_push_simple_index (GstAsfMux * asfmux, GstAsfVideoPad * pad)
{
  guint64 object_size = ASF_SIMPLE_INDEX_OBJECT_SIZE +
      g_slist_length (pad->simple_index) * ASF_SIMPLE_INDEX_ENTRY_SIZE;
  GstBuffer *buf;
  GSList *walk;
  guint8 *data;
  guint32 entries_count = g_slist_length (pad->simple_index);
  GstMapInfo map;
  gsize bufsize;

  buf = gst_buffer_new_and_alloc (object_size);
  bufsize = object_size;

  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;

  gst_asf_put_guid (data, guids[ASF_SIMPLE_INDEX_OBJECT_INDEX]);
  GST_WRITE_UINT64_LE (data + 16, object_size);
  gst_asf_put_guid (data + 24, asfmux->file_id);
  GST_WRITE_UINT64_LE (data + 40, pad->time_interval);
  GST_WRITE_UINT32_LE (data + 48, pad->max_keyframe_packet_count);
  GST_WRITE_UINT32_LE (data + 52, entries_count);
  data += ASF_SIMPLE_INDEX_OBJECT_SIZE;

  GST_DEBUG_OBJECT (asfmux,
      "Simple index object values - size:%" G_GUINT64_FORMAT ", time interval:%"
      G_GUINT64_FORMAT ", max packet count:%" G_GUINT32_FORMAT ", entries:%"
      G_GUINT32_FORMAT, object_size, pad->time_interval,
      pad->max_keyframe_packet_count, entries_count);

  for (walk = pad->simple_index; walk; walk = g_slist_next (walk)) {
    SimpleIndexEntry *entry = (SimpleIndexEntry *) walk->data;
    GST_DEBUG_OBJECT (asfmux, "Simple index entry: packet_number:%"
        G_GUINT32_FORMAT " packet_count:%" G_GUINT16_FORMAT,
        entry->packet_number, entry->packet_count);
    GST_WRITE_UINT32_LE (data, entry->packet_number);
    GST_WRITE_UINT16_LE (data + 4, entry->packet_count);
    data += ASF_SIMPLE_INDEX_ENTRY_SIZE;
  }

  GST_DEBUG_OBJECT (asfmux, "Pushing the simple index");
  g_assert (data - map.data == object_size);
  gst_buffer_unmap (buf, &map);
  return gst_asf_mux_push_buffer (asfmux, buf, bufsize);
}

static GstFlowReturn
gst_asf_mux_write_indexes (GstAsfMux * asfmux)
{
  GSList *ordered_pads;
  GSList *walker;
  GstFlowReturn ret = GST_FLOW_OK;

  /* write simple indexes for video medias */
  ordered_pads =
      g_slist_sort (g_slist_copy (asfmux->collect->data),
      (GCompareFunc) stream_number_compare);
  for (walker = ordered_pads; walker; walker = g_slist_next (walker)) {
    GstAsfPad *pad = (GstAsfPad *) walker->data;
    if (!pad->is_audio) {
      ret = gst_asf_mux_push_simple_index (asfmux, (GstAsfVideoPad *) pad);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (asfmux, "Failed to write simple index for stream %"
            G_GUINT16_FORMAT, (guint16) pad->stream_number);
        goto cleanup_and_return;
      }
    }
  }
cleanup_and_return:
  g_slist_free (ordered_pads);
  return ret;
}

/**
 * gst_asf_mux_stop_file:
 * @asfmux: #GstAsfMux
 *
 * Finalizes the asf stream by pushing the indexes after
 * the data object. Also seeks back to the header positions
 * to rewrite some fields such as the total number of bytes
 * of the file, or any other that couldn't be predicted/known
 * back on the header generation.
 *
 * Returns: GST_FLOW_OK on success
 */
static GstFlowReturn
gst_asf_mux_stop_file (GstAsfMux * asfmux)
{
  GstEvent *event;
  GstBuffer *buf;
  GstFlowReturn ret = GST_FLOW_OK;
  GSList *walk;
  GstClockTime play_duration = 0;
  guint32 bitrate = 0;
  GstSegment segment;
  GstMapInfo map;
  guint8 *data;

  /* write indexes */
  ret = gst_asf_mux_write_indexes (asfmux);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (asfmux, "Failed to write indexes");
    return ret;
  }

  /* find max stream duration and bitrate */
  for (walk = asfmux->collect->data; walk; walk = g_slist_next (walk)) {
    GstAsfPad *pad = (GstAsfPad *) walk->data;
    bitrate += pad->bitrate;
    if (pad->play_duration > play_duration)
      play_duration = pad->play_duration;
  }

  /* going back to file properties object to fill in
   * values we didn't know back then */
  GST_DEBUG_OBJECT (asfmux,
      "Sending new segment to file properties object position");
  gst_segment_init (&segment, GST_FORMAT_BYTES);
  segment.start = segment.position =
      asfmux->file_properties_object_position + 40;
  event = gst_event_new_segment (&segment);
  if (!gst_pad_push_event (asfmux->srcpad, event)) {
    GST_ERROR_OBJECT (asfmux, "Failed to update file properties object");
    return GST_FLOW_ERROR;
  }
  /* All file properties fields except the first 40 bytes */
  buf = gst_buffer_new_and_alloc (ASF_FILE_PROPERTIES_OBJECT_SIZE - 40);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;

  GST_WRITE_UINT64_LE (data, asfmux->file_size);
  gst_asf_put_time (data + 8, gst_asf_get_current_time ());
  GST_WRITE_UINT64_LE (data + 16, asfmux->total_data_packets);
  GST_WRITE_UINT64_LE (data + 24, (play_duration / 100) +
      ASF_MILI_TO_100NANO (asfmux->preroll));
  GST_WRITE_UINT64_LE (data + 32, (play_duration / 100));       /* TODO send duration */

  /* if play duration is smaller then preroll, player might have problems */
  if (asfmux->preroll > play_duration / GST_MSECOND) {
    GST_ELEMENT_WARNING (asfmux, STREAM, MUX, (_("Generated file has a larger"
                " preroll time than its streams duration")),
        ("Preroll time larger than streams duration, "
            "try setting a smaller preroll value next time"));
  }
  GST_WRITE_UINT64_LE (data + 40, asfmux->preroll);
  GST_WRITE_UINT32_LE (data + 48, 0x2); /* flags - seekable */
  GST_WRITE_UINT32_LE (data + 52, asfmux->packet_size);
  GST_WRITE_UINT32_LE (data + 56, asfmux->packet_size);
  /* FIXME - we want the max instantaneous bitrate, for vbr streams, we can't
   * get it this way, this would be the average, right? */
  GST_WRITE_UINT32_LE (data + 60, bitrate);     /* max bitrate */
  gst_buffer_unmap (buf, &map);

  /* we don't use gst_asf_mux_push_buffer because we are overwriting
   * already sent data */
  ret = gst_pad_push (asfmux->srcpad, buf);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (asfmux, "Failed to update file properties object");
    return ret;
  }

  GST_DEBUG_OBJECT (asfmux, "Seeking back to data object");

  /* seek back to the data object */
  segment.start = segment.position = asfmux->data_object_position + 16;
  event = gst_event_new_segment (&segment);
  if (!gst_pad_push_event (asfmux->srcpad, event)) {
    GST_ERROR_OBJECT (asfmux, "Seek to update data object failed");
    return GST_FLOW_ERROR;
  }

  buf = gst_buffer_new_and_alloc (32);  /* qword+guid+qword */
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  data = map.data;
  GST_WRITE_UINT64_LE (data, asfmux->data_object_size + ASF_DATA_OBJECT_SIZE);
  gst_asf_put_guid (data + 8, asfmux->file_id);
  GST_WRITE_UINT64_LE (data + 24, asfmux->total_data_packets);
  gst_buffer_unmap (buf, &map);

  return gst_pad_push (asfmux->srcpad, buf);
}

/**
 * gst_asf_mux_process_buffer:
 * @asfmux:
 * @pad: stream of the buffer
 * @buf: The buffer to be processed
 *
 * Processes the buffer by parsing it and
 * queueing it up as an asf payload for later
 * being added and pushed inside an asf packet.
 *
 * Returns: a #GstFlowReturn
 */
static GstFlowReturn
gst_asf_mux_process_buffer (GstAsfMux * asfmux, GstAsfPad * pad,
    GstBuffer * buf)
{
  guint8 keyframe;
  AsfPayload *payload;

  payload = g_malloc0 (sizeof (AsfPayload));
  payload->pad = (GstCollectData *) pad;
  payload->data = buf;

  GST_LOG_OBJECT (asfmux, "Processing payload data for stream number %u",
      pad->stream_number);

  /* stream number */
  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT)) {
    keyframe = 0;
  } else {
    keyframe = 0x1 << 7;
  }
  payload->stream_number = keyframe | pad->stream_number;

  payload->media_obj_num = pad->media_object_number;
  payload->offset_in_media_obj = 0;
  payload->replicated_data_length = 8;

  /* replicated data - 1) media object size */
  payload->media_object_size = gst_buffer_get_size (buf);
  /* replicated data - 2) presentation time */
  if (!GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buf))) {
    GST_ERROR_OBJECT (asfmux, "Received buffer without timestamp");
    gst_asf_payload_free (payload);
    return GST_FLOW_ERROR;
  }

  g_assert (GST_CLOCK_TIME_IS_VALID (asfmux->first_ts));
  g_assert (GST_CLOCK_TIME_IS_VALID (pad->first_ts));

  payload->presentation_time = asfmux->preroll +
      ((GST_BUFFER_TIMESTAMP (buf) - asfmux->first_ts) / GST_MSECOND);

  /* update counting values */
  pad->media_object_number = (pad->media_object_number + 1) % 256;
  if (GST_BUFFER_DURATION (buf) != GST_CLOCK_TIME_NONE) {
    pad->play_duration += GST_BUFFER_DURATION (buf);
  } else {
    GST_WARNING_OBJECT (asfmux, "Received buffer without duration, it will not "
        "be accounted in the total file time");
  }

  asfmux->payloads = g_slist_append (asfmux->payloads, payload);
  asfmux->payload_data_size +=
      gst_buffer_get_size (buf) + ASF_MULTIPLE_PAYLOAD_HEADER_SIZE;
  GST_LOG_OBJECT (asfmux, "Payload data size: %" G_GUINT32_FORMAT,
      asfmux->payload_data_size);

  while (asfmux->payload_data_size + asfmux->payload_parsing_info_size >=
      asfmux->packet_size) {
    GstFlowReturn ret = gst_asf_mux_flush_payloads (asfmux);
    if (ret != GST_FLOW_OK)
      return ret;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_asf_mux_collected (GstCollectPads * collect, gpointer data)
{
  GstAsfMux *asfmux = GST_ASF_MUX_CAST (data);
  GstFlowReturn ret = GST_FLOW_OK;
  GstAsfPad *best_pad = NULL;
  GstClockTime best_time = GST_CLOCK_TIME_NONE;
  GstBuffer *buf = NULL;
  GSList *walk;

  if (G_UNLIKELY (asfmux->state == GST_ASF_MUX_STATE_NONE)) {
    ret = gst_asf_mux_start_file (asfmux);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (asfmux, "Failed to send headers");
      return ret;
    } else {
      asfmux->state = GST_ASF_MUX_STATE_DATA;
    }
  }

  if (G_UNLIKELY (asfmux->state == GST_ASF_MUX_STATE_EOS))
    return GST_FLOW_EOS;

  /* select the earliest buffer */
  walk = asfmux->collect->data;
  while (walk) {
    GstAsfPad *pad;
    GstCollectData *data;
    GstClockTime time;

    data = (GstCollectData *) walk->data;
    pad = (GstAsfPad *) data;

    walk = g_slist_next (walk);

    buf = gst_collect_pads_peek (collect, data);
    if (buf == NULL) {
      GST_LOG_OBJECT (asfmux, "Pad %s has no buffers",
          GST_PAD_NAME (pad->collect.pad));
      continue;
    }
    time = GST_BUFFER_TIMESTAMP (buf);

    /* check the ts for getting the first time */
    if (!GST_CLOCK_TIME_IS_VALID (pad->first_ts) &&
        GST_CLOCK_TIME_IS_VALID (time)) {
      GST_DEBUG_OBJECT (asfmux,
          "First ts for stream number %u: %" GST_TIME_FORMAT,
          pad->stream_number, GST_TIME_ARGS (time));
      pad->first_ts = time;
      if (!GST_CLOCK_TIME_IS_VALID (asfmux->first_ts) ||
          time < asfmux->first_ts) {
        GST_DEBUG_OBJECT (asfmux, "New first ts for file %" GST_TIME_FORMAT,
            GST_TIME_ARGS (time));
        asfmux->first_ts = time;
      }
    }

    gst_buffer_unref (buf);

    if (best_pad == NULL || !GST_CLOCK_TIME_IS_VALID (time) ||
        (GST_CLOCK_TIME_IS_VALID (best_time) && time < best_time)) {
      best_pad = pad;
      best_time = time;
    }
  }

  if (best_pad != NULL) {
    /* we have data */
    GST_LOG_OBJECT (asfmux, "selected pad %s with time %" GST_TIME_FORMAT,
        GST_PAD_NAME (best_pad->collect.pad), GST_TIME_ARGS (best_time));
    buf = gst_collect_pads_pop (collect, &best_pad->collect);
    ret = gst_asf_mux_process_buffer (asfmux, best_pad, buf);
  } else {
    /* no data, let's finish it up */
    while (asfmux->payloads) {
      ret = gst_asf_mux_flush_payloads (asfmux);
      if (ret != GST_FLOW_OK) {
        return ret;
      }
    }
    g_assert (asfmux->payloads == NULL);
    g_assert (asfmux->payload_data_size == 0);
    /* in not on 'streamable' mode we need to push indexes
     * and update headers */
    if (!asfmux->prop_streamable) {
      ret = gst_asf_mux_stop_file (asfmux);
    }
    if (ret == GST_FLOW_OK) {
      gst_pad_push_event (asfmux->srcpad, gst_event_new_eos ());
      ret = GST_FLOW_EOS;
    }
    asfmux->state = GST_ASF_MUX_STATE_EOS;
  }

  return ret;
}

static void
gst_asf_mux_pad_reset (GstAsfPad * pad)
{
  pad->stream_number = 0;
  pad->media_object_number = 0;
  pad->play_duration = (GstClockTime) 0;
  pad->bitrate = 0;
  if (pad->codec_data)
    gst_buffer_unref (pad->codec_data);
  pad->codec_data = NULL;
  if (pad->taglist)
    gst_tag_list_unref (pad->taglist);
  pad->taglist = NULL;

  pad->first_ts = GST_CLOCK_TIME_NONE;

  if (pad->is_audio) {
    GstAsfAudioPad *audiopad = (GstAsfAudioPad *) pad;
    audiopad->audioinfo.rate = 0;
    audiopad->audioinfo.channels = 0;
    audiopad->audioinfo.format = 0;
    audiopad->audioinfo.av_bps = 0;
    audiopad->audioinfo.blockalign = 0;
    audiopad->audioinfo.bits_per_sample = 0;
  } else {
    GstAsfVideoPad *videopad = (GstAsfVideoPad *) pad;
    videopad->vidinfo.size = 0;
    videopad->vidinfo.width = 0;
    videopad->vidinfo.height = 0;
    videopad->vidinfo.planes = 1;
    videopad->vidinfo.bit_cnt = 0;
    videopad->vidinfo.compression = 0;
    videopad->vidinfo.image_size = 0;
    videopad->vidinfo.xpels_meter = 0;
    videopad->vidinfo.ypels_meter = 0;
    videopad->vidinfo.num_colors = 0;
    videopad->vidinfo.imp_colors = 0;

    videopad->last_keyframe_packet = 0;
    videopad->has_keyframe = FALSE;
    videopad->last_keyframe_packet_count = 0;
    videopad->max_keyframe_packet_count = 0;
    videopad->next_index_time = 0;
    videopad->time_interval = DEFAULT_SIMPLE_INDEX_TIME_INTERVAL;
    if (videopad->simple_index) {
      GSList *walk;
      for (walk = videopad->simple_index; walk; walk = g_slist_next (walk)) {
        g_free (walk->data);
        walk->data = NULL;
      }
      g_slist_free (videopad->simple_index);
    }
    videopad->simple_index = NULL;
  }
}

static gboolean
gst_asf_mux_audio_set_caps (GstPad * pad, GstCaps * caps)
{
  GstAsfMux *asfmux;
  GstAsfAudioPad *audiopad;
  GstStructure *structure;
  const gchar *caps_name;
  gint channels, rate;
  gchar *aux;
  const GValue *codec_data;

  asfmux = GST_ASF_MUX (gst_pad_get_parent (pad));

  audiopad = (GstAsfAudioPad *) gst_pad_get_element_private (pad);
  g_assert (audiopad);

  aux = gst_caps_to_string (caps);
  GST_DEBUG_OBJECT (asfmux, "%s:%s, caps=%s", GST_DEBUG_PAD_NAME (pad), aux);
  g_free (aux);

  structure = gst_caps_get_structure (caps, 0);
  caps_name = gst_structure_get_name (structure);

  if (!gst_structure_get_int (structure, "channels", &channels) ||
      !gst_structure_get_int (structure, "rate", &rate))
    goto refuse_caps;

  audiopad->audioinfo.channels = (guint16) channels;
  audiopad->audioinfo.rate = (guint32) rate;

  /* taken from avimux
   * codec initialization data, if any
   */
  codec_data = gst_structure_get_value (structure, "codec_data");
  if (codec_data) {
    audiopad->pad.codec_data = gst_value_get_buffer (codec_data);
    gst_buffer_ref (audiopad->pad.codec_data);
  }

  if (strcmp (caps_name, "audio/x-wma") == 0) {
    gint version;
    gint block_align = 0;
    gint bitrate = 0;

    if (!gst_structure_get_int (structure, "wmaversion", &version)) {
      goto refuse_caps;
    }

    if (gst_structure_get_int (structure, "block_align", &block_align)) {
      audiopad->audioinfo.blockalign = (guint16) block_align;
    }
    if (gst_structure_get_int (structure, "bitrate", &bitrate)) {
      audiopad->pad.bitrate = (guint32) bitrate;
      audiopad->audioinfo.av_bps = bitrate / 8;
    }

    if (version == 1) {
      audiopad->audioinfo.format = GST_RIFF_WAVE_FORMAT_WMAV1;
    } else if (version == 2) {
      audiopad->audioinfo.format = GST_RIFF_WAVE_FORMAT_WMAV2;
    } else if (version == 3) {
      audiopad->audioinfo.format = GST_RIFF_WAVE_FORMAT_WMAV3;
    } else {
      goto refuse_caps;
    }
  } else if (strcmp (caps_name, "audio/mpeg") == 0) {
    gint version;
    gint layer;

    if (!gst_structure_get_int (structure, "mpegversion", &version) ||
        !gst_structure_get_int (structure, "layer", &layer)) {
      goto refuse_caps;
    }
    if (version != 1 || layer != 3) {
      goto refuse_caps;
    }

    audiopad->audioinfo.format = GST_RIFF_WAVE_FORMAT_MPEGL3;
  } else {
    goto refuse_caps;
  }

  gst_object_unref (asfmux);
  return TRUE;

refuse_caps:
  GST_WARNING_OBJECT (asfmux, "pad %s refused caps %" GST_PTR_FORMAT,
      GST_PAD_NAME (pad), caps);
  gst_object_unref (asfmux);
  return FALSE;
}

/* TODO Read pixel aspect ratio */
static gboolean
gst_asf_mux_video_set_caps (GstPad * pad, GstCaps * caps)
{
  GstAsfMux *asfmux;
  GstAsfVideoPad *videopad;
  GstStructure *structure;
  const gchar *caps_name;
  gint width, height;
  gchar *aux;
  const GValue *codec_data;

  asfmux = GST_ASF_MUX (gst_pad_get_parent (pad));

  videopad = (GstAsfVideoPad *) gst_pad_get_element_private (pad);
  g_assert (videopad);

  aux = gst_caps_to_string (caps);
  GST_DEBUG_OBJECT (asfmux, "%s:%s, caps=%s", GST_DEBUG_PAD_NAME (pad), aux);
  g_free (aux);

  structure = gst_caps_get_structure (caps, 0);
  caps_name = gst_structure_get_name (structure);

  if (!gst_structure_get_int (structure, "width", &width) ||
      !gst_structure_get_int (structure, "height", &height))
    goto refuse_caps;

  videopad->vidinfo.width = (gint32) width;
  videopad->vidinfo.height = (gint32) height;

  /* taken from avimux
   * codec initialization data, if any
   */
  codec_data = gst_structure_get_value (structure, "codec_data");
  if (codec_data) {
    videopad->pad.codec_data = gst_value_get_buffer (codec_data);
    gst_buffer_ref (videopad->pad.codec_data);
  }

  if (strcmp (caps_name, "video/x-wmv") == 0) {
    gint wmvversion;
    const gchar *fstr;

    videopad->vidinfo.bit_cnt = 24;

    /* in case we have a format, we use it */
    fstr = gst_structure_get_string (structure, "format");
    if (fstr && strlen (fstr) == 4) {
      videopad->vidinfo.compression = GST_STR_FOURCC (fstr);
    } else if (gst_structure_get_int (structure, "wmvversion", &wmvversion)) {
      if (wmvversion == 2) {
        videopad->vidinfo.compression = GST_MAKE_FOURCC ('W', 'M', 'V', '2');
      } else if (wmvversion == 1) {
        videopad->vidinfo.compression = GST_MAKE_FOURCC ('W', 'M', 'V', '1');
      } else if (wmvversion == 3) {
        videopad->vidinfo.compression = GST_MAKE_FOURCC ('W', 'M', 'V', '3');
      }
    } else
      goto refuse_caps;
  } else {
    goto refuse_caps;
  }

  gst_object_unref (asfmux);
  return TRUE;

refuse_caps:
  GST_WARNING_OBJECT (asfmux, "pad %s refused caps %" GST_PTR_FORMAT,
      GST_PAD_NAME (pad), caps);
  gst_object_unref (asfmux);
  return FALSE;
}

static GstPad *
gst_asf_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  GstAsfMux *asfmux = GST_ASF_MUX_CAST (element);
  GstPad *newpad;
  GstAsfPad *collect_pad;
  gboolean is_audio;
  guint collect_size = 0;
  gchar *name = NULL;
  const gchar *pad_name = NULL;
  guint pad_id;

  GST_DEBUG_OBJECT (asfmux, "Requested pad: %s", GST_STR_NULL (req_name));

  if (asfmux->state != GST_ASF_MUX_STATE_NONE) {
    GST_WARNING_OBJECT (asfmux, "Not providing request pad after element is at "
        "paused/playing state.");
    return NULL;
  }

  if (templ == gst_element_class_get_pad_template (klass, "audio_%u")) {
    /* don't mix named and unnamed pads, if the pad already exists we fail when
     * trying to add it */
    if (req_name != NULL && sscanf (req_name, "audio_%u", &pad_id) == 1) {
      pad_name = req_name;
    } else {
      name = g_strdup_printf ("audio_%u", asfmux->stream_number + 1);
      pad_name = name;
    }
    GST_DEBUG_OBJECT (asfmux, "Adding new pad %s", name);
    newpad = gst_pad_new_from_template (templ, pad_name);
    is_audio = TRUE;
  } else if (templ == gst_element_class_get_pad_template (klass, "video_%u")) {
    /* don't mix named and unnamed pads, if the pad already exists we fail when
     * trying to add it */
    if (req_name != NULL && sscanf (req_name, "video_%u", &pad_id) == 1) {
      pad_name = req_name;
    } else {
      name = g_strdup_printf ("video_%u", asfmux->stream_number + 1);
      pad_name = name;
    }
    GST_DEBUG_OBJECT (asfmux, "Adding new pad %s", name);
    newpad = gst_pad_new_from_template (templ, name);
    is_audio = FALSE;
  } else {
    GST_WARNING_OBJECT (asfmux, "This is not our template!");
    return NULL;
  }

  g_free (name);

  /* add pad to collections */
  if (is_audio) {
    collect_size = sizeof (GstAsfAudioPad);
  } else {
    collect_size = sizeof (GstAsfVideoPad);
  }
  collect_pad = (GstAsfPad *)
      gst_collect_pads_add_pad (asfmux->collect, newpad, collect_size,
      (GstCollectDataDestroyNotify) (gst_asf_mux_pad_reset), TRUE);

  /* set up pad */
  collect_pad->is_audio = is_audio;
  if (!is_audio)
    ((GstAsfVideoPad *) collect_pad)->simple_index = NULL;
  collect_pad->taglist = NULL;
  gst_asf_mux_pad_reset (collect_pad);

  /* set pad stream number */
  asfmux->stream_number += 1;
  collect_pad->stream_number = asfmux->stream_number;

  gst_pad_set_active (newpad, TRUE);
  gst_element_add_pad (element, newpad);

  return newpad;
}

static void
gst_asf_mux_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstAsfMux *asfmux;

  asfmux = GST_ASF_MUX (object);
  switch (prop_id) {
    case PROP_PACKET_SIZE:
      g_value_set_uint (value, asfmux->prop_packet_size);
      break;
    case PROP_PREROLL:
      g_value_set_uint64 (value, asfmux->prop_preroll);
      break;
    case PROP_MERGE_STREAM_TAGS:
      g_value_set_boolean (value, asfmux->prop_merge_stream_tags);
      break;
    case PROP_PADDING:
      g_value_set_uint64 (value, asfmux->prop_padding);
      break;
    case PROP_STREAMABLE:
      g_value_set_boolean (value, asfmux->prop_streamable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_asf_mux_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstAsfMux *asfmux;

  asfmux = GST_ASF_MUX (object);
  switch (prop_id) {
    case PROP_PACKET_SIZE:
      asfmux->prop_packet_size = g_value_get_uint (value);
      break;
    case PROP_PREROLL:
      asfmux->prop_preroll = g_value_get_uint64 (value);
      break;
    case PROP_MERGE_STREAM_TAGS:
      asfmux->prop_merge_stream_tags = g_value_get_boolean (value);
      break;
    case PROP_PADDING:
      asfmux->prop_padding = g_value_get_uint64 (value);
      break;
    case PROP_STREAMABLE:
      asfmux->prop_streamable = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_asf_mux_change_state (GstElement * element, GstStateChange transition)
{
  GstAsfMux *asfmux;
  GstStateChangeReturn ret;

  asfmux = GST_ASF_MUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* TODO - check if it is possible to mux 2 files without going
       * through here */
      asfmux->payload_parsing_info_size =
          gst_asf_mux_find_payload_parsing_info_size (asfmux);
      asfmux->packet_size = asfmux->prop_packet_size;
      asfmux->preroll = asfmux->prop_preroll;
      asfmux->merge_stream_tags = asfmux->prop_merge_stream_tags;
      gst_collect_pads_start (asfmux->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (asfmux->collect);
      asfmux->state = GST_ASF_MUX_STATE_NONE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto done;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

done:
  return ret;
}
