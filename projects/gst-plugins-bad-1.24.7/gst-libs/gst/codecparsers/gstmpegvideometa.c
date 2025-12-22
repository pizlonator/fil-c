/*
 * GStreamer
 * Copyright (C) 2012 Edward Hervey <edward@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmpegvideometa.h"

GST_DEBUG_CATEGORY_STATIC (mpegv_meta_debug);
#define GST_CAT_DEFAULT mpegv_meta_debug

static gboolean
gst_mpeg_video_meta_init (GstMpegVideoMeta * mpeg_video_meta,
    gpointer params, GstBuffer * buffer)
{
  mpeg_video_meta->sequencehdr = NULL;
  mpeg_video_meta->sequenceext = NULL;
  mpeg_video_meta->sequencedispext = NULL;
  mpeg_video_meta->pichdr = NULL;
  mpeg_video_meta->picext = NULL;
  mpeg_video_meta->quantext = NULL;
  mpeg_video_meta->num_slices = mpeg_video_meta->slice_offset = 0;

  return TRUE;
}

static void
gst_mpeg_video_meta_free (GstMpegVideoMeta * mpeg_video_meta,
    GstBuffer * buffer)
{
  if (mpeg_video_meta->sequencehdr)
    g_free (mpeg_video_meta->sequencehdr);
  if (mpeg_video_meta->sequenceext)
    g_free (mpeg_video_meta->sequenceext);
  if (mpeg_video_meta->sequencedispext)
    g_free (mpeg_video_meta->sequencedispext);
  if (mpeg_video_meta->pichdr)
    g_free (mpeg_video_meta->pichdr);
  if (mpeg_video_meta->picext)
    g_free (mpeg_video_meta->picext);
  if (mpeg_video_meta->quantext)
    g_free (mpeg_video_meta->quantext);
}

static gboolean
gst_mpeg_video_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstMpegVideoMeta *smeta, *dmeta;

  smeta = (GstMpegVideoMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta =
          gst_buffer_add_mpeg_video_meta (dest, smeta->sequencehdr,
          smeta->sequenceext, smeta->sequencedispext, smeta->pichdr,
          smeta->picext, smeta->quantext);

      if (!dmeta)
        return FALSE;

      dmeta->num_slices = smeta->num_slices;
      dmeta->slice_offset = smeta->slice_offset;
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

GType
gst_mpeg_video_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "memory", NULL };      /* don't know what to set here */

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstMpegVideoMetaAPI", tags);
    GST_DEBUG_CATEGORY_INIT (mpegv_meta_debug, "mpegvideometa", 0,
        "MPEG-1/2 video GstMeta");

    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_mpeg_video_meta_get_info (void)
{
  static const GstMetaInfo *mpeg_video_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & mpeg_video_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_MPEG_VIDEO_META_API_TYPE,
        "GstMpegVideoMeta", sizeof (GstMpegVideoMeta),
        (GstMetaInitFunction) gst_mpeg_video_meta_init,
        (GstMetaFreeFunction) gst_mpeg_video_meta_free,
        (GstMetaTransformFunction) gst_mpeg_video_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & mpeg_video_meta_info,
        (GstMetaInfo *) meta);
  }

  return mpeg_video_meta_info;
}

/**
 * gst_buffer_add_mpeg_video_meta:
 * @buffer: a #GstBuffer
 *
 * Creates and adds a #GstMpegVideoMeta to a @buffer.
 *
 * Provided structures must either be %NULL or GSlice-allocated.
 *
 * Returns: (transfer full): a newly created #GstMpegVideoMeta
 *
 * Since: 1.2
 */
GstMpegVideoMeta *
gst_buffer_add_mpeg_video_meta (GstBuffer * buffer,
    const GstMpegVideoSequenceHdr * seq_hdr,
    const GstMpegVideoSequenceExt * seq_ext,
    const GstMpegVideoSequenceDisplayExt * disp_ext,
    const GstMpegVideoPictureHdr * pic_hdr,
    const GstMpegVideoPictureExt * pic_ext,
    const GstMpegVideoQuantMatrixExt * quant_ext)
{
  GstMpegVideoMeta *mpeg_video_meta;

  mpeg_video_meta =
      (GstMpegVideoMeta *) gst_buffer_add_meta (buffer,
      GST_MPEG_VIDEO_META_INFO, NULL);

  GST_DEBUG
      ("seq_hdr:%p, seq_ext:%p, disp_ext:%p, pic_hdr:%p, pic_ext:%p, quant_ext:%p",
      seq_hdr, seq_ext, disp_ext, pic_hdr, pic_ext, quant_ext);

  if (seq_hdr)
    mpeg_video_meta->sequencehdr =
        g_memdup2 (seq_hdr, sizeof (GstMpegVideoSequenceHdr));
  if (seq_ext)
    mpeg_video_meta->sequenceext =
        g_memdup2 (seq_ext, sizeof (GstMpegVideoSequenceExt));
  if (disp_ext)
    mpeg_video_meta->sequencedispext =
        g_memdup2 (disp_ext, sizeof (GstMpegVideoSequenceDisplayExt));
  mpeg_video_meta->pichdr =
      g_memdup2 (pic_hdr, sizeof (GstMpegVideoPictureHdr));
  if (pic_ext)
    mpeg_video_meta->picext =
        g_memdup2 (pic_ext, sizeof (GstMpegVideoPictureExt));
  if (quant_ext)
    mpeg_video_meta->quantext =
        g_memdup2 (quant_ext, sizeof (GstMpegVideoQuantMatrixExt));

  return mpeg_video_meta;
}
