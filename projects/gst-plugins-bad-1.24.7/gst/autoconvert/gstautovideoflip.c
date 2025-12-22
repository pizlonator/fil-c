/* GStreamer
 * Copyright 2023 Igalia S.L.
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
 * SECTION:element-autovideoflip
 * @title: autovideoflip
 *
 * The #autovideoflip element is used to flip the video plugging the right
 * element depending on caps and underlying buffer memory.
 *
 * Since: 1.24
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/video/video.h>

#include "gstautovideo.h"
#include "gstautovideoflip.h"

GST_DEBUG_CATEGORY (autovideoflip_debug);
#define GST_CAT_DEFAULT (autovideoflip_debug)

#define PROP_DIRECTION_DEFAULT GST_VIDEO_ORIENTATION_IDENTITY

/* GstVideoFlip properties */
enum
{
  PROP_0,
  PROP_VIDEO_DIRECTION
};

struct _GstAutoVideoFlip
{
  GstBaseAutoConvert parent;

  GstVideoOrientationMethod direction;
  GList *bindings;
};

G_DEFINE_TYPE (GstAutoVideoFlip, gst_auto_video_flip,
    GST_TYPE_BASE_AUTO_CONVERT);

GST_ELEMENT_REGISTER_DEFINE (autovideoflip, "autovideoflip",
    GST_RANK_NONE, gst_auto_video_flip_get_type ());

static void
gst_auto_video_flip_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAutoVideoFlip *self = GST_AUTO_VIDEO_FLIP (object);

  switch (prop_id) {
    case PROP_VIDEO_DIRECTION:
      self->direction = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_auto_video_flip_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAutoVideoFlip *self = GST_AUTO_VIDEO_FLIP (object);

  switch (prop_id) {
    case PROP_VIDEO_DIRECTION:
      g_value_set_enum (value, self->direction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#if !GLIB_CHECK_VERSION(2, 68, 0)
static GObject *
g_binding_dup_target (GBinding * binding)
{
  return g_object_ref (g_binding_get_target (binding));
}
#endif

static gboolean
element_is_handled_video_flip (GstElement * element)
{
  GstElementFactory *factory = gst_element_get_factory (element);

  return !g_strcmp0 (GST_OBJECT_NAME (factory), "glvideoflip") ||
      !g_strcmp0 (GST_OBJECT_NAME (factory), "videoflip");
}

static gboolean
gst_auto_video_flip_transform_to (GBinding * binding, const GValue * from,
    GValue * to_value, gpointer _udata)
{
  g_value_set_enum (to_value, g_value_get_enum (from));

  return TRUE;
}

static void
gst_auto_video_flip_deep_element_added (GstBin * bin, GstBin * sub_bin,
    GstElement * element)
{
  GstAutoVideoFlip *self = GST_AUTO_VIDEO_FLIP (bin);
  GList *new_bindings = NULL;

  if (!element_is_handled_video_flip (element))
    goto done;

  GST_OBJECT_LOCK (bin);
  for (GList * tmp = self->bindings; tmp; tmp = tmp->next) {
    GBinding *binding = tmp->data;
    GObject *target = g_binding_dup_target (binding);

    if (GST_ELEMENT (target) == element) {
      GST_INFO_OBJECT (self, "Newly added element %s already bound",
          GST_OBJECT_NAME (gst_element_get_factory (element)));
      GST_OBJECT_UNLOCK (bin);
      gst_object_unref (target);
      goto done;
    }
    gst_object_unref (target);
  }
  GST_OBJECT_UNLOCK (bin);

  new_bindings = g_list_prepend (new_bindings,
      g_object_bind_property_full (bin, "video-direction",
          element, "video-direction",
          G_BINDING_SYNC_CREATE | G_BINDING_DEFAULT,
          gst_auto_video_flip_transform_to, NULL, NULL, NULL)
      );

  GST_OBJECT_LOCK (bin);
  self->bindings = g_list_concat (self->bindings, new_bindings);
  GST_OBJECT_UNLOCK (bin);

done:
  GST_BIN_CLASS (gst_auto_video_flip_parent_class)->deep_element_added (bin,
      sub_bin, element);
}

static void
gst_auto_video_flip_deep_element_removed (GstBin * bin, GstBin * sub_bin,
    GstElement * element)
{
  GList *bindings = NULL;
  GstAutoVideoFlip *self = GST_AUTO_VIDEO_FLIP (bin);

  if (!element_is_handled_video_flip (element))
    goto done;

  GST_OBJECT_LOCK (bin);
  for (GList * tmp = self->bindings; tmp; tmp = tmp->next) {
    GBinding *binding = tmp->data;
    GObject *target = g_binding_dup_target (binding);

    if (GST_ELEMENT (target) == element) {
      GList *node = tmp;

      bindings = g_list_prepend (bindings, binding);
      tmp = tmp->prev;

      self->bindings = g_list_delete_link (self->bindings, node);
      if (!tmp)
        break;
    }
    gst_object_unref (target);
  }
  GST_OBJECT_UNLOCK (bin);

done:
  GST_BIN_CLASS (gst_auto_video_flip_parent_class)->deep_element_removed (bin,
      sub_bin, element);
}


static void
gst_auto_video_flip_class_init (GstAutoVideoFlipClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;

  GST_DEBUG_CATEGORY_INIT (autovideoflip_debug, "autovideoflip", 0,
      "Auto video flipper");

  gobject_class->set_property = gst_auto_video_flip_set_property;
  gobject_class->get_property = gst_auto_video_flip_get_property;
  g_object_class_install_property (gobject_class, PROP_VIDEO_DIRECTION,
      g_param_spec_enum ("video-direction", "video-direction",
          "Video direction: rotation and flipping",
          GST_TYPE_VIDEO_ORIENTATION_METHOD, PROP_DIRECTION_DEFAULT,
          GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Flip the video plugging the right element depending on caps",
      "Bin/Filter/Effect/Video",
      "Selects the right video flip element based on the caps",
      "Thibault Saunier <tsaunier@igalia.com>");

  gstbin_class->deep_element_added = gst_auto_video_flip_deep_element_added;
  gstbin_class->deep_element_removed = gst_auto_video_flip_deep_element_removed;
}

static void
gst_auto_video_flip_init (GstAutoVideoFlip * self)
{
  self->direction = PROP_DIRECTION_DEFAULT;

  /* *INDENT-OFF* */
  static const GstAutoVideoFilterGenerator gen[] = {
    {
      .first_elements = { "bayer2rgb", NULL},
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { NULL } ,
      .filters = { "videoflip", NULL },
      .rank = GST_RANK_MARGINAL,
    },
    {
      .first_elements = { NULL, },
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { NULL },
      .filters = {  "videoflip" },
      .rank = GST_RANK_SECONDARY,
    },
    {
      .first_elements = { NULL, },
      .colorspace_converters = { "videoconvertscale", NULL },
      .last_elements = { "rgb2bayer", NULL },
      .filters = {  "videoflip" },
      .rank = GST_RANK_MARGINAL,
    },
    {
      .first_elements = { "glupload" },
      .colorspace_converters = { "glcolorconvert", NULL },
      .last_elements = { NULL, },
      .filters = { "glvideoflip" },
      .rank = GST_RANK_PRIMARY,
    },
    {
      .first_elements = { NULL },
      .colorspace_converters = { "glcolorconvert", NULL },
      .last_elements = { NULL, },
      .filters = { "glvideoflip" },
      .rank = GST_RANK_PRIMARY,
    },
    {
      .first_elements = { "videoconvertscale", "glupload", NULL },
      .colorspace_converters = { NULL },
      .last_elements = { NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_MARGINAL + 1,
    },
    {
      .first_elements = { NULL },
      .colorspace_converters = { NULL },
      .last_elements = { "gldownload", NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_MARGINAL + 1,
    },
    { /* CUDA -> GL */
      .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_PRIMARY - 1,
    },
    { /* CUDA -> CUDA */
      .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_SECONDARY - 1,
    },
    { /* Software -> CUDA (uploading as soon as possible) */
      .first_elements = { "glupload", NULL },
      .colorspace_converters = { "glcoloconvert", NULL },
      .last_elements = { "cudaupload", "capsfilter caps=video/x-raw(memory:CUDAMemory)", NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_MARGINAL,
    },
    { /* CUDA -> Software */
      .first_elements = { "capsfilter caps=video/x-raw(memory:CUDAMemory)", "cudadownload", NULL },
      .colorspace_converters = { "glcolorconvert",  "glcolorscale", "glcolorconvert", NULL },
      .last_elements = { "gldownload", NULL },
      .filters = { "glvideoflip", NULL },
      .rank = GST_RANK_MARGINAL,
    },
    { /* Worst case we upload/download as required */
      .first_elements = { NULL},
      .colorspace_converters = { NULL },
      .last_elements = { NULL },
      .filters = { NULL },
      .rank = 0,
    },
  };
  /* *INDENT-ON* */


  gst_auto_video_register_well_known_bins (GST_BASE_AUTO_CONVERT (self), gen);
}
