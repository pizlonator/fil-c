/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <config.h>
#endif

#include "gsth265picture.h"
#include "gstcodecpicture-private.h"

GST_DEBUG_CATEGORY_EXTERN (gst_h265_decoder_debug);
#define GST_CAT_DEFAULT gst_h265_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstH265Picture, gst_h265_picture);

/**
 * gst_h265_picture_new:
 *
 * Create new #GstH265Picture
 *
 * Returns: a new #GstH265Picture
 */
GstH265Picture *
gst_h265_picture_new (void)
{
  GstH265Picture *pic;

  pic = g_new0 (GstH265Picture, 1);

  pic->pic_struct = GST_H265_SEI_PIC_STRUCT_FRAME;
  /* 0: interlaced, 1: progressive, 2: unspecified, 3: reserved, can be
   * interpreted as 2 */
  pic->source_scan_type = 2;
  pic->duplicate_flag = 0;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (pic), 0,
      GST_TYPE_H265_PICTURE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_codec_picture_free);

  return pic;
}

struct _GstH265Dpb
{
  GArray *pic_list;
  gint max_num_pics;
  gint num_output_needed;
};

/**
 * gst_h265_dpb_new: (skip)
 *
 * Create new #GstH265Dpb
 *
 * Returns: a new #GstH265Dpb
 */
GstH265Dpb *
gst_h265_dpb_new (void)
{
  GstH265Dpb *dpb;

  dpb = g_new0 (GstH265Dpb, 1);

  dpb->pic_list =
      g_array_sized_new (FALSE, TRUE, sizeof (GstH265Picture *),
      GST_H265_DPB_MAX_SIZE);
  g_array_set_clear_func (dpb->pic_list,
      (GDestroyNotify) gst_clear_h265_picture);

  return dpb;
}

/**
 * gst_h265_dpb_set_max_num_pics:
 * @dpb: a #GstH265Dpb
 * @max_num_pics: the maximum number of picture
 *
 * Set the number of maximum allowed pictures to store
 */
void
gst_h265_dpb_set_max_num_pics (GstH265Dpb * dpb, gint max_num_pics)
{
  g_return_if_fail (dpb != NULL);

  dpb->max_num_pics = max_num_pics;
}

/**
 * gst_h265_dpb_get_max_num_pics:
 * @dpb: a #GstH265Dpb
 *
 * Returns: the number of maximum pictures
 */
gint
gst_h265_dpb_get_max_num_pics (GstH265Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, 0);

  return dpb->max_num_pics;
}

/**
 * gst_h265_dpb_free:
 * @dpb: a #GstH265Dpb to free
 *
 * Free the @dpb
 */
void
gst_h265_dpb_free (GstH265Dpb * dpb)
{
  g_return_if_fail (dpb != NULL);

  gst_h265_dpb_clear (dpb);
  g_array_unref (dpb->pic_list);
  g_free (dpb);
}

/**
 * gst_h265_dpb_clear:
 * @dpb: a #GstH265Dpb
 *
 * Clear all stored #GstH265Picture
 */
void
gst_h265_dpb_clear (GstH265Dpb * dpb)
{
  g_return_if_fail (dpb != NULL);

  g_array_set_size (dpb->pic_list, 0);
  dpb->num_output_needed = 0;
}

/**
 * gst_h265_dpb_add:
 * @dpb: a #GstH265Dpb
 * @picture: (transfer full): a #GstH265Picture
 *
 * Store the @picture and perform increase pic_latency_cnt as defined in
 * "C.5.2.3 Additional bumping" process
 */
void
gst_h265_dpb_add (GstH265Dpb * dpb, GstH265Picture * picture)
{
  g_return_if_fail (dpb != NULL);
  g_return_if_fail (GST_IS_H265_PICTURE (picture));

  if (picture->output_flag) {
    gint i;

    for (i = 0; i < dpb->pic_list->len; i++) {
      GstH265Picture *other =
          g_array_index (dpb->pic_list, GstH265Picture *, i);

      if (other->needed_for_output)
        other->pic_latency_cnt++;
    }

    dpb->num_output_needed++;
    picture->needed_for_output = TRUE;
  } else {
    picture->needed_for_output = FALSE;
  }

  /* C.3.4 */
  picture->ref = TRUE;
  picture->long_term = FALSE;

  g_array_append_val (dpb->pic_list, picture);
}

/**
 * gst_h265_dpb_delete_unused:
 * @dpb: a #GstH265Dpb
 *
 * Delete not needed for output and not referenced all pictures from dpb
 */
void
gst_h265_dpb_delete_unused (GstH265Dpb * dpb)
{
  gint i;

  g_return_if_fail (dpb != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (!picture->needed_for_output && !picture->ref) {
      GST_TRACE ("remove picture %p (poc %d) from dpb",
          picture, picture->pic_order_cnt);
      g_array_remove_index (dpb->pic_list, i);
      i--;
    }
  }
}

/**
 * gst_h265_dpb_num_ref_pictures:
 * @dpb: a #GstH265Dpb
 *
 * Returns: The number of referenced pictures
 */
gint
gst_h265_dpb_num_ref_pictures (GstH265Dpb * dpb)
{
  gint i;
  gint ret = 0;

  g_return_val_if_fail (dpb != NULL, -1);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (picture->ref)
      ret++;
  }

  return ret;
}

/**
 * gst_h265_dpb_mark_all_non_ref:
 * @dpb: a #GstH265Dpb
 *
 * Mark all pictures are not referenced
 */
void
gst_h265_dpb_mark_all_non_ref (GstH265Dpb * dpb)
{
  gint i;

  g_return_if_fail (dpb != NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    picture->ref = FALSE;
  }
}

/**
 * gst_h265_dpb_get_ref_by_poc:
 * @dpb: a #GstH265Dpb
 * @poc: a picture order count
 *
 * Find a short or long term reference picture which has matching poc
 *
 * Returns: (nullable) (transfer full): a #GstH265Picture
 */
GstH265Picture *
gst_h265_dpb_get_ref_by_poc (GstH265Dpb * dpb, gint poc)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (picture->ref && picture->pic_order_cnt == poc)
      return gst_h265_picture_ref (picture);
  }

  GST_DEBUG ("No reference picture for poc %d", poc);

  return NULL;
}

/**
 * gst_h265_dpb_get_ref_by_poc_lsb:
 * @dpb: a #GstH265Dpb
 * @poc_lsb: a picture order count lsb
 *
 * Find a short or long term reference picture which has matching poc_lsb
 *
 * Returns: (nullable) (transfer full): a #GstH265Picture
 */
GstH265Picture *
gst_h265_dpb_get_ref_by_poc_lsb (GstH265Dpb * dpb, gint poc_lsb)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (picture->ref && picture->pic_order_cnt_lsb == poc_lsb)
      return gst_h265_picture_ref (picture);
  }

  GST_DEBUG ("No reference picture for poc lsb %d", poc_lsb);

  return NULL;
}

/**
 * gst_h265_dpb_get_short_ref_by_poc:
 * @dpb: a #GstH265Dpb
 * @poc: a picture order count
 *
 * Find a short term reference picture which has matching poc
 *
 * Returns: (nullable) (transfer full): a #GstH265Picture
 */
GstH265Picture *
gst_h265_dpb_get_short_ref_by_poc (GstH265Dpb * dpb, gint poc)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (picture->ref && !picture->long_term && picture->pic_order_cnt == poc)
      return gst_h265_picture_ref (picture);
  }

  GST_DEBUG ("No short term reference picture for poc %d", poc);

  return NULL;
}

/**
 * gst_h265_dpb_get_long_ref_by_poc:
 * @dpb: a #GstH265Dpb
 * @poc: a picture order count
 *
 * Find a long term reference picture which has matching poc
 *
 * Returns: (nullable) (transfer full): a #GstH265Picture
 */
GstH265Picture *
gst_h265_dpb_get_long_ref_by_poc (GstH265Dpb * dpb, gint poc)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (picture->ref && picture->long_term && picture->pic_order_cnt == poc)
      return gst_h265_picture_ref (picture);
  }

  GST_DEBUG ("No long term reference picture for poc %d", poc);

  return NULL;
}

/**
 * gst_h265_dpb_get_pictures_all:
 * @dpb: a #GstH265Dpb
 *
 * Return: (element-type GstH265Picture) (transfer full): a #GArray of
 *   #GstH265Picture stored in @dpb
 */
GArray *
gst_h265_dpb_get_pictures_all (GstH265Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, NULL);

  return g_array_ref (dpb->pic_list);
}

/**
 * gst_h265_dpb_get_size:
 * @dpb: a #GstH265Dpb
 *
 * Return: the length of stored dpb array
 */
gint
gst_h265_dpb_get_size (GstH265Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, -1);

  return dpb->pic_list->len;
}

/**
 * gst_h265_dpb_get_picture:
 * @dpb: a #GstH265Dpb
 * @system_frame_number The system frame number
 *
 * Returns: (transfer full) (nullable): the picture identified with the specified
 * @system_frame_number, or %NULL if DPB does not contain a #GstH265Picture
 * corresponding to the @system_frame_number
 *
 * Since: 1.20
 */
GstH265Picture *
gst_h265_dpb_get_picture (GstH265Dpb * dpb, guint32 system_frame_number)
{
  gint i;

  g_return_val_if_fail (dpb != NULL, NULL);

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (GST_CODEC_PICTURE_FRAME_NUMBER (picture) == system_frame_number) {
      gst_h265_picture_ref (picture);
      return picture;
    }
  }

  return NULL;
}

static gboolean
gst_h265_dpb_check_latency_count (GstH265Dpb * dpb, guint32 max_latency)
{
  gint i;

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (!picture->needed_for_output)
      continue;

    if (picture->pic_latency_cnt >= max_latency)
      return TRUE;
  }

  return FALSE;
}

/**
 * gst_h265_dpb_needs_bump:
 * @dpb: a #GstH265Dpb
 * @max_num_reorder_pics: sps_max_num_reorder_pics[HighestTid]
 * @max_latency_increase: SpsMaxLatencyPictures[HighestTid]
 * @max_dec_pic_buffering: sps_max_dec_pic_buffering_minus1[HighestTid ] + 1
 *   or zero if this shouldn't be used for bumping decision
 *
 * Returns: %TRUE if bumping is required
 *
 * Since: 1.20
 */
gboolean
gst_h265_dpb_needs_bump (GstH265Dpb * dpb, guint max_num_reorder_pics,
    guint max_latency_increase, guint max_dec_pic_buffering)
{
  g_return_val_if_fail (dpb != NULL, FALSE);
  g_assert (dpb->num_output_needed >= 0);

  /* If DPB is full and there is no empty space to store current picture,
   * need bumping.
   * NOTE: current picture was added already by our decoding flow, so we
   * need to do bumping until dpb->pic_list->len == dpb->max_num_pic
   */
  if (dpb->pic_list->len > dpb->max_num_pics) {
    GST_TRACE ("No empty frame buffer, need bumping");
    return TRUE;
  }

  /* C.5.2.3 */
  if (dpb->num_output_needed > max_num_reorder_pics) {
    GST_TRACE ("num_output_needed (%d) > max_num_reorder_pics (%d)",
        dpb->num_output_needed, max_num_reorder_pics);
    return TRUE;
  }

  if (dpb->num_output_needed && max_latency_increase &&
      gst_h265_dpb_check_latency_count (dpb, max_latency_increase)) {
    GST_TRACE ("has late picture, max_latency_increase: %d",
        max_latency_increase);
    return TRUE;
  }

  /* C.5.2.2 */
  if (max_dec_pic_buffering && dpb->pic_list->len >= max_dec_pic_buffering) {
    GST_TRACE ("dpb size (%d) >= max_dec_pic_buffering (%d)",
        dpb->pic_list->len, max_dec_pic_buffering);
    return TRUE;
  }

  return FALSE;
}

static gint
gst_h265_dpb_get_lowest_output_needed_picture (GstH265Dpb * dpb,
    GstH265Picture ** picture)
{
  gint i;
  GstH265Picture *lowest = NULL;
  gint index = -1;

  *picture = NULL;

  for (i = 0; i < dpb->pic_list->len; i++) {
    GstH265Picture *picture =
        g_array_index (dpb->pic_list, GstH265Picture *, i);

    if (!picture->needed_for_output)
      continue;

    if (!lowest) {
      lowest = picture;
      index = i;
      continue;
    }

    if (picture->pic_order_cnt < lowest->pic_order_cnt) {
      lowest = picture;
      index = i;
    }
  }

  if (lowest)
    *picture = gst_h265_picture_ref (lowest);

  return index;
}

/**
 * gst_h265_dpb_bump:
 * @dpb: a #GstH265Dpb
 * @drain: whether draining or not
 *
 * Perform bumping process as defined in C.5.2.4 "Bumping" process.
 * If @drain is %TRUE, @dpb will remove a #GstH265Picture from internal array
 * so that returned #GstH265Picture could hold the last reference of it
 *
 * Returns: (nullable) (transfer full): a #GstH265Picture which is needed to be
 * outputted
 *
 * Since: 1.20
 */
GstH265Picture *
gst_h265_dpb_bump (GstH265Dpb * dpb, gboolean drain)
{
  GstH265Picture *picture;
  gint index;

  g_return_val_if_fail (dpb != NULL, NULL);

  /* C.5.2.4 "Bumping" process */
  index = gst_h265_dpb_get_lowest_output_needed_picture (dpb, &picture);

  if (!picture || index < 0)
    return NULL;

  picture->needed_for_output = FALSE;

  dpb->num_output_needed--;
  g_assert (dpb->num_output_needed >= 0);

  if (!picture->ref || drain)
    g_array_remove_index_fast (dpb->pic_list, index);

  return picture;
}
