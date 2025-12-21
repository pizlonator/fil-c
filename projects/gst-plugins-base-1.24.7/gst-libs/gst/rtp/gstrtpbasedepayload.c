/* GStreamer
 * Copyright (C) <2005> Philippe Khalaf <burger@speedy.org>
 * Copyright (C) <2005> Nokia Corporation <kai.vehmanen@nokia.com>
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
 * SECTION:gstrtpbasedepayload
 * @title: GstRTPBaseDepayload
 * @short_description: Base class for RTP depayloader
 *
 * Provides a base class for RTP depayloaders
 *
 * In order to handle RTP header extensions correctly if the
 * depayloader aggregates multiple RTP packet payloads into one output
 * buffer this class provides the function
 * gst_rtp_base_depayload_set_aggregate_hdrext_enabled(). If the
 * aggregation is enabled the virtual functions
 * @GstRTPBaseDepayload.process or
 * @GstRTPBaseDepayload.process_rtp_packet must tell the base class
 * what happens to the current RTP packet. By default the base class
 * assumes that the packet payload is used with the next output
 * buffer.
 *
 * If the RTP packet will not be used with an output buffer
 * gst_rtp_base_depayload_dropped() must be called. A typical
 * situation would be if we are waiting for a keyframe.
 *
 * If the RTP packet will be used but not with the current output
 * buffer but with the next one gst_rtp_base_depayload_delayed() must
 * be called. This may happen if the current RTP packet signals the
 * start of a new output buffer and the currently processed output
 * buffer will be pushed first. The undelay happens implicitly once
 * the current buffer has been pushed or
 * gst_rtp_base_depayload_flush() has been called.
 *
 * If gst_rtp_base_depayload_flush() is called all RTP packets that
 * have not been dropped since the last output buffer are dropped,
 * e.g. if an output buffer is discarded due to malformed data. This
 * may or may not include the current RTP packet depending on the 2nd
 * parameter @keep_current.
 *
 * Be aware that in case gst_rtp_base_depayload_push_list() is used
 * each buffer will see the same list of RTP header extensions.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtpbasedepayload.h"
#include "gstrtpmeta.h"
#include "gstrtphdrext.h"

GST_DEBUG_CATEGORY_STATIC (rtpbasedepayload_debug);
#define GST_CAT_DEFAULT (rtpbasedepayload_debug)

static GstStaticCaps ntp_reference_timestamp_caps =
GST_STATIC_CAPS ("timestamp/x-ntp");

struct _GstRTPBaseDepayloadPrivate
{
  GstClockTime npt_start;
  GstClockTime npt_stop;
  gdouble play_speed;
  gdouble play_scale;
  guint clock_base;
  gboolean onvif_mode;

  gboolean discont;
  GstClockTime pts;
  GstClockTime dts;
  GstClockTime duration;

  GstClockTime ref_ts;

  guint32 last_ssrc;
  guint32 last_seqnum;
  guint32 last_rtptime;
  guint32 next_seqnum;
  gint max_reorder;
  gboolean auto_hdr_ext;

  gboolean negotiated;

  GstCaps *last_caps;
  GstEvent *segment_event;
  guint32 segment_seqnum;       /* Note: this is a GstEvent seqnum */

  gboolean source_info;
  GstBuffer *input_buffer;

  GstFlowReturn process_flow_ret;

  /* array of GstRTPHeaderExtension's * */
  GPtrArray *header_exts;

  /* maintain buffer list for header extensions read() */
  gboolean hdrext_aggregate;
  gboolean hdrext_seen;
  GstBufferList *hdrext_buffers;
  GstBuffer *hdrext_delayed;
  GstBuffer *hdrext_outbuf;
  gboolean hdrext_read_result;
};

/* Filter signals and args */
enum
{
  SIGNAL_0,
  SIGNAL_REQUEST_EXTENSION,
  SIGNAL_ADD_EXTENSION,
  SIGNAL_CLEAR_EXTENSIONS,
  LAST_SIGNAL
};

static guint gst_rtp_base_depayload_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_SOURCE_INFO FALSE
#define DEFAULT_MAX_REORDER 100
#define DEFAULT_AUTO_HEADER_EXTENSION TRUE

enum
{
  PROP_0,
  PROP_STATS,
  PROP_SOURCE_INFO,
  PROP_MAX_REORDER,
  PROP_AUTO_HEADER_EXTENSION,
  PROP_EXTENSIONS,
  PROP_LAST
};

static GParamSpec *gst_rtp_base_depayload_extensions_pspec;

static void gst_rtp_base_depayload_finalize (GObject * object);
static void gst_rtp_base_depayload_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_rtp_base_depayload_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_rtp_base_depayload_chain (GstPad * pad,
    GstObject * parent, GstBuffer * in);
static GstFlowReturn gst_rtp_base_depayload_chain_list (GstPad * pad,
    GstObject * parent, GstBufferList * list);
static gboolean gst_rtp_base_depayload_handle_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

static GstStateChangeReturn gst_rtp_base_depayload_change_state (GstElement *
    element, GstStateChange transition);

static gboolean gst_rtp_base_depayload_packet_lost (GstRTPBaseDepayload *
    filter, GstEvent * event);
static gboolean gst_rtp_base_depayload_handle_event (GstRTPBaseDepayload *
    filter, GstEvent * event);

static GstElementClass *parent_class = NULL;
static gint private_offset = 0;

static void gst_rtp_base_depayload_class_init (GstRTPBaseDepayloadClass *
    klass);
static void gst_rtp_base_depayload_init (GstRTPBaseDepayload * rtpbasepayload,
    GstRTPBaseDepayloadClass * klass);
static GstEvent *create_segment_event (GstRTPBaseDepayload * filter,
    guint rtptime, GstClockTime position);

static void gst_rtp_base_depayload_add_extension (GstRTPBaseDepayload *
    rtpbasepayload, GstRTPHeaderExtension * ext);
static void gst_rtp_base_depayload_clear_extensions (GstRTPBaseDepayload *
    rtpbasepayload);

static gboolean gst_rtp_base_depayload_operate_hdrext_buffer (GstBuffer **
    buffer, guint idx, gpointer depayloader);
static void gst_rtp_base_depayload_reset_hdrext_buffers (GstRTPBaseDepayload *
    rtpbasepayload);

GType
gst_rtp_base_depayload_get_type (void)
{
  static GType rtp_base_depayload_type = 0;

  if (g_once_init_enter ((gsize *) & rtp_base_depayload_type)) {
    static const GTypeInfo rtp_base_depayload_info = {
      sizeof (GstRTPBaseDepayloadClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_rtp_base_depayload_class_init,
      NULL,
      NULL,
      sizeof (GstRTPBaseDepayload),
      0,
      (GInstanceInitFunc) gst_rtp_base_depayload_init,
    };
    GType _type;

    _type = g_type_register_static (GST_TYPE_ELEMENT, "GstRTPBaseDepayload",
        &rtp_base_depayload_info, G_TYPE_FLAG_ABSTRACT);

    private_offset =
        g_type_add_instance_private (_type,
        sizeof (GstRTPBaseDepayloadPrivate));

    g_once_init_leave ((gsize *) & rtp_base_depayload_type, _type);
  }
  return rtp_base_depayload_type;
}

static inline GstRTPBaseDepayloadPrivate *
gst_rtp_base_depayload_get_instance_private (GstRTPBaseDepayload * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static GstRTPHeaderExtension *
gst_rtp_base_depayload_request_extension_default (GstRTPBaseDepayload *
    depayload, guint ext_id, const gchar * uri)
{
  GstRTPHeaderExtension *ext = NULL;

  if (!depayload->priv->auto_hdr_ext)
    return NULL;

  ext = gst_rtp_header_extension_create_from_uri (uri);
  if (ext) {
    GST_DEBUG_OBJECT (depayload,
        "Automatically enabled extension %s for uri \'%s\'",
        GST_ELEMENT_NAME (ext), uri);

    gst_rtp_header_extension_set_id (ext, ext_id);
  } else {
    GST_DEBUG_OBJECT (depayload,
        "Didn't find any extension implementing uri \'%s\'", uri);
  }

  return ext;
}

static gboolean
extension_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer data)
{
  gpointer ext;

  /* Call default handler if user callback didn't create the extension */
  ext = g_value_get_object (handler_return);
  if (!ext)
    return TRUE;

  g_value_set_object (return_accu, ext);
  return FALSE;
}

static void
gst_rtp_base_depayload_class_init (GstRTPBaseDepayloadClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = (GstElementClass *) klass;
  parent_class = g_type_class_peek_parent (klass);

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  gobject_class->finalize = gst_rtp_base_depayload_finalize;
  gobject_class->set_property = gst_rtp_base_depayload_set_property;
  gobject_class->get_property = gst_rtp_base_depayload_get_property;


  /**
   * GstRTPBaseDepayload:stats:
   *
   * Various depayloader statistics retrieved atomically (and are therefore
   * synchroized with each other). This property return a GstStructure named
   * application/x-rtp-depayload-stats containing the following fields relating to
   * the last processed buffer and current state of the stream being depayloaded:
   *
   *   * `clock-rate`: #G_TYPE_UINT, clock-rate of the stream
   *   * `npt-start`: #G_TYPE_UINT64, time of playback start
   *   * `npt-stop`: #G_TYPE_UINT64, time of playback stop
   *   * `play-speed`: #G_TYPE_DOUBLE, the playback speed
   *   * `play-scale`: #G_TYPE_DOUBLE, the playback scale
   *   * `running-time-dts`: #G_TYPE_UINT64, the last running-time of the
   *      last DTS
   *   * `running-time-pts`: #G_TYPE_UINT64, the last running-time of the
   *      last PTS
   *   * `seqnum`: #G_TYPE_UINT, the last seen seqnum
   *   * `timestamp`: #G_TYPE_UINT, the last seen RTP timestamp
   **/
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_STATS,
      g_param_spec_boxed ("stats", "Statistics", "Various statistics",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBaseDepayload:source-info:
   *
   * Add RTP source information found in RTP header as meta to output buffer.
   *
   * Since: 1.16
   **/
  g_object_class_install_property (gobject_class, PROP_SOURCE_INFO,
      g_param_spec_boolean ("source-info", "RTP source information",
          "Add RTP source information as buffer meta",
          DEFAULT_SOURCE_INFO, G_PARAM_READWRITE));

  /**
   * GstRTPBaseDepayload:max-reorder:
   *
   * Max seqnum reorder before the sender is assumed to have restarted.
   *
   * When max-reorder is set to 0 all reordered/duplicate packets are
   * considered coming from a restarted sender.
   *
   * Since: 1.18
   **/
  g_object_class_install_property (gobject_class, PROP_MAX_REORDER,
      g_param_spec_int ("max-reorder", "Max Reorder",
          "Max seqnum reorder before assuming sender has restarted",
          0, G_MAXINT, DEFAULT_MAX_REORDER, G_PARAM_READWRITE));

  /**
   * GstRTPBaseDepayload:auto-header-extension:
   *
   * If enabled, the depayloader will automatically try to enable all the
   * RTP header extensions provided in the sink caps, saving the application
   * the need to handle these extensions manually using the
   * GstRTPBaseDepayload::request-extension: signal.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_AUTO_HEADER_EXTENSION, g_param_spec_boolean ("auto-header-extension",
          "Automatic RTP header extension",
          "Whether RTP header extensions should be automatically enabled, if an implementation is available",
          DEFAULT_AUTO_HEADER_EXTENSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRTPBaseDepayload::request-extension:
   * @object: the #GstRTPBaseDepayload
   * @ext_id: the extension id being requested
   * @ext_uri: (nullable): the extension URI being requested
   *
   * The returned @ext must be configured with the correct @ext_id and with the
   * necessary attributes as required by the extension implementation.
   *
   * Returns: (transfer full) (nullable): the #GstRTPHeaderExtension for @ext_id, or %NULL
   *
   * Since: 1.20
   */
  gst_rtp_base_depayload_signals[SIGNAL_REQUEST_EXTENSION] =
      g_signal_new_class_handler ("request-extension",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_CALLBACK (gst_rtp_base_depayload_request_extension_default),
      extension_accumulator, NULL, NULL,
      GST_TYPE_RTP_HEADER_EXTENSION, 2, G_TYPE_UINT, G_TYPE_STRING);

  /**
   * GstRTPBaseDepayload::add-extension:
   * @object: the #GstRTPBaseDepayload
   * @ext: (transfer full): the #GstRTPHeaderExtension
   *
   * Add @ext as an extension for reading part of an RTP header extension from
   * incoming RTP packets.
   *
   * Since: 1.20
   */
  gst_rtp_base_depayload_signals[SIGNAL_ADD_EXTENSION] =
      g_signal_new_class_handler ("add-extension", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_rtp_base_depayload_add_extension), NULL, NULL, NULL,
      G_TYPE_NONE, 1, GST_TYPE_RTP_HEADER_EXTENSION);

  /**
   * GstRTPBaseDepayload::clear-extensions:
   * @object: the #GstRTPBaseDepayload
   *
   * Clear all RTP header extensions used by this depayloader.
   *
   * Since: 1.20
   */
  gst_rtp_base_depayload_signals[SIGNAL_CLEAR_EXTENSIONS] =
      g_signal_new_class_handler ("clear-extensions", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_rtp_base_depayload_clear_extensions), NULL, NULL, NULL,
      G_TYPE_NONE, 0);

  gst_rtp_base_depayload_extensions_pspec = gst_param_spec_array ("extensions",
      "RTP header extensions",
      "A list of already enabled RTP header extensions",
      g_param_spec_object ("extension", "RTP header extension",
          "An already enabled RTP extension", GST_TYPE_RTP_HEADER_EXTENSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS),
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstRTPBaseDepayload:extensions:
   *
   * A list of already enabled RTP header extensions. This may be useful for finding
   * out which extensions are already enabled (with add-extension signal) and picking a non-conflicting
   * ID for a new extension that needs to be added on top of the existing ones.
   *
   * Note that the value returned by reading this property is not dynamically updated when the set of
   * enabled extensions changes by any of existing action signals. Rather, it represents the current state
   * at the time the property is read.
   *
   * Dynamic updates of this property can be received by subscribing to its corresponding "notify" signal, i.e.
   * "notify::extensions".
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_EXTENSIONS, gst_rtp_base_depayload_extensions_pspec);

  gstelement_class->change_state = gst_rtp_base_depayload_change_state;

  klass->packet_lost = gst_rtp_base_depayload_packet_lost;
  klass->handle_event = gst_rtp_base_depayload_handle_event;

  GST_DEBUG_CATEGORY_INIT (rtpbasedepayload_debug, "rtpbasedepayload", 0,
      "Base class for RTP Depayloaders");
}

static void
gst_rtp_base_depayload_init (GstRTPBaseDepayload * filter,
    GstRTPBaseDepayloadClass * klass)
{
  GstPadTemplate *pad_template;
  GstRTPBaseDepayloadPrivate *priv;

  priv = gst_rtp_base_depayload_get_instance_private (filter);

  filter->priv = priv;

  GST_DEBUG_OBJECT (filter, "init");

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_return_if_fail (pad_template != NULL);
  filter->sinkpad = gst_pad_new_from_template (pad_template, "sink");
  gst_pad_set_chain_function (filter->sinkpad, gst_rtp_base_depayload_chain);
  gst_pad_set_chain_list_function (filter->sinkpad,
      gst_rtp_base_depayload_chain_list);
  gst_pad_set_event_function (filter->sinkpad,
      gst_rtp_base_depayload_handle_sink_event);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_return_if_fail (pad_template != NULL);
  filter->srcpad = gst_pad_new_from_template (pad_template, "src");
  gst_pad_use_fixed_caps (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  priv->npt_start = 0;
  priv->npt_stop = -1;
  priv->play_speed = 1.0;
  priv->play_scale = 1.0;
  priv->clock_base = -1;
  priv->onvif_mode = FALSE;
  priv->dts = -1;
  priv->pts = -1;
  priv->duration = -1;
  priv->ref_ts = -1;
  priv->source_info = DEFAULT_SOURCE_INFO;
  priv->max_reorder = DEFAULT_MAX_REORDER;
  priv->auto_hdr_ext = DEFAULT_AUTO_HEADER_EXTENSION;
  priv->hdrext_aggregate = FALSE;
  priv->hdrext_seen = FALSE;

  gst_segment_init (&filter->segment, GST_FORMAT_UNDEFINED);

  priv->header_exts =
      g_ptr_array_new_with_free_func ((GDestroyNotify) gst_object_unref);
  priv->hdrext_buffers = gst_buffer_list_new ();
}

static void
gst_rtp_base_depayload_finalize (GObject * object)
{
  GstRTPBaseDepayload *rtpbasedepayload = GST_RTP_BASE_DEPAYLOAD (object);
  GstRTPBaseDepayloadPrivate *priv = rtpbasedepayload->priv;

  g_ptr_array_unref (rtpbasedepayload->priv->header_exts);
  gst_clear_buffer_list (&rtpbasedepayload->priv->hdrext_buffers);
  if (priv->hdrext_delayed)
    gst_buffer_unref (priv->hdrext_delayed);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
add_and_ref_item (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_add (ret, gst_object_ref (ext));
}

static void
remove_item_from (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_remove_fast (ret, ext);
}

static void
add_item_to (GstRTPHeaderExtension * ext, GPtrArray * ret)
{
  g_ptr_array_add (ret, ext);
}

static gboolean
gst_rtp_base_depayload_setcaps (GstRTPBaseDepayload * filter, GstCaps * caps)
{
  GstRTPBaseDepayloadClass *bclass;
  GstRTPBaseDepayloadPrivate *priv;
  gboolean res = TRUE;
  GstStructure *caps_struct;
  const GValue *value;

  priv = filter->priv;

  bclass = GST_RTP_BASE_DEPAYLOAD_GET_CLASS (filter);

  GST_DEBUG_OBJECT (filter, "Set caps %" GST_PTR_FORMAT, caps);

  if (priv->last_caps) {
    if (gst_caps_is_equal (priv->last_caps, caps)) {
      res = TRUE;
      goto caps_not_changed;
    } else {
      gst_caps_unref (priv->last_caps);
      priv->last_caps = NULL;
    }
  }

  caps_struct = gst_caps_get_structure (caps, 0);

  value = gst_structure_get_value (caps_struct, "onvif-mode");
  if (value && G_VALUE_HOLDS_BOOLEAN (value))
    priv->onvif_mode = g_value_get_boolean (value);
  else
    priv->onvif_mode = FALSE;
  GST_DEBUG_OBJECT (filter, "Onvif mode: %d", priv->onvif_mode);

  if (priv->onvif_mode)
    filter->need_newsegment = FALSE;

  /* get other values for newsegment */
  value = gst_structure_get_value (caps_struct, "npt-start");
  if (value && G_VALUE_HOLDS_UINT64 (value))
    priv->npt_start = g_value_get_uint64 (value);
  else
    priv->npt_start = 0;
  GST_DEBUG_OBJECT (filter, "NPT start %" G_GUINT64_FORMAT, priv->npt_start);

  value = gst_structure_get_value (caps_struct, "npt-stop");
  if (value && G_VALUE_HOLDS_UINT64 (value))
    priv->npt_stop = g_value_get_uint64 (value);
  else
    priv->npt_stop = -1;

  GST_DEBUG_OBJECT (filter, "NPT stop %" G_GUINT64_FORMAT, priv->npt_stop);

  value = gst_structure_get_value (caps_struct, "play-speed");
  if (value && G_VALUE_HOLDS_DOUBLE (value))
    priv->play_speed = g_value_get_double (value);
  else
    priv->play_speed = 1.0;

  value = gst_structure_get_value (caps_struct, "play-scale");
  if (value && G_VALUE_HOLDS_DOUBLE (value))
    priv->play_scale = g_value_get_double (value);
  else
    priv->play_scale = 1.0;

  value = gst_structure_get_value (caps_struct, "clock-base");
  if (value && G_VALUE_HOLDS_UINT (value))
    priv->clock_base = g_value_get_uint (value);
  else
    priv->clock_base = -1;

  {
    /* ensure we have header extension implementations for the list in the
     * caps */
    guint i, j, n_fields = gst_structure_n_fields (caps_struct);
    GPtrArray *header_exts = g_ptr_array_new_with_free_func (gst_object_unref);
    GPtrArray *to_add = g_ptr_array_new ();
    GPtrArray *to_remove = g_ptr_array_new ();

    GST_OBJECT_LOCK (filter);
    g_ptr_array_foreach (filter->priv->header_exts,
        (GFunc) add_and_ref_item, header_exts);
    GST_OBJECT_UNLOCK (filter);

    for (i = 0; i < n_fields; i++) {
      const gchar *field_name = gst_structure_nth_field_name (caps_struct, i);
      if (g_str_has_prefix (field_name, "extmap-")) {
        const GValue *val;
        const gchar *uri = NULL;
        gchar *nptr;
        guint ext_id;
        GstRTPHeaderExtension *ext = NULL;

        errno = 0;
        ext_id = g_ascii_strtoull (&field_name[strlen ("extmap-")], &nptr, 10);
        if (errno != 0 || (ext_id == 0 && field_name == nptr)) {
          GST_WARNING_OBJECT (filter, "could not parse id from %s", field_name);
          res = FALSE;
          goto ext_out;
        }

        val = gst_structure_get_value (caps_struct, field_name);
        if (G_VALUE_HOLDS_STRING (val)) {
          uri = g_value_get_string (val);
        } else if (GST_VALUE_HOLDS_ARRAY (val)) {
          /* the uri is the second value in the array */
          const GValue *str = gst_value_array_get_value (val, 1);
          if (G_VALUE_HOLDS_STRING (str)) {
            uri = g_value_get_string (str);
          }
        }

        if (!uri) {
          GST_WARNING_OBJECT (filter, "could not get extmap uri for "
              "field %s", field_name);
          res = FALSE;
          goto ext_out;
        }

        /* try to find if this extension mapping already exists */
        for (j = 0; j < header_exts->len; j++) {
          ext = g_ptr_array_index (header_exts, j);
          if (gst_rtp_header_extension_get_id (ext) == ext_id) {
            if (g_strcmp0 (uri, gst_rtp_header_extension_get_uri (ext)) == 0) {
              /* still matching, we're good, set attributes from caps in case
               * the caps have changed */
              if (!gst_rtp_header_extension_set_attributes_from_caps (ext,
                      caps)) {
                GST_WARNING_OBJECT (filter,
                    "Failed to configure rtp header " "extension %"
                    GST_PTR_FORMAT " attributes from caps %" GST_PTR_FORMAT,
                    ext, caps);
                res = FALSE;
                goto ext_out;
              }
              break;
            } else {
              GST_DEBUG_OBJECT (filter, "extension id %u"
                  "was replaced with a different extension uri "
                  "original:\'%s' vs \'%s\'", ext_id,
                  gst_rtp_header_extension_get_uri (ext), uri);
              g_ptr_array_add (to_remove, ext);
              ext = NULL;
              break;
            }
          } else {
            ext = NULL;
          }
        }

        /* if no extension, attempt to request one */
        if (!ext) {
          GST_DEBUG_OBJECT (filter, "requesting extension for id %u"
              " and uri %s", ext_id, uri);
          g_signal_emit (filter,
              gst_rtp_base_depayload_signals[SIGNAL_REQUEST_EXTENSION], 0,
              ext_id, uri, &ext);
          GST_DEBUG_OBJECT (filter, "request returned extension %p \'%s\' "
              "for id %u and uri %s", ext,
              ext ? GST_OBJECT_NAME (ext) : "", ext_id, uri);

          /* We require the caller to set the appropriate extension if it's required */
          if (ext && gst_rtp_header_extension_get_id (ext) != ext_id) {
            g_warning ("\'request-extension\' signal provided an rtp header "
                "extension for uri \'%s\' that does not match the requested "
                "extension id %u", uri, ext_id);
            gst_clear_object (&ext);
          }

          if (ext && !gst_rtp_header_extension_set_attributes_from_caps (ext,
                  caps)) {
            GST_WARNING_OBJECT (filter,
                "Failed to configure rtp header " "extension %"
                GST_PTR_FORMAT " attributes from caps %" GST_PTR_FORMAT,
                ext, caps);
            res = FALSE;
            g_clear_object (&ext);
            goto ext_out;
          }

          if (ext)
            g_ptr_array_add (to_add, ext);
        }
      }
    }

    /* Note: we intentionally don't remove extensions that are not listed
     * in caps */

    GST_OBJECT_LOCK (filter);
    g_ptr_array_foreach (to_remove, (GFunc) remove_item_from,
        filter->priv->header_exts);
    g_ptr_array_foreach (to_add, (GFunc) add_item_to,
        filter->priv->header_exts);
    GST_OBJECT_UNLOCK (filter);

    g_object_notify_by_pspec (G_OBJECT (filter),
        gst_rtp_base_depayload_extensions_pspec);

  ext_out:
    g_ptr_array_unref (to_add);
    g_ptr_array_unref (to_remove);
    g_ptr_array_unref (header_exts);

    if (!res)
      return res;
  }

  if (bclass->set_caps) {
    res = bclass->set_caps (filter, caps);
    if (!res) {
      GST_WARNING_OBJECT (filter, "Subclass rejected caps %" GST_PTR_FORMAT,
          caps);
    }
  } else {
    res = TRUE;
  }

  priv->negotiated = res;

  if (priv->negotiated)
    priv->last_caps = gst_caps_ref (caps);

  return res;

caps_not_changed:
  {
    GST_DEBUG_OBJECT (filter, "Caps did not change");
    return res;
  }
}

/* takes ownership of the input buffer */
static GstFlowReturn
gst_rtp_base_depayload_handle_buffer (GstRTPBaseDepayload * filter,
    GstRTPBaseDepayloadClass * bclass, GstBuffer * in)
{
  GstBuffer *(*process_rtp_packet_func) (GstRTPBaseDepayload * base,
      GstRTPBuffer * rtp_buffer);
  GstBuffer *(*process_func) (GstRTPBaseDepayload * base, GstBuffer * in);
  GstRTPBaseDepayloadPrivate *priv;
  GstBuffer *out_buf;
  guint32 ssrc;
  guint16 seqnum;
  guint32 rtptime;
  gboolean discont, buf_discont;
  gint gap;
  GstRTPBuffer rtp = { NULL };
  GstReferenceTimestampMeta *meta;
  GstCaps *ref_caps;

  priv = filter->priv;
  priv->process_flow_ret = GST_FLOW_OK;

  process_func = bclass->process;
  process_rtp_packet_func = bclass->process_rtp_packet;

  /* we must have a setcaps first */
  if (G_UNLIKELY (!priv->negotiated))
    goto not_negotiated;

  /* Check for duplicate reference timestamp metadata */
  ref_caps = gst_static_caps_get (&ntp_reference_timestamp_caps);
  meta = gst_buffer_get_reference_timestamp_meta (in, ref_caps);
  gst_caps_unref (ref_caps);
  if (meta) {
    guint64 ref_ts = meta->timestamp;
    if (ref_ts == priv->ref_ts) {
      /* Drop the redundant/duplicate reference timstamp metadata */
      in = gst_buffer_make_writable (in);
      gst_buffer_remove_meta (in, GST_META_CAST (meta));
    } else {
      priv->ref_ts = ref_ts;
    }
  }

  if (G_UNLIKELY (!gst_rtp_buffer_map (in, GST_MAP_READ, &rtp)))
    goto invalid_buffer;

  buf_discont = GST_BUFFER_IS_DISCONT (in);

  priv->pts = GST_BUFFER_PTS (in);
  priv->dts = GST_BUFFER_DTS (in);
  priv->duration = GST_BUFFER_DURATION (in);

  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  seqnum = gst_rtp_buffer_get_seq (&rtp);
  rtptime = gst_rtp_buffer_get_timestamp (&rtp);

  priv->last_seqnum = seqnum;
  priv->last_rtptime = rtptime;

  discont = buf_discont;

  GST_LOG_OBJECT (filter, "discont %d, seqnum %u, rtptime %u, pts %"
      GST_TIME_FORMAT ", dts %" GST_TIME_FORMAT, buf_discont, seqnum, rtptime,
      GST_TIME_ARGS (priv->pts), GST_TIME_ARGS (priv->dts));

  /* Check seqnum. This is a very simple check that makes sure that the seqnums
   * are strictly increasing, dropping anything that is out of the ordinary. We
   * can only do this when the next_seqnum is known. */
  if (G_LIKELY (priv->next_seqnum != -1)) {
    if (ssrc != priv->last_ssrc) {
      GST_LOG_OBJECT (filter,
          "New ssrc %u (current ssrc %u), sender restarted",
          ssrc, priv->last_ssrc);
      discont = TRUE;
    } else {
      gap = gst_rtp_buffer_compare_seqnum (seqnum, priv->next_seqnum);

      /* if we have no gap, all is fine */
      if (G_UNLIKELY (gap != 0)) {
        GST_LOG_OBJECT (filter, "got packet %u, expected %u, gap %d", seqnum,
            priv->next_seqnum, gap);
        if (gap < 0) {
          /* seqnum > next_seqnum, we are missing some packets, this is always a
           * DISCONT. */
          GST_LOG_OBJECT (filter, "%d missing packets", gap);
          discont = TRUE;
        } else {
          /* seqnum < next_seqnum, we have seen this packet before, have a
           * reordered packet or the sender could be restarted. If the packet
           * is not too old, we throw it away as a duplicate. Otherwise we
           * mark discont and continue assuming the sender has restarted. See
           * also RFC 4737. */
          if (gap <= priv->max_reorder) {
            GST_WARNING_OBJECT (filter, "got old packet %u, expected %u, "
                "gap %d <= max_reorder (%d), dropping!",
                seqnum, priv->next_seqnum, gap, priv->max_reorder);
            goto dropping;
          }
          GST_WARNING_OBJECT (filter, "got old packet %u, expected %u, "
              "marking discont", seqnum, priv->next_seqnum);
          discont = TRUE;
        }
      }
    }
  }
  priv->next_seqnum = (seqnum + 1) & 0xffff;
  priv->last_ssrc = ssrc;

  if (G_UNLIKELY (discont)) {
    priv->discont = TRUE;
    if (!buf_discont) {
      gpointer old_inbuf = in;

      /* we detected a seqnum discont but the buffer was not flagged with a discont,
       * set the discont flag so that the subclass can throw away old data. */
      GST_LOG_OBJECT (filter, "mark DISCONT on input buffer");
      in = gst_buffer_make_writable (in);
      GST_BUFFER_FLAG_SET (in, GST_BUFFER_FLAG_DISCONT);
      /* depayloaders will check flag on rtpbuffer->buffer, so if the input
       * buffer was not writable already we need to remap to make our
       * newly-flagged buffer current on the rtpbuffer */
      if (in != old_inbuf) {
        gst_rtp_buffer_unmap (&rtp);
        if (G_UNLIKELY (!gst_rtp_buffer_map (in, GST_MAP_READ, &rtp)))
          goto invalid_buffer;
      }
    }
  }

  /* prepare segment event if needed */
  if (filter->need_newsegment) {
    priv->segment_event = create_segment_event (filter, rtptime,
        GST_BUFFER_PTS (in));
    filter->need_newsegment = FALSE;
  }

  priv->input_buffer = in;

  if (discont) {
    gst_rtp_base_depayload_reset_hdrext_buffers (filter);
    g_assert_null (priv->hdrext_delayed);
  }

  /* update RTP buffer cache for header extensions if any */
  if (priv->hdrext_aggregate &&
      !priv->hdrext_seen && gst_rtp_buffer_get_extension (&rtp)) {
    GST_INFO_OBJECT (filter, "Activate RTP header ext aggregation");
    priv->hdrext_seen = priv->hdrext_aggregate;
  }

  if (priv->hdrext_seen) {
    GstBuffer *b = gst_buffer_new ();
    /* make a copy of the buffer that only contains the RTP header
       with the extensions to not waste too much memory */
    guint s = gst_rtp_buffer_get_header_len (&rtp);
    gst_buffer_copy_into (b, in,
        GST_BUFFER_COPY_MEMORY | GST_BUFFER_COPY_DEEP, 0, s);
    gst_buffer_list_add (priv->hdrext_buffers, b);
  }

  if (process_rtp_packet_func != NULL) {
    out_buf = process_rtp_packet_func (filter, &rtp);
    gst_rtp_buffer_unmap (&rtp);
  } else if (process_func != NULL) {
    gst_rtp_buffer_unmap (&rtp);
    out_buf = process_func (filter, in);
  } else {
    goto no_process;
  }

  /* let's send it out to processing */
  if (out_buf) {
    if (priv->process_flow_ret == GST_FLOW_OK) {
      priv->process_flow_ret = gst_rtp_base_depayload_push (filter, out_buf);
    } else {
      gst_buffer_unref (out_buf);
      gst_rtp_base_depayload_reset_hdrext_buffers (filter);
    }
  }

  /* if the current buffer is delayed the depayloader should either
     have called gst_rtp_base_depayload_push() internally or returned
     a buffer that's pushed, either way the buffer cache should be
     empty here and we append the delayed buffer */
  if (priv->hdrext_delayed) {
    g_assert_true (gst_buffer_list_length (priv->hdrext_buffers) == 0);
    gst_buffer_list_add (priv->hdrext_buffers, priv->hdrext_delayed);
    priv->hdrext_delayed = NULL;
  }

  gst_buffer_unref (in);
  priv->input_buffer = NULL;

  return priv->process_flow_ret;

  /* ERRORS */
not_negotiated:
  {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_ERROR (filter, CORE, NEGOTIATION,
        ("No RTP format was negotiated."),
        ("Input buffers need to have RTP caps set on them. This is usually "
            "achieved by setting the 'caps' property of the upstream source "
            "element (often udpsrc or appsrc), or by putting a capsfilter "
            "element before the depayloader and setting the 'caps' property "
            "on that. Also see http://cgit.freedesktop.org/gstreamer/"
            "gst-plugins-good/tree/gst/rtp/README"));
    gst_buffer_unref (in);
    return GST_FLOW_NOT_NEGOTIATED;
  }
invalid_buffer:
  {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_WARNING (filter, STREAM, DECODE, (NULL),
        ("Received invalid RTP payload, dropping"));
    gst_buffer_unref (in);
    return GST_FLOW_OK;
  }
dropping:
  {
    gst_rtp_buffer_unmap (&rtp);
    gst_buffer_unref (in);
    return GST_FLOW_OK;
  }
no_process:
  {
    gst_rtp_buffer_unmap (&rtp);
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_ERROR (filter, STREAM, NOT_IMPLEMENTED, (NULL),
        ("The subclass does not have a process or process_rtp_packet method"));
    gst_buffer_unref (in);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_rtp_base_depayload_chain (GstPad * pad, GstObject * parent, GstBuffer * in)
{
  GstRTPBaseDepayloadClass *bclass;
  GstRTPBaseDepayload *basedepay;
  GstFlowReturn flow_ret;

  basedepay = GST_RTP_BASE_DEPAYLOAD_CAST (parent);

  bclass = GST_RTP_BASE_DEPAYLOAD_GET_CLASS (basedepay);

  flow_ret = gst_rtp_base_depayload_handle_buffer (basedepay, bclass, in);

  return flow_ret;
}

static GstFlowReturn
gst_rtp_base_depayload_chain_list (GstPad * pad, GstObject * parent,
    GstBufferList * list)
{
  GstRTPBaseDepayloadClass *bclass;
  GstRTPBaseDepayload *basedepay;
  GstFlowReturn flow_ret;
  GstBuffer *buffer;
  guint i, len;

  basedepay = GST_RTP_BASE_DEPAYLOAD_CAST (parent);

  bclass = GST_RTP_BASE_DEPAYLOAD_GET_CLASS (basedepay);

  flow_ret = GST_FLOW_OK;

  /* chain each buffer in list individually */
  len = gst_buffer_list_length (list);

  if (len == 0)
    goto done;

  for (i = 0; i < len; i++) {
    buffer = gst_buffer_list_get (list, i);

    /* handle_buffer takes ownership of input buffer */
    /* FIXME: add a way to steal buffers from list as we will unref it anyway */
    gst_buffer_ref (buffer);

    /* Should we fix up any missing timestamps for list buffers here
     * (e.g. set to first or previous timestamp in list) or just assume
     * the's a jitterbuffer that will have done that for us? */
    flow_ret = gst_rtp_base_depayload_handle_buffer (basedepay, bclass, buffer);
    if (flow_ret != GST_FLOW_OK)
      break;
  }

done:

  gst_buffer_list_unref (list);

  return flow_ret;
}

static gboolean
gst_rtp_base_depayload_handle_event (GstRTPBaseDepayload * filter,
    GstEvent * event)
{
  gboolean res = TRUE;
  gboolean forward = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      GST_OBJECT_LOCK (filter);
      gst_segment_init (&filter->segment, GST_FORMAT_UNDEFINED);
      GST_OBJECT_UNLOCK (filter);

      filter->need_newsegment = !filter->priv->onvif_mode;
      filter->priv->next_seqnum = -1;
      filter->priv->ref_ts = -1;
      gst_event_replace (&filter->priv->segment_event, NULL);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      res = gst_rtp_base_depayload_setcaps (filter, caps);
      forward = FALSE;
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      GST_OBJECT_LOCK (filter);
      gst_event_copy_segment (event, &segment);

      if (segment.format != GST_FORMAT_TIME) {
        GST_ERROR_OBJECT (filter, "Segment with non-TIME format not supported");
        res = FALSE;
      }
      filter->priv->segment_seqnum = gst_event_get_seqnum (event);
      filter->segment = segment;
      GST_OBJECT_UNLOCK (filter);

      /* In ONVIF mode, upstream is expected to send us the correct segment */
      if (!filter->priv->onvif_mode) {
        /* don't pass the event downstream, we generate our own segment including
         * the NTP time and other things we receive in caps */
        forward = FALSE;
      }
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      GstRTPBaseDepayloadClass *bclass;

      bclass = GST_RTP_BASE_DEPAYLOAD_GET_CLASS (filter);

      if (gst_event_has_name (event, "GstRTPPacketLost")) {
        /* we get this event from the jitterbuffer when it considers a packet as
         * being lost. We send it to our packet_lost vmethod. The default
         * implementation will make time progress by pushing out a GAP event.
         * Subclasses can override and do one of the following:
         *  - Adjust timestamp/duration to something more accurate before
         *    calling the parent (default) packet_lost method.
         *  - do some more advanced error concealing on the already received
         *    (fragmented) packets.
         *  - ignore the packet lost.
         */
        if (bclass->packet_lost)
          res = bclass->packet_lost (filter, event);
        forward = FALSE;
      }
      break;
    }
    default:
      break;
  }

  if (forward)
    res = gst_pad_push_event (filter->srcpad, event);
  else
    gst_event_unref (event);

  return res;
}

static gboolean
gst_rtp_base_depayload_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean res = FALSE;
  GstRTPBaseDepayload *filter;
  GstRTPBaseDepayloadClass *bclass;

  filter = GST_RTP_BASE_DEPAYLOAD (parent);
  bclass = GST_RTP_BASE_DEPAYLOAD_GET_CLASS (filter);
  if (bclass->handle_event)
    res = bclass->handle_event (filter, event);
  else
    gst_event_unref (event);

  return res;
}

static GstEvent *
create_segment_event (GstRTPBaseDepayload * filter, guint rtptime,
    GstClockTime position)
{
  GstEvent *event;
  GstClockTime start, stop, running_time;
  GstRTPBaseDepayloadPrivate *priv;
  GstSegment segment;

  priv = filter->priv;

  /* We don't need the object lock around - the segment
   * can't change here while we're holding the STREAM_LOCK
   */

  /* determining the start of the segment */
  start = filter->segment.start;
  if (priv->clock_base != -1 && position != -1) {
    GstClockTime exttime, gap;

    exttime = priv->clock_base;
    gst_rtp_buffer_ext_timestamp (&exttime, rtptime);
    gap = gst_util_uint64_scale_int (exttime - priv->clock_base,
        filter->clock_rate, GST_SECOND);

    /* account for lost packets */
    if (position > gap) {
      GST_DEBUG_OBJECT (filter,
          "Found gap of %" GST_TIME_FORMAT ", adjusting start: %"
          GST_TIME_FORMAT " = %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (gap), GST_TIME_ARGS (position - gap),
          GST_TIME_ARGS (position), GST_TIME_ARGS (gap));
      start = position - gap;
    }
  }

  /* determining the stop of the segment */
  stop = filter->segment.stop;
  if (priv->npt_stop != -1)
    stop = start + (priv->npt_stop - priv->npt_start);

  if (position == -1)
    position = start;

  running_time = gst_segment_to_running_time (&filter->segment,
      GST_FORMAT_TIME, start);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  segment.rate = priv->play_speed;
  segment.applied_rate = priv->play_scale;
  segment.start = start;
  segment.stop = stop;
  segment.time = priv->npt_start;
  segment.position = position;
  segment.base = running_time;

  GST_DEBUG_OBJECT (filter, "Creating segment event %" GST_SEGMENT_FORMAT,
      &segment);
  event = gst_event_new_segment (&segment);
  if (filter->priv->segment_seqnum != GST_SEQNUM_INVALID)
    gst_event_set_seqnum (event, filter->priv->segment_seqnum);

  return event;
}

static gboolean
foreach_metadata_drop (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  GType drop_api_type = (GType) user_data;
  const GstMetaInfo *info = (*meta)->info;

  if (info->api == drop_api_type)
    *meta = NULL;

  return TRUE;
}

static void
add_rtp_source_meta (GstBuffer * outbuf, GstBuffer * rtpbuf)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  GstRTPSourceMeta *meta;
  guint32 ssrc;
  GType source_meta_api = gst_rtp_source_meta_api_get_type ();

  if (!gst_rtp_buffer_map (rtpbuf, GST_MAP_READ, &rtp))
    return;

  ssrc = gst_rtp_buffer_get_ssrc (&rtp);

  /* remove any pre-existing source-meta */
  gst_buffer_foreach_meta (outbuf, foreach_metadata_drop,
      (gpointer) source_meta_api);

  meta = gst_buffer_add_rtp_source_meta (outbuf, &ssrc, NULL, 0);
  if (meta != NULL) {
    gint i;
    gint csrc_count = gst_rtp_buffer_get_csrc_count (&rtp);
    for (i = 0; i < csrc_count; i++) {
      guint32 csrc = gst_rtp_buffer_get_csrc (&rtp, i);
      gst_rtp_source_meta_append_csrc (meta, &csrc, 1);
    }
  }

  gst_rtp_buffer_unmap (&rtp);
}

static void
gst_rtp_base_depayload_add_extension (GstRTPBaseDepayload * rtpbasepayload,
    GstRTPHeaderExtension * ext)
{
  g_return_if_fail (GST_IS_RTP_HEADER_EXTENSION (ext));
  g_return_if_fail (gst_rtp_header_extension_get_id (ext) > 0);

  /* XXX: check for duplicate ids? */
  GST_OBJECT_LOCK (rtpbasepayload);
  g_ptr_array_add (rtpbasepayload->priv->header_exts, gst_object_ref (ext));
  GST_OBJECT_UNLOCK (rtpbasepayload);

  g_object_notify_by_pspec (G_OBJECT (rtpbasepayload),
      gst_rtp_base_depayload_extensions_pspec);
}

static void
gst_rtp_base_depayload_clear_extensions (GstRTPBaseDepayload * rtpbasepayload)
{
  GST_OBJECT_LOCK (rtpbasepayload);
  g_ptr_array_set_size (rtpbasepayload->priv->header_exts, 0);
  GST_OBJECT_UNLOCK (rtpbasepayload);

  g_object_notify_by_pspec (G_OBJECT (rtpbasepayload),
      gst_rtp_base_depayload_extensions_pspec);
}

static void
gst_rtp_base_depayload_get_extensions (GstRTPBaseDepayload * depayload,
    GValue * out_value)
{
  GPtrArray *extensions;
  guint i;

  GST_OBJECT_LOCK (depayload);
  extensions = depayload->priv->header_exts;

  for (i = 0; i < extensions->len; ++i) {
    GValue value = G_VALUE_INIT;
    g_value_init (&value, GST_TYPE_RTP_HEADER_EXTENSION);

    g_value_set_object (&value, g_ptr_array_index (extensions, i));

    gst_value_array_append_and_take_value (out_value, &value);
  }

  GST_OBJECT_UNLOCK (depayload);
}

static gboolean
read_rtp_header_extensions (GstRTPBaseDepayload * depayload,
    GstBuffer * input, GstBuffer * output)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint16 bit_pattern;
  guint8 *pdata;
  guint wordlen;
  gboolean needs_src_caps_update = FALSE;

  if (!input) {
    GST_DEBUG_OBJECT (depayload, "no input buffer");
    return needs_src_caps_update;
  }

  if (!gst_rtp_buffer_map (input, GST_MAP_READ, &rtp)) {
    GST_WARNING_OBJECT (depayload, "Failed to map buffer");
    return needs_src_caps_update;
  }

  if (gst_rtp_buffer_get_extension_data (&rtp, &bit_pattern, (gpointer) & pdata,
          &wordlen)) {
    GstRTPHeaderExtensionFlags ext_flags = 0;
    gsize bytelen = wordlen * 4;
    guint hdr_unit_bytes;
    gsize offset = 0;

    if (bit_pattern == 0xBEDE) {
      /* one byte extensions */
      hdr_unit_bytes = 1;
      ext_flags |= GST_RTP_HEADER_EXTENSION_ONE_BYTE;
    } else if (bit_pattern >> 4 == 0x100) {
      /* two byte extensions */
      hdr_unit_bytes = 2;
      ext_flags |= GST_RTP_HEADER_EXTENSION_TWO_BYTE;
    } else {
      GST_DEBUG_OBJECT (depayload, "unknown extension bit pattern 0x%02x%02x",
          bit_pattern >> 8, bit_pattern & 0xff);
      goto out;
    }

    while (TRUE) {
      guint8 read_id, read_len;
      GstRTPHeaderExtension *ext = NULL;
      guint i;

      if (offset + hdr_unit_bytes >= bytelen)
        /* not enough remaning data */
        break;

      if (ext_flags & GST_RTP_HEADER_EXTENSION_ONE_BYTE) {
        read_id = GST_READ_UINT8 (pdata + offset) >> 4;
        read_len = (GST_READ_UINT8 (pdata + offset) & 0x0F) + 1;
        offset += 1;

        if (read_id == 0)
          /* padding */
          continue;

        if (read_id == 15)
          /* special id for possible future expansion */
          break;
      } else {
        read_id = GST_READ_UINT8 (pdata + offset);
        offset += 1;

        if (read_id == 0)
          /* padding */
          continue;

        read_len = GST_READ_UINT8 (pdata + offset);
        offset += 1;
      }
      GST_TRACE_OBJECT (depayload, "found rtp header extension with id %u and "
          "length %u", read_id, read_len);

      /* Ignore extension headers where the size does not fit */
      if (offset + read_len > bytelen) {
        GST_WARNING_OBJECT (depayload, "Extension length extends past the "
            "size of the extension data");
        break;
      }

      GST_OBJECT_LOCK (depayload);
      for (i = 0; i < depayload->priv->header_exts->len; i++) {
        ext = g_ptr_array_index (depayload->priv->header_exts, i);
        if (read_id == gst_rtp_header_extension_get_id (ext)) {
          gst_object_ref (ext);
          break;
        }
        ext = NULL;
      }

      if (ext) {
        if (!gst_rtp_header_extension_read (ext, ext_flags, &pdata[offset],
                read_len, output)) {
          GST_WARNING_OBJECT (depayload, "RTP header extension (%s) could "
              "not read payloaded data", GST_OBJECT_NAME (ext));
          gst_object_unref (ext);
          goto out;
        }

        if (gst_rtp_header_extension_wants_update_non_rtp_src_caps (ext)) {
          needs_src_caps_update = TRUE;
        }

        gst_object_unref (ext);
      }
      GST_OBJECT_UNLOCK (depayload);

      offset += read_len;
    }
  }

out:
  gst_rtp_buffer_unmap (&rtp);

  return needs_src_caps_update;
}

static gboolean
gst_rtp_base_depayload_operate_hdrext_buffer (GstBuffer ** buffer,
    guint idx, gpointer depayloader)
{
  GstRTPBaseDepayload *depayload = depayloader;

  depayload->priv->hdrext_read_result |=
      read_rtp_header_extensions (depayload, *buffer,
      depayload->priv->hdrext_outbuf);
  return TRUE;
}

static void
gst_rtp_base_depayload_reset_hdrext_buffers (GstRTPBaseDepayload * depayload)
{
  GstRTPBaseDepayloadPrivate *priv = depayload->priv;

  gst_buffer_list_unref (priv->hdrext_buffers);
  priv->hdrext_buffers = gst_buffer_list_new ();
}

static gboolean
gst_rtp_base_depayload_set_headers (GstRTPBaseDepayload * depayload,
    GstBuffer * buffer)
{
  GstRTPBaseDepayloadPrivate *priv = depayload->priv;
  GstClockTime pts, dts, duration;
  gboolean ret = FALSE;

  pts = GST_BUFFER_PTS (buffer);
  dts = GST_BUFFER_DTS (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  /* apply last incoming timestamp and duration to outgoing buffer if
   * not otherwise set. */
  if (!GST_CLOCK_TIME_IS_VALID (pts))
    GST_BUFFER_PTS (buffer) = priv->pts;
  if (!GST_CLOCK_TIME_IS_VALID (dts))
    GST_BUFFER_DTS (buffer) = priv->dts;
  if (!GST_CLOCK_TIME_IS_VALID (duration))
    GST_BUFFER_DURATION (buffer) = priv->duration;

  if (G_UNLIKELY (depayload->priv->discont)) {
    GST_LOG_OBJECT (depayload, "Marking DISCONT on output buffer");
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    depayload->priv->discont = FALSE;
  }

  /* make sure we only set the timestamp on the first packet */
  priv->pts = GST_CLOCK_TIME_NONE;
  priv->dts = GST_CLOCK_TIME_NONE;
  priv->duration = GST_CLOCK_TIME_NONE;

  if (priv->input_buffer) {
    if (priv->source_info)
      add_rtp_source_meta (buffer, priv->input_buffer);

    if (priv->hdrext_aggregate) {
      priv->hdrext_read_result = FALSE;
      priv->hdrext_outbuf = buffer;
      /* if we have an empty list but a delayed RTP buffer let's use it */
      if (!gst_buffer_list_length (priv->hdrext_buffers) &&
          priv->hdrext_delayed) {
        gst_buffer_list_add (priv->hdrext_buffers, priv->hdrext_delayed);
        priv->hdrext_delayed = NULL;
      }
      gst_buffer_list_foreach (priv->hdrext_buffers,
          gst_rtp_base_depayload_operate_hdrext_buffer, depayload);
      ret = priv->hdrext_read_result;
      priv->hdrext_outbuf = NULL;
    } else {
      ret = read_rtp_header_extensions (depayload, priv->input_buffer, buffer);
    }
  }

  return ret;
}

static GstFlowReturn
gst_rtp_base_depayload_finish_push (GstRTPBaseDepayload * filter,
    gboolean is_list, gpointer obj)
{
  /* if this is the first buffer send a NEWSEGMENT */
  if (G_UNLIKELY (filter->priv->segment_event)) {
    gst_pad_push_event (filter->srcpad, filter->priv->segment_event);
    filter->priv->segment_event = NULL;
    GST_DEBUG_OBJECT (filter, "Pushed newsegment event on this first buffer");
  }

  if (is_list) {
    GstBufferList *blist = obj;
    return gst_pad_push_list (filter->srcpad, blist);
  } else {
    GstBuffer *buf = obj;
    return gst_pad_push (filter->srcpad, buf);
  }
}

static gboolean
gst_rtp_base_depayload_set_src_caps_from_hdrext (GstRTPBaseDepayload * filter)
{
  gboolean update_ok = TRUE;
  GstCaps *src_caps = gst_pad_get_current_caps (filter->srcpad);

  if (src_caps) {
    GstCaps *new_caps;
    gint i;

    new_caps = gst_caps_copy (src_caps);
    for (i = 0; i < filter->priv->header_exts->len; i++) {
      GstRTPHeaderExtension *ext;

      ext = g_ptr_array_index (filter->priv->header_exts, i);
      update_ok =
          gst_rtp_header_extension_update_non_rtp_src_caps (ext, new_caps);

      if (!update_ok) {
        GST_ELEMENT_ERROR (filter, STREAM, DECODE,
            ("RTP header extension (%s) could not update src caps",
                GST_OBJECT_NAME (ext)), (NULL));
        break;
      }
    }

    if (G_UNLIKELY (update_ok && !gst_caps_is_equal (src_caps, new_caps))) {
      gst_pad_set_caps (filter->srcpad, new_caps);
    }

    gst_caps_unref (src_caps);
    gst_caps_unref (new_caps);
  }

  return update_ok;
}

static GstFlowReturn
gst_rtp_base_depayload_do_push (GstRTPBaseDepayload * filter, gboolean is_list,
    gpointer obj)
{
  GstFlowReturn res;

  if (is_list) {
    GstBufferList *blist = obj;
    guint i;
    guint first_not_pushed_idx = 0;

    for (i = 0; i < gst_buffer_list_length (blist); ++i) {
      GstBuffer *buf = gst_buffer_list_get_writable (blist, i);

      if (G_UNLIKELY (gst_rtp_base_depayload_set_headers (filter, buf))) {
        /* src caps have changed; push the buffers preceding the current one,
         * then apply the new caps on the src pad */
        guint j;

        for (j = first_not_pushed_idx; j < i; ++j) {
          res = gst_rtp_base_depayload_finish_push (filter, FALSE,
              gst_buffer_ref (gst_buffer_list_get (blist, j)));
          if (G_UNLIKELY (res != GST_FLOW_OK)) {
            goto error_list;
          }
        }
        first_not_pushed_idx = i;

        if (!gst_rtp_base_depayload_set_src_caps_from_hdrext (filter)) {
          res = GST_FLOW_ERROR;
          goto error_list;
        }
      }
    }

    if (G_LIKELY (first_not_pushed_idx == 0)) {
      res = gst_rtp_base_depayload_finish_push (filter, TRUE, blist);
      blist = NULL;
    } else {
      for (i = first_not_pushed_idx; i < gst_buffer_list_length (blist); ++i) {
        res = gst_rtp_base_depayload_finish_push (filter, FALSE,
            gst_buffer_ref (gst_buffer_list_get (blist, i)));
        if (G_UNLIKELY (res != GST_FLOW_OK)) {
          break;
        }
      }
    }

  error_list:
    gst_clear_buffer_list (&blist);
  } else {
    GstBuffer *buf = obj;
    if (G_UNLIKELY (gst_rtp_base_depayload_set_headers (filter, buf))) {
      if (!gst_rtp_base_depayload_set_src_caps_from_hdrext (filter)) {
        res = GST_FLOW_ERROR;
        goto error_buffer;
      }
    }

    res = gst_rtp_base_depayload_finish_push (filter, FALSE, buf);
    buf = NULL;

  error_buffer:
    gst_clear_buffer (&buf);
  }

  gst_rtp_base_depayload_reset_hdrext_buffers (filter);

  return res;
}

/**
 * gst_rtp_base_depayload_push:
 * @filter: a #GstRTPBaseDepayload
 * @out_buf: (transfer full): a #GstBuffer
 *
 * Push @out_buf to the peer of @filter. This function takes ownership of
 * @out_buf.
 *
 * This function will by default apply the last incoming timestamp on
 * the outgoing buffer when it didn't have a timestamp already.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
gst_rtp_base_depayload_push (GstRTPBaseDepayload * filter, GstBuffer * out_buf)
{
  GstFlowReturn res;

  res = gst_rtp_base_depayload_do_push (filter, FALSE, out_buf);

  if (res != GST_FLOW_OK)
    filter->priv->process_flow_ret = res;

  return res;
}

/**
 * gst_rtp_base_depayload_push_list:
 * @filter: a #GstRTPBaseDepayload
 * @out_list: (transfer full): a #GstBufferList
 *
 * Push @out_list to the peer of @filter. This function takes ownership of
 * @out_list.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
gst_rtp_base_depayload_push_list (GstRTPBaseDepayload * filter,
    GstBufferList * out_list)
{
  GstFlowReturn res;

  res = gst_rtp_base_depayload_do_push (filter, TRUE, out_list);

  if (res != GST_FLOW_OK)
    filter->priv->process_flow_ret = res;

  return res;
}

/* convert the PacketLost event from a jitterbuffer to a GAP event.
 * subclasses can override this.  */
static gboolean
gst_rtp_base_depayload_packet_lost (GstRTPBaseDepayload * filter,
    GstEvent * event)
{
  GstClockTime timestamp, duration;
  GstEvent *sevent;
  const GstStructure *s;
  gboolean might_have_been_fec;
  gboolean res = TRUE;

  s = gst_event_get_structure (event);

  /* first start by parsing the timestamp and duration */
  timestamp = -1;
  duration = -1;

  if (!gst_structure_get_clock_time (s, "timestamp", &timestamp) ||
      !gst_structure_get_clock_time (s, "duration", &duration)) {
    GST_ERROR_OBJECT (filter,
        "Packet loss event without timestamp or duration");
    return FALSE;
  }

  sevent = gst_pad_get_sticky_event (filter->srcpad, GST_EVENT_SEGMENT, 0);
  if (G_UNLIKELY (!sevent)) {
    /* Typically happens if lost event arrives before first buffer */
    GST_DEBUG_OBJECT (filter,
        "Ignore packet loss because segment event missing");
    return FALSE;
  }
  gst_event_unref (sevent);

  if (!gst_structure_get_boolean (s, "might-have-been-fec",
          &might_have_been_fec) || !might_have_been_fec) {
    /* send GAP event */
    sevent = gst_event_new_gap (timestamp, duration);
    gst_event_set_gap_flags (sevent, GST_GAP_FLAG_MISSING_DATA);
    res = gst_pad_push_event (filter->srcpad, sevent);
  }

  return res;
}

static GstStateChangeReturn
gst_rtp_base_depayload_change_state (GstElement * element,
    GstStateChange transition)
{
  GstRTPBaseDepayload *filter;
  GstRTPBaseDepayloadPrivate *priv;
  GstStateChangeReturn ret;

  filter = GST_RTP_BASE_DEPAYLOAD (element);
  priv = filter->priv;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      filter->need_newsegment = TRUE;
      priv->npt_start = 0;
      priv->npt_stop = -1;
      priv->play_speed = 1.0;
      priv->play_scale = 1.0;
      priv->clock_base = -1;
      priv->ref_ts = -1;
      priv->onvif_mode = FALSE;
      priv->next_seqnum = -1;
      priv->negotiated = FALSE;
      priv->discont = FALSE;
      priv->segment_seqnum = GST_SEQNUM_INVALID;
      priv->hdrext_seen = FALSE;
      if (priv->hdrext_delayed)
        gst_buffer_unref (priv->hdrext_delayed);
      gst_rtp_base_depayload_reset_hdrext_buffers (filter);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_caps_replace (&priv->last_caps, NULL);
      gst_event_replace (&priv->segment_event, NULL);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}

static GstStructure *
gst_rtp_base_depayload_create_stats (GstRTPBaseDepayload * depayload)
{
  GstRTPBaseDepayloadPrivate *priv;
  GstStructure *s;
  GstClockTime pts = GST_CLOCK_TIME_NONE, dts = GST_CLOCK_TIME_NONE;

  priv = depayload->priv;

  GST_OBJECT_LOCK (depayload);
  if (depayload->segment.format != GST_FORMAT_UNDEFINED) {
    pts = gst_segment_to_running_time (&depayload->segment, GST_FORMAT_TIME,
        priv->pts);
    dts = gst_segment_to_running_time (&depayload->segment, GST_FORMAT_TIME,
        priv->dts);
  }
  GST_OBJECT_UNLOCK (depayload);

  s = gst_structure_new ("application/x-rtp-depayload-stats",
      "clock_rate", G_TYPE_UINT, depayload->clock_rate,
      "npt-start", G_TYPE_UINT64, priv->npt_start,
      "npt-stop", G_TYPE_UINT64, priv->npt_stop,
      "play-speed", G_TYPE_DOUBLE, priv->play_speed,
      "play-scale", G_TYPE_DOUBLE, priv->play_scale,
      "running-time-dts", G_TYPE_UINT64, dts,
      "running-time-pts", G_TYPE_UINT64, pts,
      "seqnum", G_TYPE_UINT, (guint) priv->last_seqnum,
      "timestamp", G_TYPE_UINT, (guint) priv->last_rtptime, NULL);

  return s;
}


static void
gst_rtp_base_depayload_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRTPBaseDepayload *depayload;
  GstRTPBaseDepayloadPrivate *priv;

  depayload = GST_RTP_BASE_DEPAYLOAD (object);
  priv = depayload->priv;

  switch (prop_id) {
    case PROP_SOURCE_INFO:
      gst_rtp_base_depayload_set_source_info_enabled (depayload,
          g_value_get_boolean (value));
      break;
    case PROP_MAX_REORDER:
      priv->max_reorder = g_value_get_int (value);
      break;
    case PROP_AUTO_HEADER_EXTENSION:
      priv->auto_hdr_ext = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_base_depayload_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRTPBaseDepayload *depayload;
  GstRTPBaseDepayloadPrivate *priv;

  depayload = GST_RTP_BASE_DEPAYLOAD (object);
  priv = depayload->priv;

  switch (prop_id) {
    case PROP_STATS:
      g_value_take_boxed (value,
          gst_rtp_base_depayload_create_stats (depayload));
      break;
    case PROP_SOURCE_INFO:
      g_value_set_boolean (value,
          gst_rtp_base_depayload_is_source_info_enabled (depayload));
      break;
    case PROP_MAX_REORDER:
      g_value_set_int (value, priv->max_reorder);
      break;
    case PROP_AUTO_HEADER_EXTENSION:
      g_value_set_boolean (value, priv->auto_hdr_ext);
      break;
    case PROP_EXTENSIONS:
      gst_rtp_base_depayload_get_extensions (depayload, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_rtp_base_depayload_set_source_info_enabled:
 * @depayload: a #GstRTPBaseDepayload
 * @enable: whether to add meta about RTP sources to buffer
 *
 * Enable or disable adding #GstRTPSourceMeta to depayloaded buffers.
 *
 * Since: 1.16
 **/
void
gst_rtp_base_depayload_set_source_info_enabled (GstRTPBaseDepayload * depayload,
    gboolean enable)
{
  depayload->priv->source_info = enable;
}

/**
 * gst_rtp_base_depayload_is_source_info_enabled:
 * @depayload: a #GstRTPBaseDepayload
 *
 * Queries whether #GstRTPSourceMeta will be added to depayloaded buffers.
 *
 * Returns: %TRUE if source-info is enabled.
 *
 * Since: 1.16
 **/
gboolean
gst_rtp_base_depayload_is_source_info_enabled (GstRTPBaseDepayload * depayload)
{
  return depayload->priv->source_info;
}

/**
 * gst_rtp_base_depayload_set_aggregate_hdrext_enabled:
 * @depayload: a #GstRTPBaseDepayload
 * @enable: whether to aggregate header extensions per output buffer
 *
 * Enable or disable aggregating header extensions.
 *
 * Since: 1.24
 **/
void
gst_rtp_base_depayload_set_aggregate_hdrext_enabled (GstRTPBaseDepayload *
    depayload, gboolean enable)
{
  depayload->priv->hdrext_aggregate = enable;
  if (!enable)
    gst_rtp_base_depayload_reset_hdrext_buffers (depayload);
}

/**
 * gst_rtp_base_depayload_is_aggregate_hdrext_enabled:
 * @depayload: a #GstRTPBaseDepayload
 *
 * Queries whether header extensions will be aggregated per depayloaded buffers.
 *
 * Returns: %TRUE if aggregate-header-extension is enabled.
 *
 * Since: 1.24
 **/
gboolean
gst_rtp_base_depayload_is_aggregate_hdrext_enabled (GstRTPBaseDepayload *
    depayload)
{
  return depayload->priv->hdrext_aggregate;
}

/**
 * gst_rtp_base_depayload_dropped:
 * @depayload: a #GstRTPBaseDepayload
 *
 * Called from @GstRTPBaseDepayload.process or
 * @GstRTPBaseDepayload.process_rtp_packet if the depayloader does not
 * use the current buffer for the output buffer. This will either drop
 * the delayed buffer or the last buffer from the header extension
 * cache.
 *
 * A typical use-case is when the depayloader implementation is
 * dropping an input RTP buffer while waiting for the first keyframe.
 *
 * Must be called with the stream lock held.
 *
 * Since: 1.24
 **/
void
gst_rtp_base_depayload_dropped (GstRTPBaseDepayload * depayload)
{
  GstRTPBaseDepayloadPrivate *priv = depayload->priv;
  guint l = gst_buffer_list_length (priv->hdrext_buffers);

  if (priv->hdrext_delayed) {
    gst_clear_buffer (&priv->hdrext_delayed);
  } else if (l) {
    gst_buffer_list_remove (priv->hdrext_buffers, l - 1, 1);
  }
}

/**
 * gst_rtp_base_depayload_delayed:
 * @depayload: a #GstRTPBaseDepayload
 *
 * Called from @GstRTPBaseDepayload.process or
 * @GstRTPBaseDepayload.process_rtp_packet when the depayloader needs
 * to keep the current input RTP header for use with the next output
 * buffer.
 *
 * The delayed buffer will remain until the end of processing the
 * current output buffer and then enqueued for processing with the
 * next output buffer.
 *
 * A typical use-case is when the depayloader implementation will
 * start a new output buffer for the current input RTP buffer but push
 * the current output buffer first.
 *
 * Must be called with the stream lock held.
 *
 * Since: 1.24
 **/
void
gst_rtp_base_depayload_delayed (GstRTPBaseDepayload * depayload)
{
  GstRTPBaseDepayloadPrivate *priv = depayload->priv;
  guint l = gst_buffer_list_length (priv->hdrext_buffers);

  if (l) {
    priv->hdrext_delayed = gst_buffer_list_get (priv->hdrext_buffers, l - 1);
    gst_buffer_ref (priv->hdrext_delayed);
    gst_buffer_list_remove (priv->hdrext_buffers, l - 1, 1);
  }
}

/**
 * gst_rtp_base_depayload_flush:
 * @depayload: a #GstRTPBaseDepayload
 * @keep_current: if the current RTP buffer shall be kept
 *
 * If @GstRTPBaseDepayload.process or
 * @GstRTPBaseDepayload.process_rtp_packet drop an output buffer this
 * function tells the base class to flush header extension cache as
 * well.
 *
 * This will not drop an input RTP header marked as delayed from
 * gst_rtp_base_depayload_delayed().
 *
 * If @keep_current is %TRUE the current input RTP header will be kept
 * and enqueued after flushing the previous input RTP headers.
 *
 * A typical use-case for @keep_current is when the depayloader
 * implementation invalidates the current output buffer and starts a
 * new one with the current RTP input buffer.
 *
 * Must be called with the stream lock held.
 *
 * Since: 1.24
 **/
void
gst_rtp_base_depayload_flush (GstRTPBaseDepayload * depayload,
    gboolean keep_current)
{
  GstRTPBaseDepayloadPrivate *priv = depayload->priv;
  guint l = gst_buffer_list_length (priv->hdrext_buffers);

  /* if the current buffer shall not be kept or has already been
     removed from the cache clear the cache */
  if (!keep_current || priv->hdrext_delayed) {
    gst_rtp_base_depayload_reset_hdrext_buffers (depayload);
  } else if (l) {
    /* clear all cached buffers (if any) except the delayed */
    GstBuffer *b = gst_buffer_list_get (priv->hdrext_buffers, l - 1);
    gst_buffer_ref (b);
    gst_rtp_base_depayload_reset_hdrext_buffers (depayload);
    gst_buffer_list_add (priv->hdrext_buffers, b);
  }
}
