/* GStreamer
 * Copyright (C) <2015> Jan Schmidt <jan@centricular.com>
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
#  include "config.h"
#endif

#include <string.h>

#include "video.h"

GType
gst_video_multiview_flagset_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter (&type)) {
    GType _type = gst_flagset_register (GST_TYPE_VIDEO_MULTIVIEW_FLAGS);
    g_once_init_leave (&type, _type);
  }
  return type;
}


/* Caps mnemonics for the various multiview representations */

static const struct mview_map_t
{
  const gchar *caps_repr;
  GstVideoMultiviewMode mode;
} gst_multiview_modes[] = {
  {
      "mono", GST_VIDEO_MULTIVIEW_MODE_MONO}, {
      "left", GST_VIDEO_MULTIVIEW_MODE_LEFT}, {
      "right", GST_VIDEO_MULTIVIEW_MODE_RIGHT}, {
      "side-by-side", GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE}, {
      "side-by-side-quincunx", GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX}, {
      "column-interleaved", GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED}, {
      "row-interleaved", GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED}, {
      "top-bottom", GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM}, {
      "checkerboard", GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD}, {
      "frame-by-frame", GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME}, {
        "multiview-frame-by-frame",
      GST_VIDEO_MULTIVIEW_MODE_MULTIVIEW_FRAME_BY_FRAME}, {
      "separated", GST_VIDEO_MULTIVIEW_MODE_SEPARATED}
};

/**
 * gst_video_multiview_mode_to_caps_string:
 * @mview_mode: A #GstVideoMultiviewMode value
 *
 * Given a #GstVideoMultiviewMode returns the multiview-mode caps string
 * for insertion into a caps structure
 *
 * Returns: (nullable): The caps string representation of the mode, or NULL if invalid.
 *
 * Since: 1.6
 */
const gchar *
gst_video_multiview_mode_to_caps_string (GstVideoMultiviewMode mview_mode)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (gst_multiview_modes); i++) {
    if (gst_multiview_modes[i].mode == mview_mode) {
      return gst_multiview_modes[i].caps_repr;
    }
  }

  return NULL;
}

/**
 * gst_video_multiview_mode_from_caps_string:
 * @caps_mview_mode: multiview-mode field string from caps
 *
 * Returns: The #GstVideoMultiviewMode value
 *
 * Given a string from a caps multiview-mode field,
 * output the corresponding #GstVideoMultiviewMode
 * or #GST_VIDEO_MULTIVIEW_MODE_NONE
 *
 * Since: 1.6
 */
GstVideoMultiviewMode
gst_video_multiview_mode_from_caps_string (const gchar * caps_mview_mode)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (gst_multiview_modes); i++) {
    if (g_str_equal (gst_multiview_modes[i].caps_repr, caps_mview_mode)) {
      return gst_multiview_modes[i].mode;
    }
  }

  GST_ERROR ("Invalid multiview info %s", caps_mview_mode);
  g_warning ("Invalid multiview info %s", caps_mview_mode);

  return GST_VIDEO_MULTIVIEW_MODE_NONE;
}

/* Array of mono, unpacked, double-height and double-width modes */
static GValue mode_values[5];

static void
init_mview_mode_vals (void)
{
  static gsize mview_mode_vals_init = 0;

  if (g_once_init_enter (&mview_mode_vals_init)) {
    GValue item = { 0, };
    GValue *list;

    g_value_init (&item, G_TYPE_STRING);

    /* Mono modes */
    list = mode_values;
    g_value_init (list, GST_TYPE_LIST);
    g_value_set_static_string (&item, "mono");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "left");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "right");
    gst_value_list_append_value (list, &item);

    /* Unpacked modes - ones split across buffers or memories */
    list = mode_values + 1;
    g_value_init (list, GST_TYPE_LIST);
    g_value_set_static_string (&item, "separated");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "frame-by-frame");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "multiview-frame-by-frame");
    gst_value_list_append_value (list, &item);

    /* Double height modes */
    list = mode_values + 2;
    g_value_init (list, GST_TYPE_LIST);
    g_value_set_static_string (&item, "top-bottom");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "row-interleaved");
    gst_value_list_append_value (list, &item);

    /* Double width modes */
    list = mode_values + 3;
    g_value_init (list, GST_TYPE_LIST);
    g_value_set_static_string (&item, "side-by-side");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "side-by-side-quincunx");
    gst_value_list_append_value (list, &item);
    g_value_set_static_string (&item, "column-interleaved");
    gst_value_list_append_value (list, &item);

    /* Double size (both width & height) modes */
    list = mode_values + 4;
    g_value_init (list, GST_TYPE_LIST);
    g_value_set_static_string (&item, "checkerboard");
    gst_value_list_append_value (list, &item);

    g_value_unset (&item);
    g_once_init_leave (&mview_mode_vals_init, 1);
  }
}

/**
 * gst_video_multiview_get_mono_modes:
 *
 * Returns: A const #GValue containing a list of mono video modes
 *
 * Utility function that returns a #GValue with a GstList of mono video
 * modes (mono/left/right) for use in caps negotiations.
 *
 * Since: 1.6
 */
const GValue *
gst_video_multiview_get_mono_modes (void)
{
  init_mview_mode_vals ();
  return mode_values;
}

/**
 * gst_video_multiview_get_unpacked_modes:
 *
 * Returns: A const #GValue containing a list of 'unpacked' stereo video modes
 *
 * Utility function that returns a #GValue with a GstList of unpacked
 * stereo video modes (separated/frame-by-frame/frame-by-frame-multiview)
 * for use in caps negotiations.
 *
 * Since: 1.6
 */
const GValue *
gst_video_multiview_get_unpacked_modes (void)
{
  init_mview_mode_vals ();
  return mode_values + 1;
}

/**
 * gst_video_multiview_get_doubled_height_modes:
 *
 * Returns: A const #GValue containing a list of stereo video modes
 *
 * Utility function that returns a #GValue with a GstList of packed stereo
 * video modes with double the height of a single view for use in
 * caps negotiations. Currently this is top-bottom and row-interleaved.
 *
 * Since: 1.6
 */
const GValue *
gst_video_multiview_get_doubled_height_modes (void)
{
  init_mview_mode_vals ();
  return mode_values + 2;
}

/**
 * gst_video_multiview_get_doubled_width_modes:
 *
 * Returns: A const #GValue containing a list of stereo video modes
 *
 * Utility function that returns a #GValue with a GstList of packed stereo
 * video modes with double the width of a single view for use in
 * caps negotiations. Currently this is side-by-side, side-by-side-quincunx
 * and column-interleaved.
 *
 * Since: 1.6
 */
const GValue *
gst_video_multiview_get_doubled_width_modes (void)
{
  init_mview_mode_vals ();
  return mode_values + 3;
}

/**
 * gst_video_multiview_get_doubled_size_modes:
 *
 * Returns: A const #GValue containing a list of stereo video modes
 *
 * Utility function that returns a #GValue with a GstList of packed
 * stereo video modes that have double the width/height of a single
 * view for use in caps negotiation. Currently this is just
 * 'checkerboard' layout.
 *
 * Since: 1.6
 */
const GValue *
gst_video_multiview_get_doubled_size_modes (void)
{
  init_mview_mode_vals ();
  return mode_values + 4;
}

static void
gst_video_multiview_separated_video_info_from_packed (GstVideoInfo * info)
{
  GstVideoMultiviewMode mview_mode;

  mview_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (info);

  /* Normalise the half-aspect flag by adjusting PAR */
  switch (mview_mode) {
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE:
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX:
    case GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED:
    case GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD:
      info->width /= 2;
      info->views *= 2;
      GST_VIDEO_INFO_MULTIVIEW_MODE (info) = GST_VIDEO_MULTIVIEW_MODE_SEPARATED;
      if (GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) &
          GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT)
        info->par_n *= 2;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED:
    case GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM:
      info->height /= 2;
      info->views *= 2;
      GST_VIDEO_INFO_MULTIVIEW_MODE (info) = GST_VIDEO_MULTIVIEW_MODE_SEPARATED;
      if (GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) &
          GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT)
        info->par_d *= 2;
      break;
    default:
      /* Mono/left/right/frame-by-frame/already separated */
      break;
  }
  GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) &=
      ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT;
}

static void
gst_video_multiview_separated_video_info_to_packed (GstVideoInfo * info,
    GstVideoMultiviewMode packed_mview_mode,
    GstVideoMultiviewFlags packed_mview_flags)
{
  /* Convert single-frame info to a packed mode */
  GST_VIDEO_INFO_MULTIVIEW_MODE (info) = packed_mview_mode;
  GST_VIDEO_INFO_MULTIVIEW_FLAGS (info) = packed_mview_flags;

  switch (packed_mview_mode) {
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE:
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX:
    case GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED:
    case GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD:
      info->width *= 2;
      info->views /= 2;
      if (packed_mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT)
        info->par_d *= 2;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED:
    case GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM:
      info->height *= 2;
      info->views /= 2;
      if (packed_mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT)
        info->par_n *= 2;
      break;
    default:
      break;
  }
}

/**
 * gst_video_multiview_video_info_change_mode:
 * @info: A #GstVideoInfo structure to operate on
 * @out_mview_mode: A #GstVideoMultiviewMode value
 * @out_mview_flags: A set of #GstVideoMultiviewFlags
 *
 * Utility function that transforms the width/height/PAR
 * and multiview mode and flags of a #GstVideoInfo into
 * the requested mode.
 *
 * Since: 1.6
 */
void
gst_video_multiview_video_info_change_mode (GstVideoInfo * info,
    GstVideoMultiviewMode out_mview_mode,
    GstVideoMultiviewFlags out_mview_flags)
{
  gst_video_multiview_separated_video_info_from_packed (info);
  gst_video_multiview_separated_video_info_to_packed (info, out_mview_mode,
      out_mview_flags);
}

/**
 * gst_video_multiview_guess_half_aspect:
 * @mv_mode: A #GstVideoMultiviewMode
 * @width: Video frame width in pixels
 * @height: Video frame height in pixels
 * @par_n: Numerator of the video pixel-aspect-ratio
 * @par_d: Denominator of the video pixel-aspect-ratio
 *
 * Returns: A boolean indicating whether the
 *   #GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT flag should be set.
 *
 * Utility function that heuristically guess whether a
 * frame-packed stereoscopic video contains half width/height
 * encoded views, or full-frame views by looking at the
 * overall display aspect ratio.
 *
 * Since: 1.6
 */
gboolean
gst_video_multiview_guess_half_aspect (GstVideoMultiviewMode mv_mode,
    guint width, guint height, guint par_n, guint par_d)
{
  switch (mv_mode) {
    case GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM:
    case GST_VIDEO_MULTIVIEW_MODE_ROW_INTERLEAVED:
      /* If the video is wider than it is tall, assume half aspect */
      if (height * par_d <= width * par_n)
        return TRUE;
      break;
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE:
    case GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX:
    case GST_VIDEO_MULTIVIEW_MODE_COLUMN_INTERLEAVED:
      /* If the video DAR is less than 2.39:1, assume half-aspect */
      if (width * par_n < 2.39 * height * par_d)
        return TRUE;
      break;
    default:
      break;
  }
  return FALSE;
}

#if 0                           /* Multiview meta disabled for now */
GType
gst_video_multiview_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR,
    NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVideoMultiviewMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_video_multiview_meta_init (GstVideoMultiviewMeta * mview_meta,
    gpointer params, GstBuffer * buffer)
{
  mview_meta->n_views = 0;
  mview_meta->view_info = NULL;

  return TRUE;
}

static void
gst_video_multiview_meta_free (GstVideoMultiviewMeta * mview_meta,
    GstBuffer * buffer)
{
  g_free (mview_meta->view_info);
}

/* video multiview metadata */
const GstMetaInfo *
gst_video_multiview_meta_get_info (void)
{
  static const GstMetaInfo *video_meta_info = NULL;

  if (g_once_init_enter (&video_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_VIDEO_MULTIVIEW_META_API_TYPE,
        "GstVideoMultiviewMeta",
        sizeof (GstVideoMultiviewMeta),
        (GstMetaInitFunction) gst_video_multiview_meta_init,
        (GstMetaFreeFunction) gst_video_multiview_meta_free,
        NULL);
    g_once_init_leave (&video_meta_info, meta);
  }

  return video_meta_info;
}


GstVideoMultiviewMeta *
gst_buffer_add_video_multiview_meta (GstBuffer * buffer, guint n_views)
{
  GstVideoMultiviewMeta *meta;

  meta =
      (GstVideoMultiviewMeta *) gst_buffer_add_meta (buffer,
      GST_VIDEO_MULTIVIEW_META_INFO, NULL);

  if (!meta)
    return NULL;

  meta->view_info = g_new0 (GstVideoMultiviewViewInfo, n_views);
  meta->n_views = n_views;

  return meta;
}

void
gst_video_multiview_meta_set_n_views (GstVideoMultiviewMeta * mview_meta,
    guint n_views)
{
  guint i;

  mview_meta->view_info =
      g_renew (GstVideoMultiviewViewInfo, mview_meta->view_info, n_views);

  if (mview_meta->view_info == NULL) {
    if (n_views > 0)
      g_warning ("Failed to allocate GstVideoMultiview data");
    mview_meta->n_views = 0;
    return;
  }

  /* Make sure new entries are zero */
  for (i = mview_meta->n_views; i < n_views; i++) {
    GstVideoMultiviewViewInfo *info = mview_meta->view_info + i;

    info->meta_id = 0;
    info->view_label = GST_VIDEO_MULTIVIEW_VIEW_UNKNOWN;
  }
  mview_meta->n_views = n_views;
}

#endif
