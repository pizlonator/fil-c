/* GStreamer
 * Copyright (C) 2020 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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

#include "gstmpeg2picture.h"
#include "gstcodecpicture-private.h"

GST_DEBUG_CATEGORY_EXTERN (gst_mpeg2_decoder_debug);
#define GST_CAT_DEFAULT gst_mpeg2_decoder_debug

GST_DEFINE_MINI_OBJECT_TYPE (GstMpeg2Picture, gst_mpeg2_picture);

static void
_gst_mpeg2_picture_free (GstMpeg2Picture * picture)
{
  GST_TRACE ("Free picture %p", picture);

  if (picture->first_field)
    gst_mpeg2_picture_unref (picture->first_field);

  gst_codec_picture_free (GST_CODEC_PICTURE (picture));
}

/**
 * gst_mpeg2_picture_new:
 *
 * Create new #GstMpeg2Picture
 *
 * Returns: a new #GstMpeg2Picture
 *
 * Since: 1.20
 */
GstMpeg2Picture *
gst_mpeg2_picture_new (void)
{
  GstMpeg2Picture *pic;

  pic = g_new0 (GstMpeg2Picture, 1);

  pic->pic_order_cnt = G_MAXINT32;
  pic->structure = GST_MPEG_VIDEO_PICTURE_STRUCTURE_FRAME;

  gst_mini_object_init (GST_MINI_OBJECT_CAST (pic), 0,
      GST_TYPE_MPEG2_PICTURE, NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_mpeg2_picture_free);

  GST_TRACE ("New picture %p", pic);

  return pic;
}

struct _GstMpeg2Dpb
{
  GstMpeg2Picture *ref_pic_list[2];
  guint num_ref_pictures;
  /* last added picture */
  GstMpeg2Picture *new_pic;
};

/**
 * gst_mpeg2_dpb_new: (skip)
 *
 * Create new #GstMpeg2Dpb
 *
 * Returns: a new #GstMpeg2Dpb
 *
 * Since: 1.20
 */
GstMpeg2Dpb *
gst_mpeg2_dpb_new (void)
{
  return g_new0 (GstMpeg2Dpb, 1);
}

/**
 * gst_mpeg2_dpb_free:
 * @dpb: a #GstMpeg2Dpb to free
 *
 * Free the @dpb
 *
 * Since: 1.20
 */
void
gst_mpeg2_dpb_free (GstMpeg2Dpb * dpb)
{
  guint i;

  g_return_if_fail (dpb != NULL);

  gst_clear_mpeg2_picture (&dpb->new_pic);

  g_assert (dpb->num_ref_pictures <= 2);
  for (i = 0; i < dpb->num_ref_pictures; i++)
    gst_clear_mpeg2_picture (&dpb->ref_pic_list[i]);

  g_free (dpb);
}

/**
 * gst_mpeg2_dpb_clear:
 * @dpb: a #GstMpeg2Dpb
 *
 * Clear all stored #GstMpeg2Picture
 *
 * Since: 1.20
 */
void
gst_mpeg2_dpb_clear (GstMpeg2Dpb * dpb)
{
  guint i;

  g_return_if_fail (dpb != NULL);

  gst_clear_mpeg2_picture (&dpb->new_pic);

  g_assert (dpb->num_ref_pictures <= 2);
  for (i = 0; i < dpb->num_ref_pictures; i++)
    gst_clear_mpeg2_picture (&dpb->ref_pic_list[i]);

  dpb->num_ref_pictures = 0;
}

static void
_dpb_add_to_reference (GstMpeg2Dpb * dpb, GstMpeg2Picture * pic)
{
  gint index = -1;

  if (G_LIKELY (dpb->num_ref_pictures == 2)) {
    index = (dpb->ref_pic_list[0]->pic_order_cnt >
        dpb->ref_pic_list[1]->pic_order_cnt);

    if (dpb->ref_pic_list[index]->pic_order_cnt > pic->pic_order_cnt)
      return;
  }

  if (index < 0) {
    index = dpb->num_ref_pictures;
    dpb->num_ref_pictures++;
  }

  gst_mpeg2_picture_replace (&dpb->ref_pic_list[index], pic);
}

/**
 * gst_mpeg2_dpb_add:
 * @dpb: a #GstMpeg2Dpb
 * @picture: (transfer full): a #GstMpeg2Picture
 *
 * Store the @picture
 *
 * Since: 1.20
 */
void
gst_mpeg2_dpb_add (GstMpeg2Dpb * dpb, GstMpeg2Picture * picture)
{
  g_return_if_fail (dpb != NULL);
  g_return_if_fail (GST_IS_MPEG2_PICTURE (picture));

  g_assert (dpb->num_ref_pictures <= 2);

  if (!GST_MPEG2_PICTURE_IS_REF (picture) || dpb->num_ref_pictures == 2) {
    gst_mpeg2_picture_replace (&dpb->new_pic, picture);
  } else {
    _dpb_add_to_reference (dpb, picture);
  }
}

/**
 * gst_mpeg2_dpb_need_bump:
 * @dpb: a #GstMpeg2Dpb
 *
 * Checks if @dbp has a new picture.
 *
 * Returns: #TRUE if @dpb needs to be bumped; otherwise, #FALSE
 *
 * Since: 1.20
 */
gboolean
gst_mpeg2_dpb_need_bump (GstMpeg2Dpb * dpb)
{
  g_return_val_if_fail (dpb != NULL, FALSE);
  g_assert (dpb->num_ref_pictures <= 2);

  if (dpb->new_pic)
    return TRUE;

  return FALSE;
}

/**
 * gst_mpeg2_dpb_bump:
 * @dpb: a #GstMpeg2Dpb
 *
 * Returns: (nullable) (transfer full): a #GstMpeg2Picture which is needed to be
 * outputted
 *
 * Since: 1.20
 */
GstMpeg2Picture *
gst_mpeg2_dpb_bump (GstMpeg2Dpb * dpb)
{
  GstMpeg2Picture *pic = NULL;
  guint i;

  g_return_val_if_fail (dpb != NULL, FALSE);
  g_assert (dpb->num_ref_pictures <= 2);

  /* First, find the lowest poc. */
  for (i = 0; i < 2; i++) {
    if (!dpb->ref_pic_list[i])
      continue;

    if (dpb->ref_pic_list[i]->needed_for_output) {
      if (!pic || pic->pic_order_cnt > dpb->ref_pic_list[i]->pic_order_cnt)
        gst_mpeg2_picture_replace (&pic, dpb->ref_pic_list[i]);
    }
  }

  if (dpb->new_pic && dpb->new_pic->needed_for_output &&
      (!pic || pic->pic_order_cnt > dpb->new_pic->pic_order_cnt))
    gst_mpeg2_picture_replace (&pic, dpb->new_pic);

  /* Then, replace the reference if needed. */
  if (dpb->new_pic && GST_MPEG2_PICTURE_IS_REF (dpb->new_pic)) {
    _dpb_add_to_reference (dpb, dpb->new_pic);
    gst_clear_mpeg2_picture (&dpb->new_pic);
  }

  if (pic) {
    pic->needed_for_output = FALSE;
    if (pic == dpb->new_pic)
      gst_clear_mpeg2_picture (&dpb->new_pic);
  }

  return pic;
}

/**
 * gst_mpeg2_dpb_get_neighbours:
 * @dpb: a #GstMpeg2Dpb
 * @picture: current #GstMpeg2Picture
 * @prev_picture_ptr: (transfer none) (out) (nullable): previuous
 *     #GstMpeg2Picture in @dpb
 * @next_picture_ptr: (transfer none) (out) (nullable): next
 *     #GstMpeg2Picture in @dpb
 *
 * Gets the neighbours #GstMpeg2Picture of @picture in @dpb.
 *
 * Since: 1.20
 */
void
gst_mpeg2_dpb_get_neighbours (GstMpeg2Dpb * dpb,
    GstMpeg2Picture * picture, GstMpeg2Picture ** prev_picture_ptr,
    GstMpeg2Picture ** next_picture_ptr)
{
  GstMpeg2Picture *ref_picture, *ref_pictures[2];
  GstMpeg2Picture **picture_ptr;
  guint i, index;

  g_return_if_fail (dpb != NULL);
  g_return_if_fail (picture != NULL);
  g_assert (dpb->num_ref_pictures <= 2);

  ref_pictures[0] = NULL;
  ref_pictures[1] = NULL;
  for (i = 0; i < 2; i++) {
    ref_picture = dpb->ref_pic_list[i];
    if (!ref_picture)
      continue;

    index = ref_picture->pic_order_cnt > picture->pic_order_cnt;
    picture_ptr = &ref_pictures[index];
    if (!*picture_ptr ||
        ((*picture_ptr)->pic_order_cnt > ref_picture->pic_order_cnt) == index)
      *picture_ptr = ref_picture;
  }

  if (prev_picture_ptr)
    *prev_picture_ptr = ref_pictures[0];
  if (next_picture_ptr)
    *next_picture_ptr = ref_pictures[1];
}
