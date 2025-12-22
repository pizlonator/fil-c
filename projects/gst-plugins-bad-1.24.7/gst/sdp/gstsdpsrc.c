/*
 * Copyright (C) 2016 Sebastian Dröge <sebastian@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsdpsrc.h"
#include <gst/app/app.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (sdp_src_debug);
#define GST_CAT_DEFAULT sdp_src_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("stream_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_SDP
};

static void gst_sdp_src_handler_init (gpointer g_iface, gpointer iface_data);

#define gst_sdp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstSdpSrc, gst_sdp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_sdp_src_handler_init));
GST_ELEMENT_REGISTER_DEFINE (sdpsrc, "sdpsrc", GST_RANK_NONE, GST_TYPE_SDP_SRC);

static void
gst_sdp_src_finalize (GObject * object)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (object);

  if (self->sdp_buffer)
    gst_buffer_unref (self->sdp_buffer);
  g_free (self->location);
  g_free (self->sdp);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_sdp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;
    case PROP_SDP:
      g_value_set_string (value, self->sdp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_sdp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (self->location);
      self->location = g_value_dup_string (value);
      break;
    case PROP_SDP:
      g_free (self->sdp);
      self->sdp = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
pad_added_cb (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (user_data);
  GstPad *ghost;

  ghost =
      gst_ghost_pad_new_from_template (GST_PAD_NAME (pad), pad,
      gst_static_pad_template_get (&src_template));
  gst_pad_set_active (ghost, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (self), ghost);
  g_object_set_data (G_OBJECT (pad), "GstSdpSrc.ghostpad", ghost);
}

static void
pad_removed_cb (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (user_data);
  GstPad *ghost;

  ghost = g_object_get_data (G_OBJECT (pad), "GstSdpSrc.ghostpad");
  if (ghost) {
    g_object_set_data (G_OBJECT (pad), "GstSdpSrc.ghostpad", NULL);

    gst_pad_set_active (ghost, FALSE);
    gst_element_remove_pad (GST_ELEMENT_CAST (self), ghost);
  }
}

static void
no_more_pads_cb (GstElement * element, gpointer user_data)
{
  gst_element_no_more_pads (GST_ELEMENT_CAST (user_data));
}

static void
remove_pad (const GValue * item, gpointer user_data)
{
  GstElement *self = user_data;
  GstPad *pad = g_value_get_object (item);

  gst_element_remove_pad (self, pad);
}

static GstStateChangeReturn
gst_sdp_src_change_state (GstElement * element, GstStateChange transition)
{
  GstSdpSrc *self = GST_SDP_SRC_CAST (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_OBJECT_LOCK (self);
      if (self->sdp_buffer)
        gst_buffer_unref (self->sdp_buffer);
      self->sdp_buffer = NULL;

      if (self->location && strcmp (self->location, "sdp://") != 0) {
        /* Do nothing */
      } else if (self->sdp) {
        guint sdp_len = strlen (self->sdp);

        self->sdp_buffer =
            gst_buffer_new_wrapped (g_strndup (self->sdp, sdp_len),
            sdp_len + 1);
      } else {
        ret = GST_STATE_CHANGE_FAILURE;
      }
      GST_OBJECT_UNLOCK (self);

      if (ret != GST_STATE_CHANGE_FAILURE) {
        if (self->sdp_buffer) {
          GstCaps *caps = gst_caps_new_empty_simple ("application/sdp");

          self->src = gst_element_factory_make ("appsrc", NULL);
          g_object_set (self->src, "caps", caps, "emit-signals", FALSE, NULL);
          gst_caps_unref (caps);
        } else {
          self->src = gst_element_factory_make ("filesrc", NULL);
          g_object_set (self->src, "location", self->location + 6, NULL);
        }

        self->demux = gst_element_factory_make ("sdpdemux", NULL);
        g_signal_connect (self->demux, "pad-added", G_CALLBACK (pad_added_cb),
            self);
        g_signal_connect (self->demux, "pad-removed",
            G_CALLBACK (pad_removed_cb), self);
        g_signal_connect (self->demux, "no-more-pads",
            G_CALLBACK (no_more_pads_cb), self);
        gst_bin_add_many (GST_BIN_CAST (self), self->src, self->demux, NULL);
        gst_element_link_pads (self->src, "src", self->demux, "sink");
      }
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:{
      GstIterator *it;

      it = gst_element_iterate_src_pads (GST_ELEMENT_CAST (self));
      while (gst_iterator_foreach (it, remove_pad, self) == GST_ITERATOR_RESYNC)
        gst_iterator_resync (it);
      gst_iterator_free (it);

      if (self->src) {
        gst_bin_remove (GST_BIN_CAST (self), self->src);
        self->src = NULL;
      }
      if (self->demux) {
        gst_bin_remove (GST_BIN_CAST (self), self->demux);
        self->demux = NULL;
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (ret != GST_STATE_CHANGE_FAILURE)
        ret = GST_STATE_CHANGE_NO_PREROLL;
      if (self->sdp_buffer) {
        if (gst_app_src_push_buffer (GST_APP_SRC_CAST (self->src),
                gst_buffer_ref (self->sdp_buffer)) != GST_FLOW_OK)
          ret = GST_STATE_CHANGE_FAILURE;
        else
          gst_app_src_end_of_stream (GST_APP_SRC_CAST (self->src));
      }
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_sdp_src_class_init (GstSdpSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (sdp_src_debug, "sdpsrc", 0, "SDP Source");

  gobject_class->finalize = gst_sdp_src_finalize;
  gobject_class->set_property = gst_sdp_src_set_property;
  gobject_class->get_property = gst_sdp_src_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location",
          "Location",
          "URI to SDP file (sdp:///path/to/file)", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SDP,
      g_param_spec_string ("sdp",
          "SDP",
          "SDP description used instead of location", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (element_class, "SDP Source",
      "Source/Network/RTP",
      "Stream RTP based on an SDP",
      "Sebastian Dröge <sebastian@centricular.com>");

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_sdp_src_change_state);
}

static void
gst_sdp_src_init (GstSdpSrc * self)
{
}

static GstURIType
gst_sdp_src_get_uri_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_sdp_src_get_protocols (GType type)
{
  static const gchar *protocols[] = { "sdp", 0 };

  return protocols;
}

static gchar *
gst_sdp_src_get_uri (GstURIHandler * handler)
{
  gchar *uri = NULL;

  g_object_get (handler, "location", &uri, NULL);

  return uri;
}

static gboolean
gst_sdp_src_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  if (uri && !g_str_has_prefix (uri, "sdp://")) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Invalid SDP URI");
    return FALSE;
  }

  g_object_set (handler, "location", uri, NULL);

  return TRUE;
}

static void
gst_sdp_src_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_sdp_src_get_uri_type;
  iface->get_protocols = gst_sdp_src_get_protocols;
  iface->get_uri = gst_sdp_src_get_uri;
  iface->set_uri = gst_sdp_src_set_uri;
}
