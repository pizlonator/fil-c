/* GStreamer
 * Copyright (C) <2020> Jan Schmidt <jan@centricular.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>

#define GST_TYPE_DVB_SUB_ENC             (gst_dvb_sub_enc_get_type())
#define GST_DVB_SUB_ENC(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVB_SUB_ENC,GstDvbSubEnc))
#define GST_DVB_SUB_ENC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVB_SUB_ENC,GstDvbSubEncClass))
#define GST_IS_DVB_SUB_ENC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVB_SUB_ENC))
#define GST_IS_DVB_SUB_ENC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVB_SUB_ENC))

GST_DEBUG_CATEGORY_EXTERN (gst_dvb_sub_enc_debug);
#define GST_CAT_DEFAULT (gst_dvb_sub_enc_debug)

typedef struct _GstDvbSubEnc GstDvbSubEnc;
typedef struct _GstDvbSubEncClass GstDvbSubEncClass;
typedef struct SubpictureRect SubpictureRect;

struct SubpictureRect {
  /* Paletted 8-bit picture */
  GstVideoFrame *frame;
  /* Actual number of colours used from the palette */
  guint32 nb_colours;

  guint x, y;
};

struct _GstDvbSubEnc
{
  GstElement element;

  GstVideoInfo in_info;
  int display_version;
  GstPad *sinkpad;
  GstPad *srcpad;

  int object_version;

  int max_colours;
  GstClockTimeDiff ts_offset;

  GstClockTime current_end_time;
};

struct _GstDvbSubEncClass
{
  GstElementClass parent_class;
};

GType gst_dvb_sub_enc_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (dvbsubenc);

gboolean gst_dvbsubenc_ayuv_to_ayuv8p (GstVideoFrame * src, GstVideoFrame * dest, int max_colours, guint32 *out_num_colours);

GstBuffer *gst_dvbenc_encode (int object_version, int page_id, int display_version, guint16 width, guint16 height, SubpictureRect *s, guint num_subpictures);
