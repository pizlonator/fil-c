/*
 * vp8parser-test.c - Print IVF/VP8 headers
 *
 * Copyright (C) 2013-2014 Intel Corporation
 *   Author: Halley Zhao <halley.zhao@intel.com>
 *   Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/codecparsers/gstvp8parser.h>

#define FOURCC_VP80             GST_MAKE_FOURCC('V','P','8','0')
#define IVF_FILE_HDR_SIZE       32
#define IVF_FRAME_HDR_SIZE      12

/* Maximum VP8 Frame Header size in bits */
#define VP8_FRAME_HDR_SIZE      10127

typedef struct _IVFFileHdr IVFFileHdr;
struct _IVFFileHdr
{
  guint16 version;
  guint16 length;
  guint32 fourcc;
  guint16 width;
  guint16 height;
  guint32 framerate;
  guint32 time_scale;
  guint32 num_frames;
};

typedef struct _IVFFrameHdr IVFFrameHdr;
struct _IVFFrameHdr
{
  guint32 frame_size;
  guint64 timestamp;
};

static gboolean
parse_ivf_file_header (IVFFileHdr * ivf_hdr, guint8 * data, guint size)
{
  GstByteReader br;

  if (size < IVF_FILE_HDR_SIZE) {
    g_warning ("size is smaller than IVF file header");
    goto error;
  }

  g_assert (data[0] == 'D' && data[1] == 'K' && data[2] == 'I'
      && data[3] == 'F');

  gst_byte_reader_init (&br, data, size);
  gst_byte_reader_skip (&br, 4);

  if (!gst_byte_reader_get_uint16_le (&br, &ivf_hdr->version))
    goto error;
  g_assert (ivf_hdr->version == 0);

  if (!gst_byte_reader_get_uint16_le (&br, &ivf_hdr->length))
    goto error;
  g_assert (ivf_hdr->length == 0x20);

  if (!gst_byte_reader_get_uint32_le (&br, &ivf_hdr->fourcc))
    goto error;
  g_assert (ivf_hdr->fourcc == FOURCC_VP80);

  if (!gst_byte_reader_get_uint16_le (&br, &ivf_hdr->width))
    goto error;
  if (!gst_byte_reader_get_uint16_le (&br, &ivf_hdr->height))
    goto error;
  if (!gst_byte_reader_get_uint32_le (&br, &ivf_hdr->framerate))
    goto error;
  if (!gst_byte_reader_get_uint32_le (&br, &ivf_hdr->time_scale))
    goto error;
  if (!gst_byte_reader_get_uint32_le (&br, &ivf_hdr->num_frames))
    goto error;

  g_print ("IVF File Information:\n");
  g_print ("  %-32s : %u\n", "version", ivf_hdr->version);
  g_print ("  %-32s : %u\n", "length", ivf_hdr->length);
  g_print ("  %-32s : '%" GST_FOURCC_FORMAT "'\n", "fourcc",
      GST_FOURCC_ARGS (ivf_hdr->fourcc));
  g_print ("  %-32s : %u\n", "width", ivf_hdr->width);
  g_print ("  %-32s : %u\n", "height", ivf_hdr->height);
  g_print ("  %-32s : %u\n", "framerate", ivf_hdr->framerate);
  g_print ("  %-32s : %u\n", "time_scale", ivf_hdr->time_scale);
  g_print ("  %-32s : %u\n", "num_frames", ivf_hdr->num_frames);
  g_print ("\n");

  return TRUE;

error:
  return FALSE;
}

static gboolean
parse_ivf_frame_header (IVFFrameHdr * frm_hdr, guint8 * data, guint size)
{
  GstByteReader br;

  if (size < IVF_FRAME_HDR_SIZE) {
    g_warning ("size is smaller than IVF frame header");
    goto error;
  }

  gst_byte_reader_init (&br, data, size);
  if (!gst_byte_reader_get_uint32_le (&br, &frm_hdr->frame_size))
    goto error;
  if (!gst_byte_reader_get_uint64_le (&br, &frm_hdr->timestamp))
    goto error;

  g_print ("IVF Frame Information:\n");
  g_print ("  %-32s : %u\n", "size", frm_hdr->frame_size);
  g_print ("  %-32s : %" G_GINT64_FORMAT " \n", "timestamp",
      frm_hdr->timestamp);
  g_print ("\n");

  return TRUE;

error:
  return FALSE;
}

static void
print_segmentation (GstVp8Segmentation * seg)
{
  gint i;

  g_print ("+ Segmentation:\n");
  g_print ("  %-32s : %d\n", "segmentation_enabled", seg->segmentation_enabled);
  g_print ("  %-32s : %d\n", "update_mb_segmentation_map",
      seg->update_mb_segmentation_map);
  g_print ("  %-32s : %d\n", "update_segment_feature_data",
      seg->update_segment_feature_data);

  if (seg->update_segment_feature_data) {
    g_print ("  %-32s : %d\n", "segment_feature_mode",
        seg->segment_feature_mode);
    g_print ("  %-32s : %d", "quantizer_update_value",
        seg->quantizer_update_value[0]);
    for (i = 1; i < 4; i++) {
      g_print (", %d", seg->quantizer_update_value[i]);
    }
    g_print ("\n");
    g_print ("  %-32s : %d", "lf_update_value", seg->lf_update_value[0]);
    for (i = 1; i < 4; i++) {
      g_print (", %d", seg->lf_update_value[i]);
    }
    g_print ("\n");
  }

  if (seg->update_mb_segmentation_map) {
    g_print ("  %-32s : %d", "segment_prob", seg->segment_prob[0]);
    for (i = 1; i < 3; i++) {
      g_print (", %d", seg->segment_prob[i]);
    }
    g_print ("\n");
  }
}

static void
print_mb_lf_adjustments (GstVp8MbLfAdjustments * adj)
{
  gint i;

  g_print ("+ MB Loop-Filter Adjustments:\n");
  g_print ("  %-32s : %d\n", "loop_filter_adj_enable",
      adj->loop_filter_adj_enable);
  if (adj->loop_filter_adj_enable) {
    g_print ("  %-32s : %d\n", "mode_ref_lf_delta_update",
        adj->mode_ref_lf_delta_update);
    if (adj->mode_ref_lf_delta_update) {
      g_print ("  %-32s : %d", "ref_frame_delta", adj->ref_frame_delta[0]);
      for (i = 1; i < 4; i++) {
        g_print (", %d", adj->ref_frame_delta[i]);
      }
      g_print ("\n");
      g_print ("  %-32s : %d", "mb_mode_delta", adj->mb_mode_delta[0]);
      for (i = 1; i < 4; i++) {
        g_print (", %d", adj->mb_mode_delta[i]);
      }
      g_print ("\n");
    }
  }
}

static void
print_quant_indices (GstVp8QuantIndices * qip)
{
  g_print ("+ Dequantization Indices:\n");
  g_print ("  %-32s : %d\n", "y_ac_qi", qip->y_ac_qi);
  g_print ("  %-32s : %d\n", "y_dc_delta", qip->y_dc_delta);
  g_print ("  %-32s : %d\n", "y2_dc_delta", qip->y2_dc_delta);
  g_print ("  %-32s : %d\n", "y2_ac_delta", qip->y2_ac_delta);
  g_print ("  %-32s : %d\n", "uv_dc_delta", qip->uv_dc_delta);
  g_print ("  %-32s : %d\n", "uv_ac_delta", qip->uv_ac_delta);
}

static void
print_mv_probs (GstVp8MvProbs * probs)
{
  gint i, j;

  g_print ("+ MV Probabilities:\n");
  for (j = 0; j < 2; j++) {
    g_print ("  %-32s : %d", j == 0 ? "row" : "column", probs->prob[j][0]);
    for (i = 1; i < 19; i++) {
      g_print (", %d", probs->prob[j][i]);
    }
    g_print ("\n");
  }
}

static void
print_mode_probs (GstVp8ModeProbs * probs)
{
  gint i;

  g_print ("+ Intra-mode Probabilities:\n");
  g_print ("  %-32s : %d", "luma", probs->y_prob[0]);
  for (i = 1; i < 4; i++) {
    g_print (", %d", probs->y_prob[i]);
  }
  g_print ("\n");
  g_print ("  %-32s : %d", "chroma", probs->uv_prob[0]);
  for (i = 1; i < 3; i++) {
    g_print (", %d", probs->uv_prob[i]);
  }
  g_print ("\n");
}

static void
print_frame_header (GstVp8FrameHdr * frame_hdr)
{
  g_print ("  %-32s : %d\n", "key_frame", frame_hdr->key_frame);
  g_print ("  %-32s : %d\n", "version", frame_hdr->version);
  g_print ("  %-32s : %d\n", "show_frame", frame_hdr->show_frame);
  g_print ("  %-32s : %d\n", "first_part_size", frame_hdr->first_part_size);
  if (frame_hdr->key_frame) {
    g_print ("  %-32s : %d\n", "width", frame_hdr->width);
    g_print ("  %-32s : %d\n", "height", frame_hdr->height);
    g_print ("  %-32s : %d\n", "horizontal_scale", frame_hdr->horiz_scale_code);
    g_print ("  %-32s : %d\n", "vertical_scale", frame_hdr->vert_scale_code);
  }

  if (frame_hdr->key_frame) {
    g_print ("  %-32s : %d\n", "color_space", frame_hdr->color_space);
    g_print ("  %-32s : %d\n", "clamping_type", frame_hdr->clamping_type);
  }

  g_print ("  %-32s : %d\n", "filter_type", frame_hdr->filter_type);
  g_print ("  %-32s : %d\n", "loop_filter_level", frame_hdr->loop_filter_level);
  g_print ("  %-32s : %d\n", "sharpness_level", frame_hdr->sharpness_level);

  g_print ("  %-32s : %d\n", "log2_nbr_of_dct_partitions",
      frame_hdr->log2_nbr_of_dct_partitions);

  if (frame_hdr->key_frame) {
    g_print ("  %-32s : %d\n", "refresh_entropy_probs",
        frame_hdr->refresh_entropy_probs);
  } else {
    g_print ("  %-32s : %d\n", "refresh_golden_frame",
        frame_hdr->refresh_golden_frame);
    g_print ("  %-32s : %d\n", "refresh_alternate_frame",
        frame_hdr->refresh_alternate_frame);
    if (!frame_hdr->refresh_golden_frame) {
      g_print ("  %-32s : %d\n", "copy_buffer_to_golden",
          frame_hdr->copy_buffer_to_golden);
    }
    if (!frame_hdr->refresh_alternate_frame) {
      g_print ("  %-32s : %d\n", "copy_buffer_to_alternate",
          frame_hdr->copy_buffer_to_alternate);
    }
    g_print ("  %-32s : %d\n", "sign_bias_golden", frame_hdr->sign_bias_golden);
    g_print ("  %-32s : %d\n", "sign_bias_alternate",
        frame_hdr->sign_bias_alternate);
    g_print ("  %-32s : %d\n", "refresh_entropy_probs",
        frame_hdr->refresh_entropy_probs);
    g_print ("  %-32s : %d\n", "refresh_last", frame_hdr->refresh_last);
  }

  g_print ("  %-32s : %d\n", "mb_no_skip_coeff", frame_hdr->mb_no_skip_coeff);
  if (frame_hdr->mb_no_skip_coeff) {
    g_print ("  %-32s : %d\n", "prob_skip_false", frame_hdr->prob_skip_false);
  }

  if (!frame_hdr->key_frame) {
    g_print ("  %-32s : %d\n", "prob_intra", frame_hdr->prob_intra);
    g_print ("  %-32s : %d\n", "prob_last", frame_hdr->prob_last);
    g_print ("  %-32s : %d\n", "prob_gf", frame_hdr->prob_gf);
  }

  print_quant_indices (&frame_hdr->quant_indices);
  print_mv_probs (&frame_hdr->mv_probs);
  print_mode_probs (&frame_hdr->mode_probs);
}

gint
main (int argc, char **argv)
{
  FILE *fp = NULL;
  guint8 buf[(VP8_FRAME_HDR_SIZE + 7) / 8];
  IVFFileHdr ivf_file_hdr;
  IVFFrameHdr ivf_frame_hdr;
  GstVp8Parser parser;
  GstVp8FrameHdr frame_hdr;
  guint hdr_size, frame_num = 0;

  g_assert (sizeof (buf) >= IVF_FILE_HDR_SIZE);
  g_assert (sizeof (buf) >= IVF_FRAME_HDR_SIZE);

  if (argc < 2) {
    g_printerr ("Usage: %s <IVF file>\n", argv[0]);
    return 1;
  }

  fp = fopen (argv[1], "r");
  if (!fp) {
    g_printerr ("failed to open IVF file (%s)\n", argv[1]);
    goto error;
  }

  if (fread (buf, IVF_FILE_HDR_SIZE, 1, fp) != 1) {
    g_printerr ("failed to read IVF header\n");
    goto error;
  }

  if (!parse_ivf_file_header (&ivf_file_hdr, buf, IVF_FILE_HDR_SIZE)) {
    g_printerr ("failed to parse IVF header\n");
    goto error;
  }

  gst_vp8_parser_init (&parser);
  while (fread (buf, IVF_FRAME_HDR_SIZE, 1, fp) == 1) {
    if (!parse_ivf_frame_header (&ivf_frame_hdr, buf, IVF_FRAME_HDR_SIZE)) {
      g_printerr ("fail to parse IVF frame header\n");
      goto error;
    }

    g_print ("Frame #%d @ offset %lu\n", frame_num, (gulong) ftell (fp));
    hdr_size = MIN (sizeof (buf), ivf_frame_hdr.frame_size);
    if (fread (buf, hdr_size, 1, fp) != 1) {
      g_printerr ("failed to read VP8 frame header\n");
      goto error;
    }

    hdr_size = ivf_frame_hdr.frame_size - hdr_size;
    if (hdr_size > 0 && fseek (fp, hdr_size, SEEK_CUR) != 0) {
      g_printerr ("failed to skip frame data (%u bytes): %s\n",
          ivf_frame_hdr.frame_size, strerror (errno));
      goto error;
    }

    memset (&frame_hdr, 0, sizeof (frame_hdr));
    if (gst_vp8_parser_parse_frame_header (&parser, &frame_hdr, buf,
            ivf_frame_hdr.frame_size) != GST_VP8_PARSER_OK) {
      g_printerr ("failed to parse frame header\n");
      goto error;
    }

    print_frame_header (&frame_hdr);
    print_segmentation (&parser.segmentation);
    print_mb_lf_adjustments (&parser.mb_lf_adjust);
    g_print ("\n");

    frame_num++;
  }

  fclose (fp);
  return 0;

error:
  if (fp)
    fclose (fp);
  return 1;
}
