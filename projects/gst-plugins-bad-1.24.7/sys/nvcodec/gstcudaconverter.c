/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstcudaconverter.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_cuda_converter_debug);
#define GST_CAT_DEFAULT gst_cuda_converter_debug

#define CUDA_BLOCK_X 16
#define CUDA_BLOCK_Y 16
#define DIV_UP(size,block) (((size) + ((block) - 1)) / (block))

/* from GstD3D11 */
typedef struct _GstCudaColorMatrix
{
  gdouble matrix[3][3];
  gdouble offset[3];
  gdouble min[3];
  gdouble max[3];
} GstCudaColorMatrix;

static gchar *
gst_cuda_dump_color_matrix (GstCudaColorMatrix * matrix)
{
  /* *INDENT-OFF* */
  static const gchar format[] =
      "[MATRIX]\n"
      "|% .6f, % .6f, % .6f|\n"
      "|% .6f, % .6f, % .6f|\n"
      "|% .6f, % .6f, % .6f|\n"
      "[OFFSET]\n"
      "|% .6f, % .6f, % .6f|\n"
      "[MIN]\n"
      "|% .6f, % .6f, % .6f|\n"
      "[MAX]\n"
      "|% .6f, % .6f, % .6f|";
  /* *INDENT-ON* */

  return g_strdup_printf (format,
      matrix->matrix[0][0], matrix->matrix[0][1], matrix->matrix[0][2],
      matrix->matrix[1][0], matrix->matrix[1][1], matrix->matrix[1][2],
      matrix->matrix[2][0], matrix->matrix[2][1], matrix->matrix[2][2],
      matrix->offset[0], matrix->offset[1], matrix->offset[2],
      matrix->min[0], matrix->min[1], matrix->min[2],
      matrix->max[0], matrix->max[1], matrix->max[2]);
}

static void
color_matrix_copy (GstCudaColorMatrix * dst, const GstCudaColorMatrix * src)
{
  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      dst->matrix[i][j] = src->matrix[i][j];
    }
  }
}

static void
color_matrix_multiply (GstCudaColorMatrix * dst, GstCudaColorMatrix * a,
    GstCudaColorMatrix * b)
{
  GstCudaColorMatrix tmp;

  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      gdouble val = 0;
      for (guint k = 0; k < 3; k++) {
        val += a->matrix[i][k] * b->matrix[k][j];
      }

      tmp.matrix[i][j] = val;
    }
  }

  color_matrix_copy (dst, &tmp);
}

static void
color_matrix_identity (GstCudaColorMatrix * m)
{
  for (guint i = 0; i < 3; i++) {
    for (guint j = 0; j < 3; j++) {
      if (i == j)
        m->matrix[i][j] = 1.0;
      else
        m->matrix[i][j] = 0;
    }
  }
}

/**
 * gst_cuda_color_range_adjust_matrix_unorm:
 * @in_info: a #GstVideoInfo
 * @out_info: a #GstVideoInfo
 * @matrix: a #GstCudaColorMatrix
 *
 * Calculates matrix for color range adjustment. Both input and output
 * signals are in normalized [0.0..1.0] space.
 *
 * Resulting values can be calculated by
 * | Yout |                           | Yin |   | matrix.offset[0] |
 * | Uout | = clamp ( matrix.matrix * | Uin | + | matrix.offset[1] |, matrix.min, matrix.max )
 * | Vout |                           | Vin |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
static gboolean
gst_cuda_color_range_adjust_matrix_unorm (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstCudaColorMatrix * matrix)
{
  gboolean in_rgb, out_rgb;
  gint in_offset[GST_VIDEO_MAX_COMPONENTS];
  gint in_scale[GST_VIDEO_MAX_COMPONENTS];
  gint out_offset[GST_VIDEO_MAX_COMPONENTS];
  gint out_scale[GST_VIDEO_MAX_COMPONENTS];
  GstVideoColorRange in_range;
  GstVideoColorRange out_range;
  gdouble src_fullscale, dst_fullscale;

  memset (matrix, 0, sizeof (GstCudaColorMatrix));
  for (guint i = 0; i < 3; i++) {
    matrix->matrix[i][i] = 1.0;
    matrix->matrix[i][i] = 1.0;
    matrix->matrix[i][i] = 1.0;
    matrix->max[i] = 1.0;
  }

  in_rgb = GST_VIDEO_INFO_IS_RGB (in_info);
  out_rgb = GST_VIDEO_INFO_IS_RGB (out_info);

  if (in_rgb != out_rgb) {
    GST_WARNING ("Invalid format conversion");
    return FALSE;
  }

  in_range = in_info->colorimetry.range;
  out_range = out_info->colorimetry.range;

  if (in_range == GST_VIDEO_COLOR_RANGE_UNKNOWN) {
    GST_WARNING ("Unknown input color range");
    if (in_rgb || GST_VIDEO_INFO_IS_GRAY (in_info))
      in_range = GST_VIDEO_COLOR_RANGE_0_255;
    else
      in_range = GST_VIDEO_COLOR_RANGE_16_235;
  }

  if (out_range == GST_VIDEO_COLOR_RANGE_UNKNOWN) {
    GST_WARNING ("Unknown output color range");
    if (out_rgb || GST_VIDEO_INFO_IS_GRAY (out_info))
      out_range = GST_VIDEO_COLOR_RANGE_0_255;
    else
      out_range = GST_VIDEO_COLOR_RANGE_16_235;
  }

  src_fullscale = (gdouble) ((1 << in_info->finfo->depth[0]) - 1);
  dst_fullscale = (gdouble) ((1 << out_info->finfo->depth[0]) - 1);

  gst_video_color_range_offsets (in_range, in_info->finfo, in_offset, in_scale);
  gst_video_color_range_offsets (out_range,
      out_info->finfo, out_offset, out_scale);

  matrix->min[0] = matrix->min[1] = matrix->min[2] =
      (gdouble) out_offset[0] / dst_fullscale;

  matrix->max[0] = (out_scale[0] + out_offset[0]) / dst_fullscale;
  matrix->max[1] = matrix->max[2] =
      (out_scale[1] + out_offset[0]) / dst_fullscale;

  if (in_info->colorimetry.range == out_info->colorimetry.range) {
    GST_DEBUG ("Same color range");
    return TRUE;
  }

  /* Formula
   *
   * 1) Scales and offset compensates input to [0..1] range
   * SRC_NORM[i] = (src[i] * src_fullscale - in_offset[i]) / in_scale[i]
   *             = (src[i] * src_fullscale / in_scale[i]) - in_offset[i] / in_scale[i]
   *
   * 2) Reverse to output UNIT scale
   * DST_UINT[i] = SRC_NORM[i] * out_scale[i] + out_offset[i]
   *             = src[i] * src_fullscale * out_scale[i] / in_scale[i]
   *               - in_offset[i] * out_scale[i] / in_scale[i]
   *               + out_offset[i]
   *
   * 3) Back to [0..1] scale
   * dst[i] = DST_UINT[i] / dst_fullscale
   *        = COEFF[i] * src[i] + OFF[i]
   * where
   *             src_fullscale * out_scale[i]
   * COEFF[i] = ------------------------------
   *             dst_fullscale * in_scale[i]
   *
   *            out_offset[i]     in_offset[i] * out_scale[i]
   * OFF[i] =  -------------- -  ------------------------------
   *            dst_fullscale     dst_fullscale * in_scale[i]
   */
  for (guint i = 0; i < 3; i++) {
    matrix->matrix[i][i] = (src_fullscale * out_scale[i]) /
        (dst_fullscale * in_scale[i]);
    matrix->offset[i] = (out_offset[i] / dst_fullscale) -
        ((gdouble) in_offset[i] * out_scale[i] / (dst_fullscale * in_scale[i]));
  }

  return TRUE;
}

/**
 * gst_cuda_yuv_to_rgb_matrix_unorm:
 * @in_yuv_info: a #GstVideoInfo of input YUV signal
 * @out_rgb_info: a #GstVideoInfo of output RGB signal
 * @matrix: a #GstCudaColorMatrix
 *
 * Calculates transform matrix from YUV to RGB conversion. Both input and output
 * signals are in normalized [0.0..1.0] space and additional gamma decoding
 * or primary/transfer function transform is not performed by this matrix.
 *
 * Resulting non-linear RGB values can be calculated by
 * | R' |                           | Y' |   | matrix.offset[0] |
 * | G' | = clamp ( matrix.matrix * | Cb | + | matrix.offset[1] | matrix.min, matrix.max )
 * | B' |                           | Cr |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
static gboolean
gst_cuda_yuv_to_rgb_matrix_unorm (const GstVideoInfo * in_yuv_info,
    const GstVideoInfo * out_rgb_info, GstCudaColorMatrix * matrix)
{
  gint offset[4], scale[4];
  gdouble Kr, Kb, Kg;

  /*
   * <Formula>
   *
   * Input: Unsigned normalized Y'CbCr(unorm), [0.0..1.0] range
   * Output: Unsigned normalized non-linear R'G'B'(unorm), [0.0..1.0] range
   *
   * 1) Y'CbCr(unorm) to scaled Y'CbCr
   * | Y' |     | Y'(unorm) |
   * | Cb | = S | Cb(unorm) |
   * | Cb |     | Cr(unorm) |
   * where S = (2 ^ bitdepth) - 1
   *
   * 2) Y'CbCr to YPbPr
   * Y  = (Y' - offsetY )    / scaleY
   * Pb = [(Cb - offsetCbCr) / scaleCbCr]
   * Pr = [(Cr - offsetCrCr) / scaleCrCr]
   * =>
   * Y  = Y'(unorm) * Sy  + Oy
   * Pb = Cb(unorm) * Suv + Ouv
   * Pb = Cr(unorm) * Suv + Ouv
   * where
   * Sy  = S / scaleY
   * Suv = S / scaleCbCr
   * Oy  = -(offsetY / scaleY)
   * Ouv = -(offsetCbCr / scaleCbCr)
   *
   * 3) YPbPr to R'G'B'
   * | R' |      | Y  |
   * | G' | = M *| Pb |
   * | B' |      | Pr |
   * where
   *     | vecR |
   * M = | vecG |
   *     | vecB |
   * vecR = | 1,         0           ,       2(1 - Kr)      |
   * vecG = | 1, -(Kb/Kg) * 2(1 - Kb), -(Kr/Kg) * 2(1 - Kr) |
   * vecB = | 1,       2(1 - Kb)     ,          0           |
   * =>
   * R' = dot(vecR, (Syuv * Y'CbCr(unorm))) + dot(vecR, Offset)
   * G' = dot(vecG, (Svuy * Y'CbCr(unorm))) + dot(vecG, Offset)
   * B' = dot(vecB, (Syuv * Y'CbCr(unorm)) + dot(vecB, Offset)
   * where
   *        | Sy,   0,   0 |
   * Syuv = |  0, Suv,   0 |
   *        |  0    0, Suv |
   *
   *          | Oy  |
   * Offset = | Ouv |
   *          | Ouv |
   *
   * 4) YUV -> RGB matrix
   * | R' |            | Y'(unorm) |   | offsetA |
   * | G' | = Matrix * | Cb(unorm) | + | offsetB |
   * | B' |            | Cr(unorm) |   | offsetC |
   *
   * where
   *          | vecR |
   * Matrix = | vecG | * Syuv
   *          | vecB |
   *
   * offsetA = dot(vecR, Offset)
   * offsetB = dot(vecG, Offset)
   * offsetC = dot(vecB, Offset)
   *
   * 4) Consider 16-235 scale RGB
   * RGBfull(0..255) -> RGBfull(16..235) matrix is represented by
   * | Rs |      | Rf |   | Or |
   * | Gs | = Ms | Gf | + | Og |
   * | Bs |      | Bf |   | Ob |
   *
   * Combining all matrix into
   * | Rs |                   | Y'(unorm) |   | offsetA |     | Or |
   * | Gs | = Ms * ( Matrix * | Cb(unorm) | + | offsetB | ) + | Og |
   * | Bs |                   | Cr(unorm) |   | offsetC |     | Ob |
   *
   *                        | Y'(unorm) |      | offsetA |   | Or |
   *        = Ms * Matrix * | Cb(unorm) | + Ms | offsetB | + | Og |
   *                        | Cr(unorm) |      | offsetC |   | Ob |
   */

  memset (matrix, 0, sizeof (GstCudaColorMatrix));
  for (guint i = 0; i < 3; i++)
    matrix->max[i] = 1.0;

  gst_video_color_range_offsets (in_yuv_info->colorimetry.range,
      in_yuv_info->finfo, offset, scale);

  if (gst_video_color_matrix_get_Kr_Kb (in_yuv_info->colorimetry.matrix,
          &Kr, &Kb)) {
    guint S;
    gdouble Sy, Suv;
    gdouble Oy, Ouv;
    gdouble vecR[3], vecG[3], vecB[3];

    Kg = 1.0 - Kr - Kb;

    vecR[0] = 1.0;
    vecR[1] = 0;
    vecR[2] = 2 * (1 - Kr);

    vecG[0] = 1.0;
    vecG[1] = -(Kb / Kg) * 2 * (1 - Kb);
    vecG[2] = -(Kr / Kg) * 2 * (1 - Kr);

    vecB[0] = 1.0;
    vecB[1] = 2 * (1 - Kb);
    vecB[2] = 0;

    /* Assume all components has the same bitdepth */
    S = (1 << in_yuv_info->finfo->depth[0]) - 1;
    Sy = (gdouble) S / scale[0];
    Suv = (gdouble) S / scale[1];
    Oy = -((gdouble) offset[0] / scale[0]);
    Ouv = -((gdouble) offset[1] / scale[1]);

    matrix->matrix[0][0] = Sy * vecR[0];
    matrix->matrix[1][0] = Sy * vecG[0];
    matrix->matrix[2][0] = Sy * vecB[0];

    matrix->matrix[0][1] = Suv * vecR[1];
    matrix->matrix[1][1] = Suv * vecG[1];
    matrix->matrix[2][1] = Suv * vecB[1];

    matrix->matrix[0][2] = Suv * vecR[2];
    matrix->matrix[1][2] = Suv * vecG[2];
    matrix->matrix[2][2] = Suv * vecB[2];

    matrix->offset[0] = vecR[0] * Oy + vecR[1] * Ouv + vecR[2] * Ouv;
    matrix->offset[1] = vecG[0] * Oy + vecG[1] * Ouv + vecG[2] * Ouv;
    matrix->offset[2] = vecB[0] * Oy + vecB[1] * Ouv + vecB[2] * Ouv;

    /* Apply RGB range scale matrix */
    if (out_rgb_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
      GstCudaColorMatrix scale_matrix, rst;
      GstVideoInfo full_rgb = *out_rgb_info;

      full_rgb.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

      if (gst_cuda_color_range_adjust_matrix_unorm (&full_rgb,
              out_rgb_info, &scale_matrix)) {
        /* Ms * Matrix */
        color_matrix_multiply (&rst, &scale_matrix, matrix);

        /* Ms * transform offsets */
        for (guint i = 0; i < 3; i++) {
          gdouble val = 0;
          for (guint j = 0; j < 3; j++) {
            val += scale_matrix.matrix[i][j] * matrix->offset[j];
          }
          rst.offset[i] = val + scale_matrix.offset[i];
        }

        /* copy back to output matrix */
        for (guint i = 0; i < 3; i++) {
          for (guint j = 0; j < 3; j++) {
            matrix->matrix[i][j] = rst.matrix[i][j];
          }
          matrix->offset[i] = rst.offset[i];
          matrix->min[i] = scale_matrix.min[i];
          matrix->max[i] = scale_matrix.max[i];
        }
      }
    }
  } else {
    /* Unknown matrix */
    matrix->matrix[0][0] = 1.0;
    matrix->matrix[1][1] = 1.0;
    matrix->matrix[2][2] = 1.0;
  }

  return TRUE;
}

/**
 * gst_cuda_rgb_to_yuv_matrix_unorm:
 * @in_rgb_info: a #GstVideoInfo of input RGB signal
 * @out_yuv_info: a #GstVideoInfo of output YUV signal
 * @matrix: a #GstCudaColorMatrix
 *
 * Calculates transform matrix from RGB to YUV conversion. Both input and output
 * signals are in normalized [0.0..1.0] space and additional gamma decoding
 * or primary/transfer function transform is not performed by this matrix.
 *
 * Resulting RGB values can be calculated by
 * | Y' |                           | R' |   | matrix.offset[0] |
 * | Cb | = clamp ( matrix.matrix * | G' | + | matrix.offset[1] |, matrix.min, matrix.max )
 * | Cr |                           | B' |   | matrix.offset[2] |
 *
 * Returns: %TRUE if successful
 */
static gboolean
gst_cuda_rgb_to_yuv_matrix_unorm (const GstVideoInfo * in_rgb_info,
    const GstVideoInfo * out_yuv_info, GstCudaColorMatrix * matrix)
{
  gint offset[4], scale[4];
  gdouble Kr, Kb, Kg;

  /*
   * <Formula>
   *
   * Input: Unsigned normalized non-linear R'G'B'(unorm), [0.0..1.0] range
   * Output: Unsigned normalized Y'CbCr(unorm), [0.0..1.0] range
   *
   * 1) R'G'B' to YPbPr
   * | Y  |      | R' |
   * | Pb | = M *| G' |
   * | Pr |      | B' |
   * where
   *     | vecY |
   * M = | vecU |
   *     | vecV |
   * vecY = |       Kr      ,       Kg      ,      Kb       |
   * vecU = | -0.5*Kr/(1-Kb), -0.5*Kg/(1-Kb),     0.5       |
   * vecV = |      0.5      , -0.5*Kg/(1-Kr), -0.5*Kb(1-Kr) |
   *
   * 2) YPbPr to Y'CbCr(unorm)
   * Y'(unorm) = (Y  * scaleY + offsetY)       / S
   * Cb(unorm) = (Pb * scaleCbCr + offsetCbCr) / S
   * Cr(unorm) = (Pr * scaleCbCr + offsetCbCr) / S
   * =>
   * Y'(unorm) = (Y  * scaleY    / S) + (offsetY    / S)
   * Cb(unorm) = (Pb * scaleCbCr / S) + (offsetCbCr / S)
   * Cr(unorm) = (Pb * scaleCbCr / S) + (offsetCbCr / S)
   * where S = (2 ^ bitdepth) - 1
   *
   * 3) RGB -> YUV matrix
   * | Y'(unorm) |            | R' |   | offsetA |
   * | Cb(unorm) | = Matrix * | G' | + | offsetB |
   * | Cr(unorm) |            | B' |   | offsetC |
   *
   * where
   *          | (scaleY/S)    * vecY |
   * Matrix = | (scaleCbCr/S) * vecU |
   *          | (scaleCbCr/S) * vecV |
   *
   * offsetA = offsetY    / S
   * offsetB = offsetCbCr / S
   * offsetC = offsetCbCr / S
   *
   * 4) Consider 16-235 scale RGB
   * RGBstudio(16..235) -> RGBfull(0..255) matrix is represented by
   * | Rf |      | Rs |   | Or |
   * | Gf | = Ms | Gs | + | Og |
   * | Bf |      | Bs |   | Ob |
   *
   * Combining all matrix into
   * | Y'(unorm) |                 | Rs |   | Or |     | offsetA |
   * | Cb(unorm) | = Matrix * ( Ms | Gs | + | Og | ) + | offsetB |
   * | Cr(unorm) |                 | Bs |   | Ob |     | offsetC |
   *
   *                             | Rs |          | Or |   | offsetA |
   *               = Matrix * Ms | Gs | + Matrix | Og | + | offsetB |
   *                             | Bs |          | Ob |   | offsetB |
   */

  memset (matrix, 0, sizeof (GstCudaColorMatrix));
  for (guint i = 0; i < 3; i++)
    matrix->max[i] = 1.0;

  gst_video_color_range_offsets (out_yuv_info->colorimetry.range,
      out_yuv_info->finfo, offset, scale);

  if (gst_video_color_matrix_get_Kr_Kb (out_yuv_info->colorimetry.matrix,
          &Kr, &Kb)) {
    guint S;
    gdouble Sy, Suv;
    gdouble Oy, Ouv;
    gdouble vecY[3], vecU[3], vecV[3];

    Kg = 1.0 - Kr - Kb;

    vecY[0] = Kr;
    vecY[1] = Kg;
    vecY[2] = Kb;

    vecU[0] = -0.5 * Kr / (1 - Kb);
    vecU[1] = -0.5 * Kg / (1 - Kb);
    vecU[2] = 0.5;

    vecV[0] = 0.5;
    vecV[1] = -0.5 * Kg / (1 - Kr);
    vecV[2] = -0.5 * Kb / (1 - Kr);

    /* Assume all components has the same bitdepth */
    S = (1 << out_yuv_info->finfo->depth[0]) - 1;
    Sy = (gdouble) scale[0] / S;
    Suv = (gdouble) scale[1] / S;
    Oy = (gdouble) offset[0] / S;
    Ouv = (gdouble) offset[1] / S;

    for (guint i = 0; i < 3; i++) {
      matrix->matrix[0][i] = Sy * vecY[i];
      matrix->matrix[1][i] = Suv * vecU[i];
      matrix->matrix[2][i] = Suv * vecV[i];
    }

    matrix->offset[0] = Oy;
    matrix->offset[1] = Ouv;
    matrix->offset[2] = Ouv;

    matrix->min[0] = Oy;
    matrix->min[1] = Oy;
    matrix->min[2] = Oy;

    matrix->max[0] = ((gdouble) scale[0] + offset[0]) / S;
    matrix->max[1] = ((gdouble) scale[1] + offset[0]) / S;
    matrix->max[2] = ((gdouble) scale[1] + offset[0]) / S;

    /* Apply RGB range scale matrix */
    if (in_rgb_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
      GstCudaColorMatrix scale_matrix, rst;
      GstVideoInfo full_rgb = *in_rgb_info;

      full_rgb.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

      if (gst_cuda_color_range_adjust_matrix_unorm (in_rgb_info,
              &full_rgb, &scale_matrix)) {
        /* Matrix * Ms */
        color_matrix_multiply (&rst, matrix, &scale_matrix);

        /* Matrix * scale offsets */
        for (guint i = 0; i < 3; i++) {
          gdouble val = 0;
          for (guint j = 0; j < 3; j++) {
            val += matrix->matrix[i][j] * scale_matrix.offset[j];
          }
          rst.offset[i] = val + matrix->offset[i];
        }

        /* copy back to output matrix */
        for (guint i = 0; i < 3; i++) {
          for (guint j = 0; j < 3; j++) {
            matrix->matrix[i][j] = rst.matrix[i][j];
          }
          matrix->offset[i] = rst.offset[i];
        }
      }
    }
  } else {
    /* Unknown matrix */
    matrix->matrix[0][0] = 1.0;
    matrix->matrix[1][1] = 1.0;
    matrix->matrix[2][2] = 1.0;
  }

  return TRUE;
}

typedef struct
{
  float coeffX[3];
  float coeffY[3];
  float coeffZ[3];
  float offset[3];
  float min[3];
  float max[3];
} ColorMatrix;

typedef struct
{
  ColorMatrix toRGBCoeff;
  ColorMatrix toYuvCoeff;
  ColorMatrix primariesCoeff;
} ConstBuffer;

#define COLOR_SPACE_IDENTITY "color_space_identity"
#define COLOR_SPACE_CONVERT "color_space_convert"

#define SAMPLE_YUV_PLANAR "sample_yuv_planar"
#define SAMPLE_YV12 "sample_yv12"
#define SAMPLE_YUV_PLANAR_10BIS "sample_yuv_planar_10bits"
#define SAMPLE_YUV_PLANAR_12BIS "sample_yuv_planar_12bits"
#define SAMPLE_SEMI_PLANAR "sample_semi_planar"
#define SAMPLE_SEMI_PLANAR_SWAP "sample_semi_planar_swap"
#define SAMPLE_RGBA "sample_rgba"
#define SAMPLE_BGRA "sample_bgra"
#define SAMPLE_RGBx "sample_rgbx"
#define SAMPLE_BGRx "sample_bgrx"
#define SAMPLE_ARGB "sample_argb"
/* same as ARGB */
#define SAMPLE_ARGB64 "sample_argb"
#define SAMPLE_AGBR "sample_abgr"
#define SAMPLE_RGBP "sample_rgbp"
#define SAMPLE_BGRP "sample_bgrp"
#define SAMPLE_GBR "sample_gbr"
#define SAMPLE_GBR_10 "sample_gbr_10"
#define SAMPLE_GBR_12 "sample_gbr_12"
#define SAMPLE_GBRA "sample_gbra"

#define WRITE_I420 "write_i420"
#define WRITE_YV12 "write_yv12"
#define WRITE_NV12 "write_nv12"
#define WRITE_NV21 "write_nv21"
#define WRITE_P010 "write_p010"
#define WRITE_I420_10 "write_i420_10"
#define WRITE_I420_12 "write_i420_12"
#define WRITE_Y444 "write_y444"
#define WRITE_Y444_10 "write_y444_10"
#define WRITE_Y444_12 "write_y444_12"
#define WRITE_Y444_16 "write_y444_16"
#define WRITE_RGBA "write_rgba"
#define WRITE_RGBx "write_rgbx"
#define WRITE_BGRA "write_bgra"
#define WRITE_BGRx "write_bgrx"
#define WRITE_ARGB "write_argb"
#define WRITE_ABGR "write_abgr"
#define WRITE_RGB "write_rgb"
#define WRITE_BGR "write_bgr"
#define WRITE_RGB10A2 "write_rgb10a2"
#define WRITE_BGR10A2 "write_bgr10a2"
#define WRITE_Y42B "write_y42b"
#define WRITE_I422_10 "write_i422_10"
#define WRITE_I422_12 "write_i422_12"
#define WRITE_RGBP "write_rgbp"
#define WRITE_BGRP "write_bgrp"
#define WRITE_GBR "write_gbr"
#define WRITE_GBR_10 "write_gbr_10"
#define WRITE_GBR_12 "write_gbr_12"
#define WRITE_GBR_16 "write_gbr_16"
#define WRITE_GBRA "write_gbra"
#define ROTATE_IDENTITY "rotate_identity"
#define ROTATE_90R "rotate_90r"
#define ROTATE_180 "rotate_180"
#define ROTATE_90L "rotate_90l"
#define ROTATE_HORIZ "rotate_horiz"
#define ROTATE_VERT "rotate_vert"
#define ROTATE_UL_LR "rotate_ul_lr"
#define ROTATE_UR_LL "rotate_ur_ll"

/* *INDENT-OFF* */
const static gchar KERNEL_COMMON[] =
"struct ColorMatrix\n"
"{\n"
"  float CoeffX[3];\n"
"  float CoeffY[3];\n"
"  float CoeffZ[3];\n"
"  float Offset[3];\n"
"  float Min[3];\n"
"  float Max[3];\n"
"};\n"
"\n"
"__device__ inline float\n"
"dot (const float coeff[3], float3 val)\n"
"{\n"
"  return coeff[0] * val.x + coeff[1] * val.y + coeff[2] * val.z;\n"
"}\n"
"\n"
"__device__ inline float\n"
"clamp (float val, float min_val, float max_val)\n"
"{\n"
"  return max (min_val, min (val, max_val));\n"
"}\n"
"\n"
"__device__ inline float3\n"
"clamp3 (float3 val, const float min_val[3], const float max_val[3])\n"
"{\n"
"  return make_float3 (clamp (val.x, min_val[0], max_val[0]),\n"
"      clamp (val.y, min_val[1], max_val[2]),\n"
"      clamp (val.z, min_val[1], max_val[2]));\n"
"}\n"
"\n"
"__device__ inline unsigned char\n"
"scale_to_2bits (float val)\n"
"{\n"
"  return (unsigned short) __float2int_rz (val * 3.0);\n"
"}\n"
"\n"
"__device__ inline unsigned char\n"
"scale_to_uchar (float val)\n"
"{\n"
"  return (unsigned char) __float2int_rz (val * 255.0);\n"
"}\n"
"\n"
"__device__ inline unsigned short\n"
"scale_to_ushort (float val)\n"
"{\n"
"  return (unsigned short) __float2int_rz (val * 65535.0);\n"
"}\n"
"\n"
"__device__ inline unsigned short\n"
"scale_to_10bits (float val)\n"
"{\n"
"  return (unsigned short) __float2int_rz (val * 1023.0);\n"
"}\n"
"\n"
"__device__ inline unsigned short\n"
"scale_to_12bits (float val)\n"
"{\n"
"  return (unsigned short) __float2int_rz (val * 4095.0);\n"
"}\n"
"\n"
"__device__ inline float3\n"
COLOR_SPACE_IDENTITY "(float3 sample, const ColorMatrix * matrix)\n"
"{\n"
"  return sample;\n"
"}\n"
"\n"
"__device__ inline float3\n"
COLOR_SPACE_CONVERT "(float3 sample, const ColorMatrix * matrix)\n"
"{\n"
"  float3 out;\n"
"  out.x = dot (matrix->CoeffX, sample);\n"
"  out.y = dot (matrix->CoeffY, sample);\n"
"  out.z = dot (matrix->CoeffZ, sample);\n"
"  out.x += matrix->Offset[0];\n"
"  out.y += matrix->Offset[1];\n"
"  out.z += matrix->Offset[2];\n"
"  return clamp3 (out, matrix->Min, matrix->Max);\n"
"}\n"
"/* All 8bits yuv planar except for yv12 */\n"
"__device__ inline float4\n"
SAMPLE_YUV_PLANAR "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float u = tex2D<float>(tex1, x, y);\n"
"  float v = tex2D<float>(tex2, x, y);\n"
"  return make_float4 (luma, u, v, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_YV12 "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float u = tex2D<float>(tex2, x, y);\n"
"  float v = tex2D<float>(tex1, x, y);\n"
"  return make_float4 (luma, u, v, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_YUV_PLANAR_10BIS "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float u = tex2D<float>(tex1, x, y);\n"
"  float v = tex2D<float>(tex2, x, y);\n"
"  /* (1 << 6) to scale [0, 1.0) range */\n"
"  return make_float4 (luma * 64, u * 64, v * 64, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_YUV_PLANAR_12BIS "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float u = tex2D<float>(tex1, x, y);\n"
"  float v = tex2D<float>(tex2, x, y);\n"
"  /* (1 << 4) to scale [0, 1.0) range */\n"
"  return make_float4 (luma * 16, u * 16, v * 16, 1);\n"
"}\n"
"\n"
"/* NV12, P010, and P016 */\n"
"__device__ inline float4\n"
SAMPLE_SEMI_PLANAR "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float2 uv = tex2D<float2>(tex1, x, y);\n"
"  return make_float4 (luma, uv.x, uv.y, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_SEMI_PLANAR_SWAP "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float luma = tex2D<float>(tex0, x, y);\n"
"  float2 vu = tex2D<float2>(tex1, x, y);\n"
"  return make_float4 (luma, vu.y, vu.x, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_RGBA "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  return tex2D<float4>(tex0, x, y);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_BGRA "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float4 bgra = tex2D<float4>(tex0, x, y);\n"
"  return make_float4 (bgra.z, bgra.y, bgra.x, bgra.w);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_RGBx "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float4 rgbx = tex2D<float4>(tex0, x, y);\n"
"  rgbx.w = 1;\n"
"  return rgbx;\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_BGRx "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float4 bgrx = tex2D<float4>(tex0, x, y);\n"
"  return make_float4 (bgrx.z, bgrx.y, bgrx.x, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_ARGB "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float4 argb = tex2D<float4>(tex0, x, y);\n"
"  return make_float4 (argb.y, argb.z, argb.w, argb.x);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_AGBR "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float4 abgr = tex2D<float4>(tex0, x, y);\n"
"  return make_float4 (abgr.w, abgr.z, abgr.y, abgr.x);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_RGBP "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float r = tex2D<float>(tex0, x, y);\n"
"  float g = tex2D<float>(tex1, x, y);\n"
"  float b = tex2D<float>(tex2, x, y);\n"
"  return make_float4 (r, g, b, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_BGRP "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float b = tex2D<float>(tex0, x, y);\n"
"  float g = tex2D<float>(tex1, x, y);\n"
"  float r = tex2D<float>(tex2, x, y);\n"
"  return make_float4 (r, g, b, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_GBR "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float g = tex2D<float>(tex0, x, y);\n"
"  float b = tex2D<float>(tex1, x, y);\n"
"  float r = tex2D<float>(tex2, x, y);\n"
"  return make_float4 (r, g, b, 1);\n"
"}\n"
"__device__ inline float4\n"
SAMPLE_GBR_10 "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float g = tex2D<float>(tex0, x, y);\n"
"  float b = tex2D<float>(tex1, x, y);\n"
"  float r = tex2D<float>(tex2, x, y);\n"
"  /* (1 << 6) to scale [0, 1.0) range */\n"
"  return make_float4 (r * 64, g * 64, b * 64, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_GBR_12 "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float g = tex2D<float>(tex0, x, y);\n"
"  float b = tex2D<float>(tex1, x, y);\n"
"  float r = tex2D<float>(tex2, x, y);\n"
"  /* (1 << 4) to scale [0, 1.0) range */\n"
"  return make_float4 (r * 16, g * 16, b * 16, 1);\n"
"}\n"
"\n"
"__device__ inline float4\n"
SAMPLE_GBRA "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, float x, float y)\n"
"{\n"
"  float g = tex2D<float>(tex0, x, y);\n"
"  float b = tex2D<float>(tex1, x, y);\n"
"  float r = tex2D<float>(tex2, x, y);\n"
"  float a = tex2D<float>(tex3, x, y);\n"
"  return make_float4 (r, g, b, a);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_I420 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  dst0[x + y * stride0] = scale_to_uchar (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x / 2 + (y / 2) * stride1;\n"
"    dst1[pos] = scale_to_uchar (sample.y);\n"
"    dst2[pos] = scale_to_uchar (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_YV12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  dst0[x + y * stride0] = scale_to_uchar (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x / 2 + (y / 2) * stride1;\n"
"    dst1[pos] = scale_to_uchar (sample.z);\n"
"    dst2[pos] = scale_to_uchar (sample.y);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_NV12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  dst0[x + y * stride0] = scale_to_uchar (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x + (y / 2) * stride1;\n"
"    dst1[pos] = scale_to_uchar (sample.y);\n"
"    dst1[pos + 1] = scale_to_uchar (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_NV21 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  dst0[x + y * stride0] = scale_to_uchar (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x + (y / 2) * stride1;\n"
"    dst1[pos] = scale_to_uchar (sample.z);\n"
"    dst1[pos + 1] = scale_to_uchar (sample.y);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_P010 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  *(unsigned short *) &dst0[x * 2 + y * stride0] = scale_to_ushort (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x * 2 + (y / 2) * stride1;\n"
"    *(unsigned short *) &dst1[pos] = scale_to_ushort (sample.y);\n"
"    *(unsigned short *) &dst1[pos + 2] = scale_to_ushort (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_I420_10 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  *(unsigned short *) &dst0[x * 2 + y * stride0] = scale_to_10bits (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x + (y / 2) * stride1;\n"
"    *(unsigned short *) &dst1[pos] = scale_to_10bits (sample.y);\n"
"    *(unsigned short *) &dst2[pos] = scale_to_10bits (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_I420_12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  *(unsigned short *) &dst0[x * 2 + y * stride0] = scale_to_12bits (sample.x);\n"
"  if (x % 2 == 0 && y % 2 == 0) {\n"
"    unsigned int pos = x + (y / 2) * stride1;\n"
"    *(unsigned short *) &dst1[pos] = scale_to_12bits (sample.y);\n"
"    *(unsigned short *) &dst2[pos] = scale_to_12bits (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_Y444 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.x);\n"
"  dst1[pos] = scale_to_uchar (sample.y);\n"
"  dst2[pos] = scale_to_uchar (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_Y444_10 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_10bits (sample.x);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_10bits (sample.y);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_10bits (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_Y444_12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_12bits (sample.x);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_12bits (sample.y);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_12bits (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_Y444_16 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_ushort (sample.x);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_ushort (sample.y);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_ushort (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_RGBA "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 3] = scale_to_uchar (sample.w);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_RGBx "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 3] = 255;\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_BGRA "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 3] = scale_to_uchar (sample.w);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_BGRx "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 3] = 255;\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_ARGB "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.w);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 3] = scale_to_uchar (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_ABGR "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 4 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.w);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 3] = scale_to_uchar (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_RGB "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 3 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.x);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_BGR "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 3 + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.z);\n"
"  dst0[pos + 1] = scale_to_uchar (sample.y);\n"
"  dst0[pos + 2] = scale_to_uchar (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_RGB10A2 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  unsigned int alpha = (unsigned int) scale_to_2bits (sample.x);\n"
"  unsigned int packed_rgb = alpha << 30;\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.x));\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.y)) << 10;\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.z)) << 20;\n"
"  *(unsigned int *) &dst0[x * 4 + y * stride0] = packed_rgb;\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_BGR10A2 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  unsigned int alpha = (unsigned int) scale_to_2bits (sample.x);\n"
"  unsigned int packed_rgb = alpha << 30;\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.x)) << 20;\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.y)) << 10;\n"
"  packed_rgb |= ((unsigned int) scale_to_10bits (sample.z));\n"
"  *(unsigned int *) &dst0[x * 4 + y * stride0] = packed_rgb;\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_Y42B "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  dst0[x + y * stride0] = scale_to_uchar (sample.x);\n"
"  if (x % 2 == 0) {\n"
"    unsigned int pos = x / 2 + y * stride1;\n"
"    dst1[pos] = scale_to_uchar (sample.y);\n"
"    dst2[pos] = scale_to_uchar (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_I422_10 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  *(unsigned short *) &dst0[x * 2 + y * stride0] = scale_to_10bits (sample.x);\n"
"  if (x % 2 == 0) {\n"
"    unsigned int pos = x + y * stride1;\n"
"    *(unsigned short *) &dst1[pos] = scale_to_10bits (sample.y);\n"
"    *(unsigned short *) &dst2[pos] = scale_to_10bits (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_I422_12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  *(unsigned short *) &dst0[x * 2 + y * stride0] = scale_to_12bits (sample.x);\n"
"  if (x % 2 == 0) {\n"
"    unsigned int pos = x + y * stride1;\n"
"    *(unsigned short *) &dst1[pos] = scale_to_12bits (sample.y);\n"
"    *(unsigned short *) &dst2[pos] = scale_to_12bits (sample.z);\n"
"  }\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_RGBP "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.x);\n"
"  dst1[pos] = scale_to_uchar (sample.y);\n"
"  dst2[pos] = scale_to_uchar (sample.z);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_BGRP "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.z);\n"
"  dst1[pos] = scale_to_uchar (sample.y);\n"
"  dst2[pos] = scale_to_uchar (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_GBR "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.y);\n"
"  dst1[pos] = scale_to_uchar (sample.z);\n"
"  dst2[pos] = scale_to_uchar (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_GBR_10 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_10bits (sample.y);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_10bits (sample.z);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_10bits (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_GBR_12 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_12bits (sample.y);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_12bits (sample.z);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_12bits (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_GBR_16 "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x * 2 + y * stride0;\n"
"  *(unsigned short *) &dst0[pos] = scale_to_ushort (sample.y);\n"
"  *(unsigned short *) &dst1[pos] = scale_to_ushort (sample.z);\n"
"  *(unsigned short *) &dst2[pos] = scale_to_ushort (sample.x);\n"
"}\n"
"\n"
"__device__ inline void\n"
WRITE_GBRA "(unsigned char * dst0, unsigned char * dst1, unsigned char * dst2,\n"
"    unsigned char * dst3, float4 sample, int x, int y, int stride0, int stride1)\n"
"{\n"
"  int pos = x + y * stride0;\n"
"  dst0[pos] = scale_to_uchar (sample.y);\n"
"  dst1[pos] = scale_to_uchar (sample.z);\n"
"  dst2[pos] = scale_to_uchar (sample.x);\n"
"  dst3[pos] = scale_to_uchar (sample.w);\n"
"}\n"
"__device__ inline float2\n"
ROTATE_IDENTITY "(float x, float y)\n"
"{\n"
"  return make_float2(x, y);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_90R "(float x, float y)\n"
"{\n"
"  return make_float2(y, 1.0 - x);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_180 "(float x, float y)\n"
"{\n"
"  return make_float2(1.0 - x, 1.0 - y);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_90L "(float x, float y)\n"
"{\n"
"  return make_float2(1.0 - y, x);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_HORIZ "(float x, float y)\n"
"{\n"
"  return make_float2(1.0 - x, y);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_VERT "(float x, float y)\n"
"{\n"
"  return make_float2(x, 1.0 - y);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_UL_LR "(float x, float y)\n"
"{\n"
"  return make_float2(y, x);\n"
"}\n"
"\n"
"__device__ inline float2\n"
ROTATE_UR_LL "(float x, float y)\n"
"{\n"
"  return make_float2(1.0 - y, 1.0 - x);\n"
"}\n"
"\n";

#define GST_CUDA_KERNEL_UNPACK_FUNC "gst_cuda_kernel_unpack_func"
static const gchar RGB_TO_RGBx[] =
"extern \"C\" {\n"
"__global__ void\n"
GST_CUDA_KERNEL_UNPACK_FUNC
"(unsigned char *src, unsigned char *dst, int width, int height,\n"
"    int src_stride, int dst_stride)\n"
"{\n"
"  int x_pos = blockIdx.x * blockDim.x + threadIdx.x;\n"
"  int y_pos = blockIdx.y * blockDim.y + threadIdx.y;\n"
"  if (x_pos < width && y_pos < height) {\n"
"    int dst_pos = x_pos * 4 + y_pos * dst_stride;\n"
"    int src_pos = x_pos * 3 + y_pos * src_stride;\n"
"    dst[dst_pos] = src[src_pos];\n"
"    dst[dst_pos + 1] = src[src_pos + 1];\n"
"    dst[dst_pos + 2] = src[src_pos + 2];\n"
"    dst[dst_pos + 3] = 0xff;\n"
"  }\n"
"}\n"
"}\n";

static const gchar RGB10A2_TO_ARGB64[] =
"extern \"C\" {\n"
"__global__ void\n"
GST_CUDA_KERNEL_UNPACK_FUNC
"(unsigned char *src, unsigned char *dst, int width, int height,\n"
"    int src_stride, int dst_stride)\n"
"{\n"
"  int x_pos = blockIdx.x * blockDim.x + threadIdx.x;\n"
"  int y_pos = blockIdx.y * blockDim.y + threadIdx.y;\n"
"  if (x_pos < width && y_pos < height) {\n"
"    unsigned short a, r, g, b;\n"
"    unsigned int val;\n"
"    int dst_pos = x_pos * 8 + y_pos * dst_stride;\n"
"    val = *(unsigned int *)&src[x_pos * 4 + y_pos * src_stride];\n"
"    a = (val >> 30) & 0x03;\n"
"    a = (a << 14) | (a << 12) | (a << 10) | (a << 8) | (a << 6) | (a << 4) | (a << 2) | (a << 0);\n"
"    r = (val & 0x3ff);\n"
"    r = (r << 6) | (r >> 4);\n"
"    g = ((val >> 10) & 0x3ff);\n"
"    g = (g << 6) | (g >> 4);\n"
"    b = ((val >> 20) & 0x3ff);\n"
"    b = (b << 6) | (b >> 4);\n"
"    *(unsigned short *) &dst[dst_pos] = a;\n"
"    *(unsigned short *) &dst[dst_pos + 2] = r;\n"
"    *(unsigned short *) &dst[dst_pos + 4] = g;\n"
"    *(unsigned short *) &dst[dst_pos + 6] = b;\n"
"  }\n"
"}\n"
"}\n";

static const gchar BGR10A2_TO_ARGB64[] =
"extern \"C\" {\n"
"__global__ void\n"
GST_CUDA_KERNEL_UNPACK_FUNC
"(unsigned char *src, unsigned char *dst, int width, int height,\n"
"    int src_stride, int dst_stride)\n"
"{\n"
"  int x_pos = blockIdx.x * blockDim.x + threadIdx.x;\n"
"  int y_pos = blockIdx.y * blockDim.y + threadIdx.y;\n"
"  if (x_pos < width && y_pos < height) {\n"
"    unsigned short a, r, g, b;\n"
"    unsigned int val;\n"
"    int dst_pos = x_pos * 8 + y_pos * dst_stride;\n"
"    val = *(unsigned int *)&src[x_pos * 4 + y_pos * src_stride];\n"
"    a = (val >> 30) & 0x03;\n"
"    a = (a << 14) | (a << 12) | (a << 10) | (a << 8) | (a << 6) | (a << 4) | (a << 2) | (a << 0);\n"
"    b = (val & 0x3ff);\n"
"    b = (b << 6) | (b >> 4);\n"
"    g = ((val >> 10) & 0x3ff);\n"
"    g = (g << 6) | (g >> 4);\n"
"    r = ((val >> 20) & 0x3ff);\n"
"    r = (r << 6) | (r >> 4);\n"
"    *(unsigned short *) &dst[dst_pos] = a;\n"
"    *(unsigned short *) &dst[dst_pos + 2] = r;\n"
"    *(unsigned short *) &dst[dst_pos + 4] = g;\n"
"    *(unsigned short *) &dst[dst_pos + 6] = b;\n"
"  }\n"
"}\n"
"}\n";

#define GST_CUDA_KERNEL_MAIN_FUNC "gst_cuda_converter_main"

static const gchar TEMPLETA_KERNEL[] =
/* KERNEL_COMMON */
"%s\n"
/* UNPACK FUNCTION */
"%s\n"
"__constant__ ColorMatrix TO_RGB_MATRIX = { { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s } };\n"
"__constant__ ColorMatrix TO_YUV_MATRIX = { { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s },\n"
"                                           { %s, %s, %s } };\n"
"__constant__ int WIDTH = %d;\n"
"__constant__ int HEIGHT = %d;\n"
"__constant__ int LEFT = %d;\n"
"__constant__ int TOP = %d;\n"
"__constant__ int RIGHT = %d;\n"
"__constant__ int BOTTOM = %d;\n"
"__constant__ int VIEW_WIDTH = %d;\n"
"__constant__ int VIEW_HEIGHT = %d;\n"
"__constant__ float OFFSET_X = %s;\n"
"__constant__ float OFFSET_Y = %s;\n"
"__constant__ float BORDER_X = %s;\n"
"__constant__ float BORDER_Y = %s;\n"
"__constant__ float BORDER_Z = %s;\n"
"__constant__ float BORDER_W = %s;\n"
"\n"
"extern \"C\" {\n"
"__global__ void\n"
GST_CUDA_KERNEL_MAIN_FUNC "(cudaTextureObject_t tex0, cudaTextureObject_t tex1,\n"
"    cudaTextureObject_t tex2, cudaTextureObject_t tex3, unsigned char * dst0,\n"
"    unsigned char * dst1, unsigned char * dst2, unsigned char * dst3,\n"
"    int stride0, int stride1)\n"
"{\n"
"  int x_pos = blockIdx.x * blockDim.x + threadIdx.x;\n"
"  int y_pos = blockIdx.y * blockDim.y + threadIdx.y;\n"
"  float4 sample;\n"
"  if (x_pos >= WIDTH || y_pos >= HEIGHT)\n"
"    return;\n"
"  if (x_pos < LEFT || x_pos >= RIGHT || y_pos < TOP || y_pos >= BOTTOM) {\n"
"    sample = make_float4 (BORDER_X, BORDER_Y, BORDER_Z, BORDER_W);\n"
"  } else {\n"
"    float x = OFFSET_X + (float) (x_pos - LEFT) / VIEW_WIDTH;\n"
"    float y = OFFSET_Y + (float) (y_pos - TOP) / VIEW_HEIGHT;\n"
"    float2 rotated = %s (x, y);\n"
"    float4 s = %s (tex0, tex1, tex2, tex3, rotated.x, rotated.y);\n"
"    float3 xyz = make_float3 (s.x, s.y, s.z);\n"
"    float3 rgb = %s (xyz, &TO_RGB_MATRIX);\n"
"    float3 yuv = %s (rgb, &TO_YUV_MATRIX);\n"
"    sample = make_float4 (yuv.x, yuv.y, yuv.z, s.w);\n"
"  }\n"
"  %s (dst0, dst1, dst2, dst3, sample, x_pos, y_pos, stride0, stride1);\n"
"}\n"
"}\n";
/* *INDENT-ON* */

typedef struct _TextureFormat
{
  GstVideoFormat format;
  CUarray_format array_format[GST_VIDEO_MAX_COMPONENTS];
  guint channels[GST_VIDEO_MAX_COMPONENTS];
  const gchar *sample_func;
} TextureFormat;

#define CU_AD_FORMAT_NONE 0
#define MAKE_FORMAT_YUV_PLANAR(f,cf,sample_func) \
  { GST_VIDEO_FORMAT_ ##f,  { CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_ ##cf, \
      CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_NONE },  {1, 1, 1, 0}, sample_func }
#define MAKE_FORMAT_YUV_SEMI_PLANAR(f,cf,sample_func) \
  { GST_VIDEO_FORMAT_ ##f,  { CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_ ##cf, \
      CU_AD_FORMAT_NONE, CU_AD_FORMAT_NONE }, {1, 2, 0, 0}, sample_func }
#define MAKE_FORMAT_RGB(f,cf,sample_func) \
  { GST_VIDEO_FORMAT_ ##f,  { CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_NONE, \
      CU_AD_FORMAT_NONE, CU_AD_FORMAT_NONE }, {4, 0, 0, 0}, sample_func }
#define MAKE_FORMAT_RGBP(f,cf,sample_func) \
  { GST_VIDEO_FORMAT_ ##f,  { CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_ ##cf, \
      CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_NONE }, {1, 1, 1, 0}, sample_func }
#define MAKE_FORMAT_RGBAP(f,cf,sample_func) \
  { GST_VIDEO_FORMAT_ ##f,  { CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_ ##cf, \
      CU_AD_FORMAT_ ##cf, CU_AD_FORMAT_ ##cf }, {1, 1, 1, 1}, sample_func }

static const TextureFormat format_map[] = {
  MAKE_FORMAT_YUV_PLANAR (I420, UNSIGNED_INT8, SAMPLE_YUV_PLANAR),
  MAKE_FORMAT_YUV_PLANAR (YV12, UNSIGNED_INT8, SAMPLE_YV12),
  MAKE_FORMAT_YUV_SEMI_PLANAR (NV12, UNSIGNED_INT8, SAMPLE_SEMI_PLANAR),
  MAKE_FORMAT_YUV_SEMI_PLANAR (NV21, UNSIGNED_INT8, SAMPLE_SEMI_PLANAR_SWAP),
  MAKE_FORMAT_YUV_SEMI_PLANAR (P010_10LE, UNSIGNED_INT16, SAMPLE_SEMI_PLANAR),
  MAKE_FORMAT_YUV_SEMI_PLANAR (P012_LE, UNSIGNED_INT16, SAMPLE_SEMI_PLANAR),
  MAKE_FORMAT_YUV_SEMI_PLANAR (P016_LE, UNSIGNED_INT16, SAMPLE_SEMI_PLANAR),
  MAKE_FORMAT_YUV_PLANAR (I420_10LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_10BIS),
  MAKE_FORMAT_YUV_PLANAR (I420_12LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_12BIS),
  MAKE_FORMAT_YUV_PLANAR (Y444, UNSIGNED_INT8, SAMPLE_YUV_PLANAR),
  MAKE_FORMAT_YUV_PLANAR (Y444_10LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_10BIS),
  MAKE_FORMAT_YUV_PLANAR (Y444_12LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_12BIS),
  MAKE_FORMAT_YUV_PLANAR (Y444_16LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR),
  MAKE_FORMAT_RGB (RGBA, UNSIGNED_INT8, SAMPLE_RGBA),
  MAKE_FORMAT_RGB (BGRA, UNSIGNED_INT8, SAMPLE_BGRA),
  MAKE_FORMAT_RGB (RGBx, UNSIGNED_INT8, SAMPLE_RGBx),
  MAKE_FORMAT_RGB (BGRx, UNSIGNED_INT8, SAMPLE_BGRx),
  MAKE_FORMAT_RGB (ARGB, UNSIGNED_INT8, SAMPLE_ARGB),
  MAKE_FORMAT_RGB (ARGB64, UNSIGNED_INT16, SAMPLE_ARGB64),
  MAKE_FORMAT_RGB (ABGR, UNSIGNED_INT8, SAMPLE_AGBR),
  MAKE_FORMAT_YUV_PLANAR (Y42B, UNSIGNED_INT8, SAMPLE_YUV_PLANAR),
  MAKE_FORMAT_YUV_PLANAR (I422_10LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_10BIS),
  MAKE_FORMAT_YUV_PLANAR (I422_12LE, UNSIGNED_INT16, SAMPLE_YUV_PLANAR_12BIS),
  MAKE_FORMAT_RGBP (RGBP, UNSIGNED_INT8, SAMPLE_RGBP),
  MAKE_FORMAT_RGBP (BGRP, UNSIGNED_INT8, SAMPLE_BGRP),
  MAKE_FORMAT_RGBP (GBR, UNSIGNED_INT8, SAMPLE_GBR),
  MAKE_FORMAT_RGBP (GBR_10LE, UNSIGNED_INT16, SAMPLE_GBR_10),
  MAKE_FORMAT_RGBP (GBR_12LE, UNSIGNED_INT16, SAMPLE_GBR_12),
  MAKE_FORMAT_RGBP (GBR_16LE, UNSIGNED_INT16, SAMPLE_GBR),
  MAKE_FORMAT_RGBAP (GBRA, UNSIGNED_INT8, SAMPLE_GBRA),
};

typedef struct _TextureBuffer
{
  CUdeviceptr ptr;
  gsize stride;
  CUtexObject texture;
} TextureBuffer;

typedef struct
{
  gint x;
  gint y;
  gint width;
  gint height;
} ConverterRect;

struct _GstCudaConverterPrivate
{
  GstVideoInfo in_info;
  GstVideoInfo out_info;

  GstVideoOrientationMethod method;

  GstStructure *config;

  GstVideoInfo texture_info;
  const TextureFormat *texture_fmt;
  gint texture_align;
  ConverterRect dest_rect;

  TextureBuffer fallback_buffer[GST_VIDEO_MAX_COMPONENTS];
  CUfilter_mode filter_mode[GST_VIDEO_MAX_COMPONENTS];
  TextureBuffer unpack_buffer;

  CUmodule module;
  CUfunction main_func;
  CUfunction unpack_func;
};

static void gst_cuda_converter_dispose (GObject * object);
static void gst_cuda_converter_finalize (GObject * object);

#define gst_cuda_converter_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstCudaConverter, gst_cuda_converter,
    GST_TYPE_OBJECT);

static void
gst_cuda_converter_class_init (GstCudaConverterClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_cuda_converter_dispose;
  object_class->finalize = gst_cuda_converter_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_cuda_converter_debug,
      "cudaconverter", 0, "cudaconverter");
}

static void
gst_cuda_converter_init (GstCudaConverter * self)
{
  GstCudaConverterPrivate *priv;

  self->priv = priv = gst_cuda_converter_get_instance_private (self);
  priv->config = gst_structure_new_empty ("GstCudaConverter");
}

static void
gst_cuda_converter_dispose (GObject * object)
{
  GstCudaConverter *self = GST_CUDA_CONVERTER (object);
  GstCudaConverterPrivate *priv = self->priv;
  guint i;

  if (self->context && gst_cuda_context_push (self->context)) {
    if (priv->module) {
      CuModuleUnload (priv->module);
      priv->module = NULL;
    }

    for (i = 0; i < G_N_ELEMENTS (priv->fallback_buffer); i++) {
      if (priv->fallback_buffer[i].ptr) {
        if (priv->fallback_buffer[i].texture) {
          CuTexObjectDestroy (priv->fallback_buffer[i].texture);
          priv->fallback_buffer[i].texture = 0;
        }

        CuMemFree (priv->fallback_buffer[i].ptr);
        priv->fallback_buffer[i].ptr = 0;
      }
    }

    if (priv->unpack_buffer.ptr) {
      if (priv->unpack_buffer.texture) {
        CuTexObjectDestroy (priv->unpack_buffer.texture);
        priv->unpack_buffer.texture = 0;
      }

      CuMemFree (priv->unpack_buffer.ptr);
      priv->unpack_buffer.ptr = 0;
    }

    gst_cuda_context_pop (NULL);
  }

  gst_clear_object (&self->context);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_cuda_converter_finalize (GObject * object)
{
  GstCudaConverter *self = GST_CUDA_CONVERTER (object);
  GstCudaConverterPrivate *priv = self->priv;

  gst_structure_free (priv->config);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static const gchar *
get_color_range_name (GstVideoColorRange range)
{
  switch (range) {
    case GST_VIDEO_COLOR_RANGE_0_255:
      return "FULL";
    case GST_VIDEO_COLOR_RANGE_16_235:
      return "STUDIO";
    default:
      break;
  }

  return "UNKNOWN";
}

typedef struct _GstCudaColorMatrixString
{
  gchar matrix[3][3][G_ASCII_DTOSTR_BUF_SIZE];
  gchar offset[3][G_ASCII_DTOSTR_BUF_SIZE];
  gchar min[3][G_ASCII_DTOSTR_BUF_SIZE];
  gchar max[3][G_ASCII_DTOSTR_BUF_SIZE];
} GstCudaColorMatrixString;

static void
color_matrix_to_string (const GstCudaColorMatrix * m,
    GstCudaColorMatrixString * str)
{
  guint i, j;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      g_ascii_formatd (str->matrix[i][j], G_ASCII_DTOSTR_BUF_SIZE, "%f",
          m->matrix[i][j]);
    }

    g_ascii_formatd (str->offset[i],
        G_ASCII_DTOSTR_BUF_SIZE, "%f", m->offset[i]);
    g_ascii_formatd (str->min[i], G_ASCII_DTOSTR_BUF_SIZE, "%f", m->min[i]);
    g_ascii_formatd (str->max[i], G_ASCII_DTOSTR_BUF_SIZE, "%f", m->max[i]);
  }
}

static gboolean
gst_cuda_converter_setup (GstCudaConverter * self)
{
  GstCudaConverterPrivate *priv = self->priv;
  const GstVideoInfo *in_info;
  const GstVideoInfo *out_info;
  const GstVideoInfo *texture_info;
  GstCudaColorMatrix to_rgb_matrix;
  GstCudaColorMatrix to_yuv_matrix;
  GstCudaColorMatrix border_color_matrix;
  GstCudaColorMatrixString to_rgb_matrix_str;
  GstCudaColorMatrixString to_yuv_matrix_str;
  gchar border_color_str[4][G_ASCII_DTOSTR_BUF_SIZE];
  gdouble border_color[4];
  gchar offset_x[G_ASCII_DTOSTR_BUF_SIZE];
  gchar offset_y[G_ASCII_DTOSTR_BUF_SIZE];
  gint i, j;
  const gchar *unpack_function = NULL;
  const gchar *write_func = NULL;
  const gchar *to_rgb_func = COLOR_SPACE_IDENTITY;
  const gchar *to_yuv_func = COLOR_SPACE_IDENTITY;
  const gchar *rotate_func = ROTATE_IDENTITY;
  const GstVideoColorimetry *in_color;
  const GstVideoColorimetry *out_color;
  gchar *str;
  gchar *program = NULL;
  CUresult ret;

  in_info = &priv->in_info;
  out_info = &priv->out_info;
  texture_info = &priv->texture_info;
  in_color = &in_info->colorimetry;
  out_color = &out_info->colorimetry;

  memset (&to_rgb_matrix, 0, sizeof (GstCudaColorMatrix));
  color_matrix_identity (&to_rgb_matrix);

  memset (&to_yuv_matrix, 0, sizeof (GstCudaColorMatrix));
  color_matrix_identity (&to_yuv_matrix);

  switch (GST_VIDEO_INFO_FORMAT (out_info)) {
    case GST_VIDEO_FORMAT_I420:
      write_func = WRITE_I420;
      break;
    case GST_VIDEO_FORMAT_YV12:
      write_func = WRITE_YV12;
      break;
    case GST_VIDEO_FORMAT_NV12:
      write_func = WRITE_NV12;
      break;
    case GST_VIDEO_FORMAT_NV21:
      write_func = WRITE_NV21;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      write_func = WRITE_P010;
      break;
    case GST_VIDEO_FORMAT_I420_10LE:
      write_func = WRITE_I420_10;
      break;
    case GST_VIDEO_FORMAT_I420_12LE:
      write_func = WRITE_I420_12;
      break;
    case GST_VIDEO_FORMAT_Y444:
      write_func = WRITE_Y444;
      break;
    case GST_VIDEO_FORMAT_Y444_10LE:
      write_func = WRITE_Y444_10;
      break;
    case GST_VIDEO_FORMAT_Y444_12LE:
      write_func = WRITE_Y444_12;
      break;
    case GST_VIDEO_FORMAT_Y444_16LE:
      write_func = WRITE_Y444_16;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      write_func = WRITE_RGBA;
      break;
    case GST_VIDEO_FORMAT_RGBx:
      write_func = WRITE_RGBx;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      write_func = WRITE_BGRA;
      break;
    case GST_VIDEO_FORMAT_BGRx:
      write_func = WRITE_BGRx;
      break;
    case GST_VIDEO_FORMAT_ARGB:
      write_func = WRITE_ARGB;
      break;
    case GST_VIDEO_FORMAT_ABGR:
      write_func = WRITE_ABGR;
      break;
    case GST_VIDEO_FORMAT_RGB:
      write_func = WRITE_RGB;
      break;
    case GST_VIDEO_FORMAT_BGR:
      write_func = WRITE_BGR;
      break;
    case GST_VIDEO_FORMAT_RGB10A2_LE:
      write_func = WRITE_RGB10A2;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      write_func = WRITE_BGR10A2;
      break;
    case GST_VIDEO_FORMAT_Y42B:
      write_func = WRITE_Y42B;
      break;
    case GST_VIDEO_FORMAT_I422_10LE:
      write_func = WRITE_I422_10;
      break;
    case GST_VIDEO_FORMAT_I422_12LE:
      write_func = WRITE_I422_12;
      break;
    case GST_VIDEO_FORMAT_RGBP:
      write_func = WRITE_RGBP;
      break;
    case GST_VIDEO_FORMAT_BGRP:
      write_func = WRITE_BGRP;
      break;
    case GST_VIDEO_FORMAT_GBR:
      write_func = WRITE_GBR;
      break;
    case GST_VIDEO_FORMAT_GBR_10LE:
      write_func = WRITE_GBR_10;
      break;
    case GST_VIDEO_FORMAT_GBR_12LE:
      write_func = WRITE_GBR_12;
      break;
    case GST_VIDEO_FORMAT_GBR_16LE:
      write_func = WRITE_GBR_16;
      break;
    case GST_VIDEO_FORMAT_GBRA:
      write_func = WRITE_GBRA;
      break;
    default:
      break;
  }

  if (!write_func) {
    GST_ERROR_OBJECT (self, "Unknown write function for format %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)));
    return FALSE;
  }

  /* Decide texture info to use, 3 channel RGB or 10bits packed RGB
   * need be converted to other format */
  priv->texture_info = priv->in_info;
  switch (GST_VIDEO_INFO_FORMAT (in_info)) {
    case GST_VIDEO_FORMAT_RGB:
      gst_video_info_set_format (&priv->texture_info,
          GST_VIDEO_FORMAT_RGBx, GST_VIDEO_INFO_WIDTH (in_info),
          GST_VIDEO_INFO_HEIGHT (in_info));
      unpack_function = RGB_TO_RGBx;
      break;
    case GST_VIDEO_FORMAT_BGR:
      gst_video_info_set_format (&priv->texture_info,
          GST_VIDEO_FORMAT_BGRx, GST_VIDEO_INFO_WIDTH (in_info),
          GST_VIDEO_INFO_HEIGHT (in_info));
      unpack_function = RGB_TO_RGBx;
      break;
    case GST_VIDEO_FORMAT_RGB10A2_LE:
      gst_video_info_set_format (&priv->texture_info,
          GST_VIDEO_FORMAT_ARGB64, GST_VIDEO_INFO_WIDTH (in_info),
          GST_VIDEO_INFO_HEIGHT (in_info));
      unpack_function = RGB10A2_TO_ARGB64;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      gst_video_info_set_format (&priv->texture_info,
          GST_VIDEO_FORMAT_ARGB64, GST_VIDEO_INFO_WIDTH (in_info),
          GST_VIDEO_INFO_HEIGHT (in_info));
      unpack_function = BGR10A2_TO_ARGB64;
      break;
    default:
      break;
  }

  for (i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (format_map[i].format == GST_VIDEO_INFO_FORMAT (texture_info)) {
      priv->texture_fmt = &format_map[i];
      break;
    }
  }

  if (!priv->texture_fmt) {
    GST_ERROR_OBJECT (self, "Couldn't find texture format for %s (%s)",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (texture_info)));
    return FALSE;
  }

  /* calculate black color
   * TODO: add support border color */
  if (GST_VIDEO_INFO_IS_RGB (out_info)) {
    GstVideoInfo rgb_info = *out_info;
    rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
    gst_cuda_color_range_adjust_matrix_unorm (&rgb_info, out_info,
        &border_color_matrix);
  } else {
    GstVideoInfo rgb_info;

    gst_video_info_set_format (&rgb_info, GST_VIDEO_FORMAT_RGBA64_LE,
        out_info->width, out_info->height);

    gst_cuda_rgb_to_yuv_matrix_unorm (&rgb_info,
        out_info, &border_color_matrix);
  }

  for (i = 0; i < 3; i++) {
    /* TODO: property */
    gdouble border_rgba[4] = { 0, 0, 0 };
    border_color[i] = 0;
    for (j = 0; j < 3; j++)
      border_color[i] += border_color_matrix.matrix[i][j] * border_rgba[i];
    border_color[i] = border_color_matrix.offset[i];
    border_color[i] = CLAMP (border_color[i],
        border_color_matrix.min[i], border_color_matrix.max[i]);

    g_ascii_formatd (border_color_str[i],
        G_ASCII_DTOSTR_BUF_SIZE, "%f", border_color[i]);
  }
  g_ascii_formatd (border_color_str[3], G_ASCII_DTOSTR_BUF_SIZE, "%f", 1);

  /* FIXME: handle primaries and transfer functions */
  if (GST_VIDEO_INFO_IS_RGB (texture_info)) {
    if (GST_VIDEO_INFO_IS_RGB (out_info)) {
      /* RGB -> RGB */
      if (in_color->range == out_color->range) {
        GST_DEBUG_OBJECT (self, "RGB -> RGB conversion without matrix");
      } else {
        if (!gst_cuda_color_range_adjust_matrix_unorm (in_info, out_info,
                &to_rgb_matrix)) {
          GST_ERROR_OBJECT (self, "Failed to get RGB range adjust matrix");
          return FALSE;
        }

        str = gst_cuda_dump_color_matrix (&to_rgb_matrix);
        GST_DEBUG_OBJECT (self, "RGB range adjust %s -> %s\n%s",
            get_color_range_name (in_color->range),
            get_color_range_name (out_color->range), str);
        g_free (str);

        to_rgb_func = COLOR_SPACE_CONVERT;
      }
    } else {
      /* RGB -> YUV */
      if (!gst_cuda_rgb_to_yuv_matrix_unorm (in_info, out_info, &to_yuv_matrix)) {
        GST_ERROR_OBJECT (self, "Failed to get RGB -> YUV transform matrix");
        return FALSE;
      }

      str = gst_cuda_dump_color_matrix (&to_yuv_matrix);
      GST_DEBUG_OBJECT (self, "RGB -> YUV matrix:\n%s", str);
      g_free (str);

      to_yuv_func = COLOR_SPACE_CONVERT;
    }
  } else {
    if (GST_VIDEO_INFO_IS_RGB (out_info)) {
      /* YUV -> RGB */
      if (!gst_cuda_yuv_to_rgb_matrix_unorm (in_info, out_info, &to_rgb_matrix)) {
        GST_ERROR_OBJECT (self, "Failed to get YUV -> RGB transform matrix");
        return FALSE;
      }

      str = gst_cuda_dump_color_matrix (&to_rgb_matrix);
      GST_DEBUG_OBJECT (self, "YUV -> RGB matrix:\n%s", str);
      g_free (str);

      to_rgb_func = COLOR_SPACE_CONVERT;
    } else {
      /* YUV -> YUV */
      if (in_color->range == out_color->range) {
        GST_DEBUG_OBJECT (self, "YUV -> YU conversion without matrix");
      } else {
        if (!gst_cuda_color_range_adjust_matrix_unorm (in_info, out_info,
                &to_yuv_matrix)) {
          GST_ERROR_OBJECT (self, "Failed to get GRAY range adjust matrix");
          return FALSE;
        }

        str = gst_cuda_dump_color_matrix (&to_yuv_matrix);
        GST_DEBUG_OBJECT (self, "YUV range adjust matrix:\n%s", str);
        g_free (str);

        to_yuv_func = COLOR_SPACE_CONVERT;
      }
    }
  }

  color_matrix_to_string (&to_rgb_matrix, &to_rgb_matrix_str);
  color_matrix_to_string (&to_yuv_matrix, &to_yuv_matrix_str);

  /* half pixel offset, to sample texture at center of the pixel position */
  g_ascii_formatd (offset_x, G_ASCII_DTOSTR_BUF_SIZE, "%f",
      (gdouble) 0.5 / priv->dest_rect.width);
  g_ascii_formatd (offset_y, G_ASCII_DTOSTR_BUF_SIZE, "%f",
      (gdouble) 0.5 / priv->dest_rect.height);

  switch (priv->method) {
    case GST_VIDEO_ORIENTATION_90R:
      rotate_func = ROTATE_90R;
      break;
    case GST_VIDEO_ORIENTATION_180:
      rotate_func = ROTATE_180;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      rotate_func = ROTATE_90L;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      rotate_func = ROTATE_HORIZ;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      rotate_func = ROTATE_VERT;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      rotate_func = ROTATE_UL_LR;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      rotate_func = ROTATE_UR_LL;
      break;
    default:
      break;
  }

  str = g_strdup_printf (TEMPLETA_KERNEL, KERNEL_COMMON,
      unpack_function ? unpack_function : "",
      /* TO RGB matrix */
      to_rgb_matrix_str.matrix[0][0],
      to_rgb_matrix_str.matrix[0][1],
      to_rgb_matrix_str.matrix[0][2],
      to_rgb_matrix_str.matrix[1][0],
      to_rgb_matrix_str.matrix[1][1],
      to_rgb_matrix_str.matrix[1][2],
      to_rgb_matrix_str.matrix[2][0],
      to_rgb_matrix_str.matrix[2][1],
      to_rgb_matrix_str.matrix[2][2],
      to_rgb_matrix_str.offset[0],
      to_rgb_matrix_str.offset[1],
      to_rgb_matrix_str.offset[2],
      to_rgb_matrix_str.min[0],
      to_rgb_matrix_str.min[1],
      to_rgb_matrix_str.min[2],
      to_rgb_matrix_str.max[0],
      to_rgb_matrix_str.max[1], to_rgb_matrix_str.max[2],
      /* TO YUV matrix */
      to_yuv_matrix_str.matrix[0][0],
      to_yuv_matrix_str.matrix[0][1],
      to_yuv_matrix_str.matrix[0][2],
      to_yuv_matrix_str.matrix[1][0],
      to_yuv_matrix_str.matrix[1][1],
      to_yuv_matrix_str.matrix[1][2],
      to_yuv_matrix_str.matrix[2][0],
      to_yuv_matrix_str.matrix[2][1],
      to_yuv_matrix_str.matrix[2][2],
      to_yuv_matrix_str.offset[0],
      to_yuv_matrix_str.offset[1],
      to_yuv_matrix_str.offset[2],
      to_yuv_matrix_str.min[0],
      to_yuv_matrix_str.min[1],
      to_yuv_matrix_str.min[2],
      to_yuv_matrix_str.max[0],
      to_yuv_matrix_str.max[1], to_yuv_matrix_str.max[2],
      /* width/height */
      GST_VIDEO_INFO_WIDTH (out_info), GST_VIDEO_INFO_HEIGHT (out_info),
      /* viewport */
      priv->dest_rect.x, priv->dest_rect.y,
      priv->dest_rect.x + priv->dest_rect.width,
      priv->dest_rect.y + priv->dest_rect.height,
      priv->dest_rect.width, priv->dest_rect.height,
      /* half pixel offsets */
      offset_x, offset_y,
      /* border colors */
      border_color_str[0], border_color_str[1],
      border_color_str[2], border_color_str[3],
      /* adjust coord before sampling */
      rotate_func,
      /* sampler function name */
      priv->texture_fmt->sample_func,
      /* TO RGB conversion function name */
      to_rgb_func,
      /* TO YUV conversion function name */
      to_yuv_func,
      /* write function name */
      write_func);

  GST_LOG_OBJECT (self, "kernel code:\n%s\n", str);
  gint cuda_device;
  g_object_get (self->context, "cuda-device-id", &cuda_device, NULL);
  program = gst_cuda_nvrtc_compile_cubin (str, cuda_device);
  if (!program) {
    GST_WARNING_OBJECT (self, "Couldn't compile to cubin, trying ptx");
    program = gst_cuda_nvrtc_compile (str);
  }
  g_free (str);

  if (!program) {
    GST_ERROR_OBJECT (self, "Could not compile code");
    return FALSE;
  }

  if (priv->dest_rect.x != 0 || priv->dest_rect.y != 0 ||
      priv->dest_rect.width != out_info->width ||
      priv->dest_rect.height != out_info->height ||
      in_info->width != out_info->width
      || in_info->height != out_info->height) {
    for (i = 0; i < G_N_ELEMENTS (priv->filter_mode); i++)
      priv->filter_mode[i] = CU_TR_FILTER_MODE_LINEAR;
  } else {
    for (i = 0; i < G_N_ELEMENTS (priv->filter_mode); i++)
      priv->filter_mode[i] = CU_TR_FILTER_MODE_POINT;
  }

  if (!gst_cuda_context_push (self->context)) {
    GST_ERROR_OBJECT (self, "Couldn't push context");
    g_free (program);
    return FALSE;
  }

  /* Allocates intermediate memory for texture */
  if (unpack_function) {
    CUDA_TEXTURE_DESC texture_desc;
    CUDA_RESOURCE_DESC resource_desc;
    CUtexObject texture = 0;

    memset (&texture_desc, 0, sizeof (CUDA_TEXTURE_DESC));
    memset (&resource_desc, 0, sizeof (CUDA_RESOURCE_DESC));

    ret = CuMemAllocPitch (&priv->unpack_buffer.ptr,
        &priv->unpack_buffer.stride,
        GST_VIDEO_INFO_COMP_WIDTH (texture_info, 0) *
        GST_VIDEO_INFO_COMP_PSTRIDE (texture_info, 0),
        GST_VIDEO_INFO_HEIGHT (texture_info), 16);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't allocate unpack buffer");
      goto error;
    }

    resource_desc.resType = CU_RESOURCE_TYPE_PITCH2D;
    resource_desc.res.pitch2D.format = priv->texture_fmt->array_format[0];
    resource_desc.res.pitch2D.numChannels = 4;
    resource_desc.res.pitch2D.width = in_info->width;
    resource_desc.res.pitch2D.height = in_info->height;
    resource_desc.res.pitch2D.pitchInBytes = priv->unpack_buffer.stride;
    resource_desc.res.pitch2D.devPtr = priv->unpack_buffer.ptr;

    texture_desc.filterMode = priv->filter_mode[0];
    texture_desc.flags = 0x2;
    texture_desc.addressMode[0] = 1;
    texture_desc.addressMode[1] = 1;
    texture_desc.addressMode[2] = 1;

    ret = CuTexObjectCreate (&texture, &resource_desc, &texture_desc, NULL);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Couldn't create unpack texture");
      goto error;
    }

    priv->unpack_buffer.texture = texture;
  }

  ret = CuModuleLoadData (&priv->module, program);
  g_clear_pointer (&program, g_free);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (self, "Could not load module");
    priv->module = NULL;
    goto error;
  }

  ret = CuModuleGetFunction (&priv->main_func,
      priv->module, GST_CUDA_KERNEL_MAIN_FUNC);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (self, "Could not get main function");
    goto error;
  }

  if (unpack_function) {
    ret = CuModuleGetFunction (&priv->unpack_func,
        priv->module, GST_CUDA_KERNEL_UNPACK_FUNC);
    if (!gst_cuda_result (ret)) {
      GST_ERROR_OBJECT (self, "Could not get unpack function");
      goto error;
    }
  }

  gst_cuda_context_pop (NULL);

  return TRUE;

error:
  gst_cuda_context_pop (NULL);
  g_free (program);

  return FALSE;
}

static gboolean
copy_config (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstCudaConverter *self = (GstCudaConverter *) user_data;

  gst_structure_id_set_value (self->priv->config, field_id, value);

  return TRUE;
}

static void
gst_cuda_converter_set_config (GstCudaConverter * self, GstStructure * config)
{
  gst_structure_foreach (config, copy_config, self);
  gst_structure_free (config);
}

static gint
get_opt_int (GstCudaConverter * self, const gchar * opt, gint def)
{
  gint res;
  if (!gst_structure_get_int (self->priv->config, opt, &res))
    res = def;
  return res;
}

GstCudaConverter *
gst_cuda_converter_new (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstCudaContext * context,
    GstStructure * config)
{
  GstCudaConverter *self;
  GstCudaConverterPrivate *priv;
  gint method;

  g_return_val_if_fail (in_info != NULL, NULL);
  g_return_val_if_fail (out_info != NULL, NULL);
  g_return_val_if_fail (GST_IS_CUDA_CONTEXT (context), NULL);

  self = g_object_new (GST_TYPE_CUDA_CONVERTER, NULL);

  if (!GST_IS_CUDA_CONTEXT (context)) {
    GST_WARNING_OBJECT (self, "Not a valid cuda context object");
    goto error;
  }

  self->context = gst_object_ref (context);
  priv = self->priv;
  priv->in_info = *in_info;
  priv->out_info = *out_info;

  if (config)
    gst_cuda_converter_set_config (self, config);

  priv->dest_rect.x = get_opt_int (self, GST_CUDA_CONVERTER_OPT_DEST_X, 0);
  priv->dest_rect.y = get_opt_int (self, GST_CUDA_CONVERTER_OPT_DEST_Y, 0);
  priv->dest_rect.width = get_opt_int (self,
      GST_CUDA_CONVERTER_OPT_DEST_WIDTH, out_info->width);
  priv->dest_rect.height = get_opt_int (self,
      GST_CUDA_CONVERTER_OPT_DEST_HEIGHT, out_info->height);
  if (gst_structure_get_enum (priv->config,
          GST_CUDA_CONVERTER_OPT_ORIENTATION_METHOD,
          GST_TYPE_VIDEO_ORIENTATION_METHOD, &method)) {
    priv->method = method;
    GST_DEBUG_OBJECT (self, "Selected orientation method %d", method);
  }

  if (!gst_cuda_converter_setup (self))
    goto error;

  priv->texture_align = gst_cuda_context_get_texture_alignment (context);

  gst_object_ref_sink (self);
  return self;

error:
  gst_object_unref (self);
  return NULL;
}


static CUtexObject
gst_cuda_converter_create_texture_unchecked (GstCudaConverter * self,
    CUdeviceptr src, gint width, gint height, CUarray_format format,
    guint channels, gint stride, gint plane, CUfilter_mode mode)
{
  CUDA_TEXTURE_DESC texture_desc;
  CUDA_RESOURCE_DESC resource_desc;
  CUtexObject texture = 0;
  CUresult cuda_ret;

  memset (&texture_desc, 0, sizeof (CUDA_TEXTURE_DESC));
  memset (&resource_desc, 0, sizeof (CUDA_RESOURCE_DESC));

  resource_desc.resType = CU_RESOURCE_TYPE_PITCH2D;
  resource_desc.res.pitch2D.format = format;
  resource_desc.res.pitch2D.numChannels = channels;
  resource_desc.res.pitch2D.width = width;
  resource_desc.res.pitch2D.height = height;
  resource_desc.res.pitch2D.pitchInBytes = stride;
  resource_desc.res.pitch2D.devPtr = src;

  texture_desc.filterMode = mode;
  /* Will read texture value as a normalized [0, 1] float value
   * with [0, 1) coordinates */
  /* CU_TRSF_NORMALIZED_COORDINATES */
  texture_desc.flags = 0x2;
  /* CU_TR_ADDRESS_MODE_CLAMP */
  texture_desc.addressMode[0] = 1;
  texture_desc.addressMode[1] = 1;
  texture_desc.addressMode[2] = 1;

  cuda_ret = CuTexObjectCreate (&texture, &resource_desc, &texture_desc, NULL);

  if (!gst_cuda_result (cuda_ret)) {
    GST_ERROR_OBJECT (self, "Could not create texture");
    return 0;
  }

  return texture;
}

static gboolean
ensure_fallback_buffer (GstCudaConverter * self, gint width_in_bytes,
    gint height, guint plane)
{
  GstCudaConverterPrivate *priv = self->priv;
  CUresult ret;

  if (priv->fallback_buffer[plane].ptr)
    return TRUE;

  ret = CuMemAllocPitch (&priv->fallback_buffer[plane].ptr,
      &priv->fallback_buffer[plane].stride, width_in_bytes, height, 16);

  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (self, "Couldn't allocate fallback buffer");
    return FALSE;
  }

  return TRUE;
}

static CUtexObject
gst_cuda_converter_create_texture (GstCudaConverter * self,
    CUdeviceptr src, gint width, gint height, gint stride, CUfilter_mode mode,
    CUarray_format format, guint channles, gint plane, CUstream stream)
{
  GstCudaConverterPrivate *priv = self->priv;
  CUresult ret;
  CUdeviceptr src_ptr;
  CUDA_MEMCPY2D params = { 0, };

  if (!ensure_fallback_buffer (self, stride, height, plane))
    return 0;

  params.srcMemoryType = CU_MEMORYTYPE_DEVICE;
  params.srcPitch = stride;
  params.srcDevice = (CUdeviceptr) src;

  params.dstMemoryType = CU_MEMORYTYPE_DEVICE;
  params.dstPitch = priv->fallback_buffer[plane].stride;
  params.dstDevice = priv->fallback_buffer[plane].ptr;
  params.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (&priv->in_info, plane)
      * GST_VIDEO_INFO_COMP_PSTRIDE (&priv->in_info, plane),
      params.Height = GST_VIDEO_INFO_COMP_HEIGHT (&priv->in_info, plane);

  ret = CuMemcpy2DAsync (&params, stream);
  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (self, "Couldn't copy to fallback buffer");
    return 0;
  }

  if (!priv->fallback_buffer[plane].texture) {
    src_ptr = priv->fallback_buffer[plane].ptr;
    stride = priv->fallback_buffer[plane].stride;

    priv->fallback_buffer[plane].texture =
        gst_cuda_converter_create_texture_unchecked (self, src_ptr, width,
        height, format, channles, stride, plane, mode);
  }

  return priv->fallback_buffer[plane].texture;
}

static gboolean
gst_cuda_converter_unpack_rgb (GstCudaConverter * self,
    GstVideoFrame * src_frame, CUstream stream)
{
  GstCudaConverterPrivate *priv = self->priv;
  CUdeviceptr src;
  gint width, height, src_stride, dst_stride;
  CUresult ret;
  gpointer args[] = { &src, &priv->unpack_buffer.ptr,
    &width, &height, &src_stride, &dst_stride
  };

  g_assert (priv->unpack_buffer.ptr);
  g_assert (priv->unpack_buffer.stride > 0);

  src = (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (src_frame, 0);
  width = GST_VIDEO_FRAME_WIDTH (src_frame);
  height = GST_VIDEO_FRAME_HEIGHT (src_frame);
  src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (src_frame, 0);
  dst_stride = (gint) priv->unpack_buffer.stride;

  ret = CuLaunchKernel (priv->unpack_func, DIV_UP (width, CUDA_BLOCK_X),
      DIV_UP (height, CUDA_BLOCK_Y), 1, CUDA_BLOCK_X, CUDA_BLOCK_Y, 1, 0,
      stream, args, NULL);

  if (!gst_cuda_result (ret)) {
    GST_ERROR_OBJECT (self, "Couldn't unpack source RGB");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_cuda_converter_convert_frame (GstCudaConverter * converter,
    GstVideoFrame * src_frame, GstVideoFrame * dst_frame, CUstream stream,
    gboolean * synchronized)
{
  GstCudaConverterPrivate *priv;
  const TextureFormat *format;
  CUtexObject texture[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  guint8 *dst[GST_VIDEO_MAX_COMPONENTS] = { NULL, };
  gint stride[2] = { 0, };
  gint i;
  gboolean ret = FALSE;
  CUresult cuda_ret;
  gint width, height;
  gpointer args[] = { &texture[0], &texture[1], &texture[2], &texture[3],
    &dst[0], &dst[1], &dst[2], &dst[3], &stride[0], &stride[1]
  };
  gboolean need_sync = FALSE;
  GstCudaMemory *cmem;

  g_return_val_if_fail (GST_IS_CUDA_CONVERTER (converter), FALSE);
  g_return_val_if_fail (src_frame != NULL, FALSE);
  g_return_val_if_fail (dst_frame != NULL, FALSE);

  priv = converter->priv;
  format = priv->texture_fmt;

  g_assert (format);

  cmem = (GstCudaMemory *) gst_buffer_peek_memory (src_frame->buffer, 0);
  g_return_val_if_fail (gst_is_cuda_memory (GST_MEMORY_CAST (cmem)), FALSE);

  if (!gst_cuda_context_push (converter->context)) {
    GST_ERROR_OBJECT (converter, "Couldn't push context");
    return FALSE;
  }

  if (priv->unpack_func) {
    if (!gst_cuda_converter_unpack_rgb (converter, src_frame, stream))
      goto out;

    texture[0] = priv->unpack_buffer.texture;
    if (!texture[0]) {
      GST_ERROR_OBJECT (converter, "Unpack texture is unavailable");
      goto out;
    }
  } else {
    for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (src_frame); i++) {
      if (!gst_cuda_memory_get_texture (cmem,
              i, priv->filter_mode[i], &texture[i])) {
        CUdeviceptr src;
        src = (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (src_frame, i);
        texture[i] = gst_cuda_converter_create_texture (converter,
            src, GST_VIDEO_FRAME_COMP_WIDTH (src_frame, i),
            GST_VIDEO_FRAME_COMP_HEIGHT (src_frame, i),
            GST_VIDEO_FRAME_PLANE_STRIDE (src_frame, i),
            priv->filter_mode[i], format->array_format[i], format->channels[i],
            i, stream);
        need_sync = TRUE;
      }

      if (!texture[i]) {
        GST_ERROR_OBJECT (converter, "Couldn't create texture %d", i);
        goto out;
      }
    }
  }

  width = GST_VIDEO_FRAME_WIDTH (dst_frame);
  height = GST_VIDEO_FRAME_HEIGHT (dst_frame);

  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (dst_frame); i++)
    dst[i] = GST_VIDEO_FRAME_PLANE_DATA (dst_frame, i);

  stride[0] = stride[1] = GST_VIDEO_FRAME_PLANE_STRIDE (dst_frame, 0);
  if (GST_VIDEO_FRAME_N_PLANES (dst_frame) > 1)
    stride[1] = GST_VIDEO_FRAME_PLANE_STRIDE (dst_frame, 1);

  cuda_ret = CuLaunchKernel (priv->main_func, DIV_UP (width, CUDA_BLOCK_X),
      DIV_UP (height, CUDA_BLOCK_Y), 1, CUDA_BLOCK_X, CUDA_BLOCK_Y, 1, 0,
      stream, args, NULL);

  if (!gst_cuda_result (cuda_ret)) {
    GST_ERROR_OBJECT (converter, "Couldn't convert frame");
    goto out;
  }

  if (need_sync)
    CuStreamSynchronize (stream);

  if (synchronized)
    *synchronized = need_sync;

  ret = TRUE;

out:

  gst_cuda_context_pop (NULL);
  return ret;
}
