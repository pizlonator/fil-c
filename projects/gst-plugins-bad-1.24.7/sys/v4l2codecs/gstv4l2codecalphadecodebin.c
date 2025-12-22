/* GStreamer
 * Copyright (C) <2021> Collabora Ltd.
 *   Author: Daniel Almeida <daniel.almeida@collabora.com>
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

#include <gst/pbutils/pbutils.h>

#include "gstv4l2codecalphadecodebin.h"
#include "gstv4l2decoder.h"

GST_DEBUG_CATEGORY_STATIC (v4l2_codecalphadecodebin_debug);
#define GST_CAT_DEFAULT (v4l2_codecalphadecodebin_debug)

typedef struct
{
  GstBin parent;

  gboolean constructed;
  const gchar *missing_element;
} GstV4l2CodecAlphaDecodeBinPrivate;

#define gst_v4l2_codec_alpha_decode_bin_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstV4l2CodecAlphaDecodeBin,
    gst_v4l2_codec_alpha_decode_bin, GST_TYPE_BIN,
    G_ADD_PRIVATE (GstV4l2CodecAlphaDecodeBin);
    GST_DEBUG_CATEGORY_INIT (v4l2_codecalphadecodebin_debug,
        "v4l2codecs-alphadecodebin", 0, "V4L2 stateless alpha decode bin"));


static GstStaticPadTemplate gst_alpha_decode_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static gboolean
gst_v4l2_codec_alpha_decode_bin_open (GstV4l2CodecAlphaDecodeBin * self)
{
  GstV4l2CodecAlphaDecodeBinPrivate *priv =
      gst_v4l2_codec_alpha_decode_bin_get_instance_private (self);

  if (priv->missing_element) {
    gst_element_post_message (GST_ELEMENT (self),
        gst_missing_element_message_new (GST_ELEMENT (self),
            priv->missing_element));
  } else if (!priv->constructed) {
    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to construct alpha decoder pipeline."), (NULL));
  }

  return priv->constructed;
}

static GstStateChangeReturn
gst_v4l2_codec_alpha_decode_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  GstV4l2CodecAlphaDecodeBin *self = GST_V4L2_CODEC_ALPHA_DECODE_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_v4l2_codec_alpha_decode_bin_open (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_v4l2_codec_alpha_decode_bin_constructed (GObject * obj)
{
  GstV4l2CodecAlphaDecodeBin *self = GST_V4L2_CODEC_ALPHA_DECODE_BIN (obj);
  GstV4l2CodecAlphaDecodeBinPrivate *priv =
      gst_v4l2_codec_alpha_decode_bin_get_instance_private (self);
  GstV4l2CodecAlphaDecodeBinClass *klass =
      GST_V4L2_CODEC_ALPHA_DECODE_BIN_GET_CLASS (self);
  GstPad *src_gpad, *sink_gpad;
  GstPad *src_pad = NULL, *sink_pad = NULL;
  GstElement *alphademux = NULL;
  GstElement *queue = NULL;
  GstElement *alpha_queue = NULL;
  GstElement *decoder = NULL;
  GstElement *alpha_decoder = NULL;
  GstElement *alphacombine = NULL;

  /* setup ghost pads */
  sink_gpad = gst_ghost_pad_new_no_target_from_template ("sink",
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink"));
  gst_element_add_pad (GST_ELEMENT (self), sink_gpad);

  src_gpad = gst_ghost_pad_new_no_target_from_template ("src",
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src"));
  gst_element_add_pad (GST_ELEMENT (self), src_gpad);

  /* create elements */
  alphademux = gst_element_factory_make ("codecalphademux", NULL);
  if (!alphademux) {
    priv->missing_element = "codecalphademux";
    goto cleanup;
  }

  queue = gst_element_factory_make ("queue", NULL);
  alpha_queue = gst_element_factory_make ("queue", NULL);
  if (!queue || !alpha_queue) {
    priv->missing_element = "queue";
    goto cleanup;
  }

  decoder = gst_element_factory_make (klass->decoder_name, "maindec");
  if (!decoder) {
    priv->missing_element = klass->decoder_name;
    goto cleanup;
  }

  alpha_decoder = gst_element_factory_make (klass->decoder_name, "alphadec");
  if (!alpha_decoder) {
    priv->missing_element = klass->decoder_name;
    goto cleanup;
  }

  /* We disable QoS on decoders because we need to maintain frame pairing in
   * order for alphacombine to work. */
  g_object_set (decoder, "qos", FALSE, NULL);
  g_object_set (alpha_decoder, "qos", FALSE, NULL);

  alphacombine = gst_element_factory_make ("alphacombine", NULL);
  if (!alphacombine) {
    priv->missing_element = "alphacombine";
    goto cleanup;
  }

  gst_bin_add_many (GST_BIN (self), alphademux, queue, alpha_queue, decoder,
      alpha_decoder, alphacombine, NULL);

  /* link elements */
  sink_pad = gst_element_get_static_pad (alphademux, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (sink_gpad), sink_pad);
  gst_clear_object (&sink_pad);

  gst_element_link_pads (alphademux, "src", queue, "sink");
  gst_element_link_pads (queue, "src", decoder, "sink");
  gst_element_link_pads (decoder, "src", alphacombine, "sink");

  gst_element_link_pads (alphademux, "alpha", alpha_queue, "sink");
  gst_element_link_pads (alpha_queue, "src", alpha_decoder, "sink");
  gst_element_link_pads (alpha_decoder, "src", alphacombine, "alpha");

  src_pad = gst_element_get_static_pad (alphacombine, "src");
  gst_ghost_pad_set_target (GST_GHOST_PAD (src_gpad), src_pad);
  gst_object_unref (src_pad);

  g_object_set (queue, "max-size-bytes", 0, "max-size-time",
      G_GUINT64_CONSTANT (0), "max-size-buffers", 1, NULL);
  g_object_set (alpha_queue, "max-size-bytes", 0, "max-size-time",
      G_GUINT64_CONSTANT (0), "max-size-buffers", 1, NULL);

  /* signal success, we will handle this in NULL->READY transition */
  priv->constructed = TRUE;
  return;

cleanup:
  gst_clear_object (&alphademux);
  gst_clear_object (&queue);
  gst_clear_object (&alpha_queue);
  gst_clear_object (&decoder);
  gst_clear_object (&alpha_decoder);
  gst_clear_object (&alphacombine);

  G_OBJECT_CLASS (parent_class)->constructed (obj);
}

static void
gst_v4l2_codec_alpha_decode_bin_class_init (GstV4l2CodecAlphaDecodeBinClass *
    klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *obj_class = (GObjectClass *) klass;

  /* This is needed to access the subclass class instance, otherwise we cannot
   * read the class parameters */
  obj_class->constructed = gst_v4l2_codec_alpha_decode_bin_constructed;

  gst_element_class_add_static_pad_template (element_class,
      &gst_alpha_decode_bin_src_template);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_v4l2_codec_alpha_decode_bin_change_state);

  /* let's make the doc generator happy */
  gst_type_mark_as_plugin_api (GST_TYPE_V4L2_CODEC_ALPHA_DECODE_BIN, 0);
}

static void
gst_v4l2_codec_alpha_decode_bin_init (GstV4l2CodecAlphaDecodeBin * self)
{
}

void
gst_v4l2_codec_alpha_decode_bin_register (GstPlugin * plugin,
    GClassInitFunc class_init, gconstpointer class_data,
    const gchar * element_name_tmpl, GstV4l2CodecDevice * device, guint rank)
{
  /* TODO check that we have compatible src format */

  gst_v4l2_decoder_register (plugin,
      GST_TYPE_V4L2_CODEC_ALPHA_DECODE_BIN, class_init, class_data, NULL,
      element_name_tmpl, device,
      rank + GST_V4L2_CODEC_ALPHA_DECODE_BIN_RANK_OFFSET, NULL);
}
