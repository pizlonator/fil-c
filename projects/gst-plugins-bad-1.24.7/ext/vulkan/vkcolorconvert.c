/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-vulkancolorconvert
 * @title: vulkancolorconvert
 *
 * vulkancolorconvert converts between different video colorspaces.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "vkcolorconvert.h"

#include "shaders/identity.vert.h"
#include "shaders/swizzle.frag.h"
#include "shaders/swizzle_and_clobber_alpha.frag.h"
#include "shaders/yuy2_to_rgb.frag.h"
#include "shaders/ayuv_to_rgb.frag.h"
#include "shaders/nv12_to_rgb.frag.h"
#include "shaders/rgb_to_ayuv.frag.h"
#include "shaders/rgb_to_yuy2.frag.h"
#include "shaders/rgb_to_nv12.frag.h"

#include "gstvulkanelements.h"

GST_DEBUG_CATEGORY (gst_debug_vulkan_color_convert);
#define GST_CAT_DEFAULT gst_debug_vulkan_color_convert

#define N_SHADER_INFO (8*8 + 8*3*2)
static shader_info shader_infos[N_SHADER_INFO];

static void
get_rgb_format_swizzle_order (GstVideoFormat format,
    gint swizzle[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *finfo = gst_video_format_get_info (format);
  int c_i = 0, i;

  g_return_if_fail (finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB
      || format == GST_VIDEO_FORMAT_AYUV);

  for (i = 0; i < finfo->n_components; i++) {
    swizzle[c_i++] = finfo->poffset[i];
  }

  /* special case spaced RGB formats as the space does not contain a poffset
   * value and we need all four components to be valid in order to swizzle
   * correctly */
  if (format == GST_VIDEO_FORMAT_xRGB || format == GST_VIDEO_FORMAT_xBGR) {
    swizzle[c_i++] = 0;
  } else if (format == GST_VIDEO_FORMAT_RGBx || format == GST_VIDEO_FORMAT_BGRx) {
    swizzle[c_i++] = 3;
  } else {
    for (i = finfo->n_components; i < GST_VIDEO_MAX_COMPONENTS; i++) {
      swizzle[c_i++] = -1;
    }
  }
}

static void
get_vulkan_rgb_format_swizzle_order (VkFormat format, gint * swizzle,
    guint swizzle_count, guint offset)
{
  const GstVulkanFormatInfo *finfo = gst_vulkan_format_get_info (format);
  int i;

  g_return_if_fail (finfo->flags & GST_VULKAN_FORMAT_FLAG_RGB);
  g_return_if_fail (finfo->n_components <= swizzle_count);

  for (i = 0; i < finfo->n_components; i++) {
    swizzle[i] = offset + finfo->poffset[i];
  }
  for (i = finfo->n_components; i < swizzle_count; i++) {
    swizzle[i] = -1;
  }
}

/* given a swizzle index, produce an index such that:
 *
 * swizzle[idx[i]] == identity[i] where:
 * - swizzle is the original swizzle
 * - idx is the result
 * - identity = {0, 1, 2,...}
 * - unset fields are marked by -1
 */
static void
swizzle_identity_order (gint * swizzle, gint * idx)
{
  int i;

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    idx[i] = -1;
  }

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    if (swizzle[i] >= 0 && swizzle[i] < 4 && idx[swizzle[i]] == -1) {
      idx[swizzle[i]] = i;
    }
  }
}

typedef struct
{
  double dm[4][4];
} Matrix4;

static void
matrix_debug (const Matrix4 * s)
{
  GST_DEBUG ("[%f %f %f %f]", s->dm[0][0], s->dm[0][1], s->dm[0][2],
      s->dm[0][3]);
  GST_DEBUG ("[%f %f %f %f]", s->dm[1][0], s->dm[1][1], s->dm[1][2],
      s->dm[1][3]);
  GST_DEBUG ("[%f %f %f %f]", s->dm[2][0], s->dm[2][1], s->dm[2][2],
      s->dm[2][3]);
  GST_DEBUG ("[%f %f %f %f]", s->dm[3][0], s->dm[3][1], s->dm[3][2],
      s->dm[3][3]);
}

static void
matrix_to_float (const Matrix4 * m, float *ret)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      ret[j * 4 + i] = m->dm[i][j];
    }
  }
}

static void
matrix_set_identity (Matrix4 * m)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m->dm[i][j] = (i == j);
    }
  }
}

static void
matrix_copy (Matrix4 * d, const Matrix4 * s)
{
  gint i, j;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      d->dm[i][j] = s->dm[i][j];
}

/* Perform 4x4 matrix multiplication:
 *  - @dst@ = @a@ * @b@
 *  - @dst@ may be a pointer to @a@ andor @b@
 */
static void
matrix_multiply (Matrix4 * dst, Matrix4 * a, Matrix4 * b)
{
  Matrix4 tmp;
  int i, j, k;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      double x = 0;
      for (k = 0; k < 4; k++) {
        x += a->dm[i][k] * b->dm[k][j];
      }
      tmp.dm[i][j] = x;
    }
  }
  matrix_copy (dst, &tmp);
}

#if 0
static void
matrix_invert (Matrix4 * d, Matrix4 * s)
{
  Matrix4 tmp;
  int i, j;
  double det;

  matrix_set_identity (&tmp);
  for (j = 0; j < 3; j++) {
    for (i = 0; i < 3; i++) {
      tmp.dm[j][i] =
          s->dm[(i + 1) % 3][(j + 1) % 3] * s->dm[(i + 2) % 3][(j + 2) % 3] -
          s->dm[(i + 1) % 3][(j + 2) % 3] * s->dm[(i + 2) % 3][(j + 1) % 3];
    }
  }
  det =
      tmp.dm[0][0] * s->dm[0][0] + tmp.dm[0][1] * s->dm[1][0] +
      tmp.dm[0][2] * s->dm[2][0];
  for (j = 0; j < 3; j++) {
    for (i = 0; i < 3; i++) {
      tmp.dm[i][j] /= det;
    }
  }
  matrix_copy (d, &tmp);
}
#endif
static void
matrix_offset_components (Matrix4 * m, double a1, double a2, double a3)
{
  Matrix4 a;

  matrix_set_identity (&a);
  a.dm[0][3] = a1;
  a.dm[1][3] = a2;
  a.dm[2][3] = a3;
  matrix_debug (&a);
  matrix_multiply (m, &a, m);
}

static void
matrix_scale_components (Matrix4 * m, double a1, double a2, double a3)
{
  Matrix4 a;

  matrix_set_identity (&a);
  a.dm[0][0] = a1;
  a.dm[1][1] = a2;
  a.dm[2][2] = a3;
  matrix_multiply (m, &a, m);
}

static void
matrix_YCbCr_to_RGB (Matrix4 * m, double Kr, double Kb)
{
  double Kg = 1.0 - Kr - Kb;
  Matrix4 k = {
    {
          {1., 0., 2 * (1 - Kr), 0.},
          {1., -2 * Kb * (1 - Kb) / Kg, -2 * Kr * (1 - Kr) / Kg, 0.},
          {1., 2 * (1 - Kb), 0., 0.},
          {0., 0., 0., 1.},
        }
  };

  matrix_multiply (m, &k, m);
}

typedef struct
{
  GstVideoInfo in_info;
  GstVideoInfo out_info;

  Matrix4 to_RGB_matrix;
  Matrix4 to_YUV_matrix;
  Matrix4 convert_matrix;
} ConvertInfo;

static void
convert_to_RGB (ConvertInfo * conv, Matrix4 * m)
{
  GstVideoInfo *info = &conv->in_info;

  {
    const GstVideoFormatInfo *uinfo;
    gint offset[4], scale[4], depth[4];
    int i;

    uinfo = gst_video_format_get_info (GST_VIDEO_INFO_FORMAT (info));

    /* bring color components to [0..1.0] range */
    gst_video_color_range_offsets (info->colorimetry.range, uinfo, offset,
        scale);

    for (i = 0; i < uinfo->n_components; i++)
      depth[i] = (1 << uinfo->depth[i]) - 1;

    matrix_offset_components (m, -offset[0] / (float) depth[0],
        -offset[1] / (float) depth[1], -offset[2] / (float) depth[2]);
    matrix_scale_components (m, depth[0] / ((float) scale[0]),
        depth[1] / ((float) scale[1]), depth[2] / ((float) scale[2]));
    GST_DEBUG ("to RGB scale/offset matrix");
    matrix_debug (m);
  }

  if (GST_VIDEO_INFO_IS_YUV (info)) {
    gdouble Kr, Kb;

    if (gst_video_color_matrix_get_Kr_Kb (info->colorimetry.matrix, &Kr, &Kb))
      matrix_YCbCr_to_RGB (m, Kr, Kb);
    GST_DEBUG ("to RGB matrix");
    matrix_debug (m);
  }
}

static void
matrix_RGB_to_YCbCr (Matrix4 * m, double Kr, double Kb)
{
  double Kg = 1.0 - Kr - Kb;
  Matrix4 k;
  double x;

  k.dm[0][0] = Kr;
  k.dm[0][1] = Kg;
  k.dm[0][2] = Kb;
  k.dm[0][3] = 0;

  x = 1 / (2 * (1 - Kb));
  k.dm[1][0] = -x * Kr;
  k.dm[1][1] = -x * Kg;
  k.dm[1][2] = x * (1 - Kb);
  k.dm[1][3] = 0;

  x = 1 / (2 * (1 - Kr));
  k.dm[2][0] = x * (1 - Kr);
  k.dm[2][1] = -x * Kg;
  k.dm[2][2] = -x * Kb;
  k.dm[2][3] = 0;

  k.dm[3][0] = 0;
  k.dm[3][1] = 0;
  k.dm[3][2] = 0;
  k.dm[3][3] = 1;

  matrix_multiply (m, &k, m);
}

static void
convert_to_YUV (ConvertInfo * conv, Matrix4 * m)
{
  GstVideoInfo *info = &conv->out_info;

  if (GST_VIDEO_INFO_IS_YUV (info)) {
    gdouble Kr, Kb;

    if (gst_video_color_matrix_get_Kr_Kb (info->colorimetry.matrix, &Kr, &Kb))
      matrix_RGB_to_YCbCr (m, Kr, Kb);
    GST_DEBUG ("to YUV matrix");
    matrix_debug (m);
  }

  {
    const GstVideoFormatInfo *uinfo;
    gint offset[4], scale[4], depth[4];
    int i;

    uinfo = gst_video_format_get_info (GST_VIDEO_INFO_FORMAT (info));

    /* bring color components to nominal range */
    gst_video_color_range_offsets (info->colorimetry.range, uinfo, offset,
        scale);

    for (i = 0; i < uinfo->n_components; i++)
      depth[i] = (1 << uinfo->depth[i]) - 1;

    matrix_scale_components (m, scale[0] / (float) depth[0],
        scale[1] / (float) depth[1], scale[2] / (float) depth[2]);
    matrix_offset_components (m, offset[0] / (float) depth[0],
        offset[1] / (float) depth[1], offset[2] / (float) depth[2]);
    GST_DEBUG ("to YUV scale/offset matrix");
    matrix_debug (m);
  }
}

#if 0
static void
matrix_RGB_to_XYZ (Matrix4 * dst, double Rx, double Ry, double Gx,
    double Gy, double Bx, double By, double Wx, double Wy)
{
  Matrix4 m, im;
  double sx, sy, sz;
  double wx, wy, wz;

  matrix_set_identity (&m);

  m.dm[0][0] = Rx;
  m.dm[1][0] = Ry;
  m.dm[2][0] = (1.0 - Rx - Ry);
  m.dm[0][1] = Gx;
  m.dm[1][1] = Gy;
  m.dm[2][1] = (1.0 - Gx - Gy);
  m.dm[0][2] = Bx;
  m.dm[1][2] = By;
  m.dm[2][2] = (1.0 - Bx - By);

  matrix_invert (&im, &m);

  wx = Wx / Wy;
  wy = 1.0;
  wz = (1.0 - Wx - Wy) / Wy;

  sx = im.dm[0][0] * wx + im.dm[0][1] * wy + im.dm[0][2] * wz;
  sy = im.dm[1][0] * wx + im.dm[1][1] * wy + im.dm[1][2] * wz;
  sz = im.dm[2][0] * wx + im.dm[2][1] * wy + im.dm[2][2] * wz;

  m.dm[0][0] *= sx;
  m.dm[1][0] *= sx;
  m.dm[2][0] *= sx;
  m.dm[0][1] *= sy;
  m.dm[1][1] *= sy;
  m.dm[2][1] *= sy;
  m.dm[0][2] *= sz;
  m.dm[1][2] *= sz;
  m.dm[2][2] *= sz;

  matrix_copy (dst, &m);
}

static void
convert_primaries (ConvertInfo * conv)
{
  gboolean same_matrix, same_primaries;
  Matrix4 p1, p2;

  same_matrix =
      conv->in_info.colorimetry.matrix == conv->out_info.colorimetry.matrix;
  same_primaries =
      conv->in_info.colorimetry.primaries ==
      conv->out_info.colorimetry.primaries;

  GST_DEBUG ("matrix %d -> %d (%d)", conv->in_info.colorimetry.matrix,
      conv->out_info.colorimetry.matrix, same_matrix);
  GST_DEBUG ("primaries %d -> %d (%d)", conv->in_info.colorimetry.primaries,
      conv->out_info.colorimetry.primaries, same_primaries);

  matrix_set_identity (&conv->convert_matrix);

  if (!same_primaries) {
    const GstVideoColorPrimariesInfo *pi;

    pi = gst_video_color_primaries_get_info (conv->in_info.colorimetry.
        primaries);
    matrix_RGB_to_XYZ (&p1, pi->Rx, pi->Ry, pi->Gx, pi->Gy, pi->Bx, pi->By,
        pi->Wx, pi->Wy);
    GST_DEBUG ("to XYZ matrix");
    matrix_debug (&p1);
    GST_DEBUG ("current matrix");
    matrix_multiply (&conv->convert_matrix, &conv->convert_matrix, &p1);
    matrix_debug (&conv->convert_matrix);

    pi = gst_video_color_primaries_get_info (conv->out_info.colorimetry.
        primaries);
    matrix_RGB_to_XYZ (&p2, pi->Rx, pi->Ry, pi->Gx, pi->Gy, pi->Bx, pi->By,
        pi->Wx, pi->Wy);
    matrix_invert (&p2, &p2);
    GST_DEBUG ("to RGB matrix");
    matrix_debug (&p2);
    matrix_multiply (&conv->convert_matrix, &conv->convert_matrix, &p2);
    GST_DEBUG ("current matrix");
    matrix_debug (&conv->convert_matrix);
  }
}
#endif
static ConvertInfo *
convert_info_new (GstVideoInfo * in_info, GstVideoInfo * out_info)
{
  ConvertInfo *conv = g_new0 (ConvertInfo, 1);

  matrix_set_identity (&conv->to_RGB_matrix);
  matrix_set_identity (&conv->convert_matrix);
  matrix_set_identity (&conv->to_YUV_matrix);

  memcpy (&conv->in_info, in_info, sizeof (*in_info));
  memcpy (&conv->out_info, out_info, sizeof (*out_info));

  convert_to_RGB (conv, &conv->to_RGB_matrix);
  /* by default videoconvert does not convert primaries
     convert_primaries (conv); */
  convert_to_YUV (conv, &conv->to_YUV_matrix);

  return conv;
}

static void
video_format_to_reorder (GstVideoFormat v_format, gint * reorder,
    gboolean input)
{
  switch (v_format) {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_AYUV:
      get_rgb_format_swizzle_order (v_format, reorder);
      break;
    case GST_VIDEO_FORMAT_UYVY:
      reorder[0] = 1;
      reorder[1] = 0;
      reorder[2] = input ? 3 : 2;
      reorder[3] = 0;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      reorder[0] = 0;
      reorder[1] = 1;
      reorder[2] = 0;
      reorder[3] = input ? 3 : 2;
      break;
    case GST_VIDEO_FORMAT_NV12:
      reorder[0] = 0;
      reorder[1] = 1;
      reorder[2] = 2;
      reorder[3] = 0;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  GST_TRACE ("swizzle: %u, %u, %u, %u", reorder[0], reorder[1], reorder[2],
      reorder[3]);
}

static guint
finfo_get_plane_n_components (const GstVideoFormatInfo * finfo, guint plane)
{
  guint n_components = 0, i;

  switch (finfo->format) {
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xBGR:
      /* fixup spaced RGB formats as we treat the space as a normal alpha
       * component */
      return plane == 0 ? 4 : 0;
    default:
      break;
  }

  for (i = 0; i < finfo->n_components; i++) {
    if (finfo->plane[i] == plane)
      n_components++;
  }

  return n_components;
}

static void
get_vulkan_format_swizzle_order (GstVideoFormat v_format,
    VkFormat vk_format[GST_VIDEO_MAX_PLANES],
    gint swizzle[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *finfo;
  int i, prev_in_i = 0;

  finfo = gst_video_format_get_info (v_format);
  for (i = 0; i < finfo->n_planes; i++) {
    guint plane_components = finfo_get_plane_n_components (finfo, i);
    get_vulkan_rgb_format_swizzle_order (vk_format[i],
        &swizzle[prev_in_i], plane_components, prev_in_i);
    prev_in_i += plane_components;
  }

  if (v_format == GST_VIDEO_FORMAT_YUY2 || v_format == GST_VIDEO_FORMAT_UYVY) {
    /* Fixup these packed YUV formats as we use a two component format for
     * a 4-component pixel and access two samples in the shader */
    g_assert (swizzle[0] == 0);
    g_assert (swizzle[1] == 1);
    swizzle[2] = 2;
    swizzle[3] = 3;
  }

  GST_TRACE ("%s: %i, %i, %i, %i", finfo->name, swizzle[0], swizzle[1],
      swizzle[2], swizzle[3]);
}

static void
calculate_reorder_indexes (GstVideoFormat in_format,
    GstVulkanImageView * in_views[GST_VIDEO_MAX_COMPONENTS],
    GstVideoFormat out_format,
    GstVulkanImageView * out_views[GST_VIDEO_MAX_COMPONENTS],
    int ret_in[GST_VIDEO_MAX_COMPONENTS], int ret_out[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *in_finfo, *out_finfo;
  VkFormat in_vk_formats[GST_VIDEO_MAX_COMPONENTS];
  VkFormat out_vk_formats[GST_VIDEO_MAX_COMPONENTS];
  int in_vk_order[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  int in_reorder[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  int out_vk_order[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  int out_reorder[GST_VIDEO_MAX_COMPONENTS] = { 0, };
  int tmp[GST_VIDEO_MAX_PLANES] = { 0, };
  int i;

  in_finfo = gst_video_format_get_info (in_format);
  out_finfo = gst_video_format_get_info (out_format);

  for (i = 0; i < in_finfo->n_planes; i++)
    in_vk_formats[i] = in_views[i]->image->create_info.format;
  for (i = 0; i < out_finfo->n_planes; i++)
    out_vk_formats[i] = out_views[i]->image->create_info.format;

  get_vulkan_format_swizzle_order (in_format, in_vk_formats, in_vk_order);
  video_format_to_reorder (in_format, in_reorder, TRUE);

  video_format_to_reorder (out_format, out_reorder, FALSE);
  get_vulkan_format_swizzle_order (out_format, out_vk_formats, out_vk_order);

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
    tmp[i] = out_vk_order[out_reorder[i]];
  /* find the identity order for RGBA->$format */
  GST_TRACE ("pre-invert: %u, %u, %u, %u", tmp[0], tmp[1], tmp[2], tmp[3]);
  if (out_format == GST_VIDEO_FORMAT_YUY2
      || out_format == GST_VIDEO_FORMAT_UYVY) {
    for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
      ret_out[i] = tmp[i];
  } else {
    swizzle_identity_order (tmp, ret_out);
  }

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
    ret_in[i] = in_reorder[in_vk_order[i]];
  GST_TRACE ("in reorder: %u, %u, %u, %u", ret_in[0], ret_in[1], ret_in[2],
      ret_in[3]);
  GST_TRACE ("out reorder: %u, %u, %u, %u", ret_out[0], ret_out[1], ret_out[2],
      ret_out[3]);
}

struct RGBUpdateData
{
  int in_reorder[4];
  int out_reorder[4];
};

static GstMemory *
swizzle_rgb_create_uniform_memory (GstVulkanColorConvert * conv,
    shader_info * sinfo, GstVulkanImageView ** in_views,
    GstVulkanImageView ** out_views)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);

  if (sinfo->user_data) {
    return gst_memory_ref (sinfo->user_data);
  } else {
    struct RGBUpdateData data = { 0, };
    GstMapInfo map_info;
    GstMemory *uniforms;

    uniforms =
        gst_vulkan_buffer_memory_alloc (vfilter->device,
        sizeof (struct RGBUpdateData),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    calculate_reorder_indexes (GST_VIDEO_INFO_FORMAT (&conv->quad->in_info),
        in_views, GST_VIDEO_INFO_FORMAT (&conv->quad->out_info),
        out_views, data.in_reorder, data.out_reorder);

    if (!gst_memory_map (uniforms, &map_info, GST_MAP_WRITE)) {
      gst_memory_unref (uniforms);
      return NULL;
    }
    memcpy (map_info.data, &data, sizeof (data));
    gst_memory_unmap (uniforms, &map_info);

    sinfo->user_data = gst_memory_ref (uniforms);
    return uniforms;
  }
}

struct ColorMatrices
{
  float to_RGB[16];
  float primaries[16];
  float to_YUV[16];
};

struct YUVUpdateData
{
  int in_reorder[4];
  int out_reorder[4];
  int tex_size[2];
  /* each member is aligned on 4x previous component size boundaries */
  int _padding[2];
  struct ColorMatrices matrices;
};

static GstMemory *
yuv_to_rgb_create_uniform_memory (GstVulkanColorConvert * conv,
    shader_info * sinfo, GstVulkanImageView ** in_views,
    GstVulkanImageView ** out_views)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);

  if (sinfo->user_data) {
    return gst_memory_ref (sinfo->user_data);
  } else {
    struct YUVUpdateData data;
    ConvertInfo *conv_info;
    GstMapInfo map_info;
    GstMemory *uniforms;

    uniforms =
        gst_vulkan_buffer_memory_alloc (vfilter->device,
        sizeof (struct YUVUpdateData),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    calculate_reorder_indexes (GST_VIDEO_INFO_FORMAT (&conv->quad->in_info),
        in_views, GST_VIDEO_INFO_FORMAT (&conv->quad->out_info),
        out_views, data.in_reorder, data.out_reorder);

    conv_info = convert_info_new (&conv->quad->in_info, &conv->quad->out_info);
    matrix_to_float (&conv_info->to_RGB_matrix, data.matrices.to_RGB);
    matrix_to_float (&conv_info->convert_matrix, data.matrices.primaries);
    matrix_to_float (&conv_info->to_YUV_matrix, data.matrices.to_YUV);
    /* FIXME: keep this around */
    g_free (conv_info);

    data.tex_size[0] = GST_VIDEO_INFO_WIDTH (&conv->quad->in_info);
    data.tex_size[1] = GST_VIDEO_INFO_HEIGHT (&conv->quad->in_info);

    if (!gst_memory_map (uniforms, &map_info, GST_MAP_WRITE)) {
      gst_memory_unref (uniforms);
      return NULL;
    }
    memcpy (map_info.data, &data, sizeof (data));
    gst_memory_unmap (uniforms, &map_info);

    sinfo->user_data = gst_memory_ref (uniforms);
    return uniforms;
  }

}

static void
unref_memory_if_set (shader_info * sinfo)
{
  if (sinfo->user_data)
    gst_memory_unref (sinfo->user_data);
  sinfo->user_data = NULL;
}

static gboolean gst_vulkan_color_convert_start (GstBaseTransform * bt);
static gboolean gst_vulkan_color_convert_stop (GstBaseTransform * bt);

static GstCaps *gst_vulkan_color_convert_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstFlowReturn gst_vulkan_color_convert_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_vulkan_color_convert_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);

static GstStaticPadTemplate gst_vulkan_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            "{ BGRA, RGBA, ABGR, ARGB, BGRx, RGBx, xBGR, xRGB, AYUV, YUY2, NV12 }")));

static GstStaticPadTemplate gst_vulkan_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            "{ BGRA, RGBA, ABGR, ARGB, BGRx, RGBx, xBGR, xRGB, AYUV, YUY2, NV12 }")));

enum
{
  PROP_0,
};

enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* static guint gst_vulkan_color_convert_signals[LAST_SIGNAL] = { 0 }; */

#define gst_vulkan_color_convert_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanColorConvert, gst_vulkan_color_convert,
    GST_TYPE_VULKAN_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_color_convert,
        "vulkancolorconvert", 0, "Vulkan Color Convert"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkancolorconvert, "vulkancolorconvert",
    GST_RANK_NONE, GST_TYPE_VULKAN_COLOR_CONVERT, vulkan_element_init (plugin));

struct yuv_info
{
  GstVideoFormat format;
  gchar *from_frag;
  gsize from_frag_size;
  gchar *to_frag;
  gsize to_frag_size;
};

static void
fill_shader_info (void)
{
  GstVideoFormat rgbs[] = { GST_VIDEO_FORMAT_RGBA, GST_VIDEO_FORMAT_ARGB,
    GST_VIDEO_FORMAT_BGRA, GST_VIDEO_FORMAT_ABGR, GST_VIDEO_FORMAT_RGBx,
    GST_VIDEO_FORMAT_xRGB, GST_VIDEO_FORMAT_BGRx, GST_VIDEO_FORMAT_xBGR
  };
  struct yuv_info yuvs[] = {
    {GST_VIDEO_FORMAT_AYUV, ayuv_to_rgb_frag, ayuv_to_rgb_frag_size,
        rgb_to_ayuv_frag, rgb_to_ayuv_frag_size},
    {GST_VIDEO_FORMAT_YUY2, yuy2_to_rgb_frag, yuy2_to_rgb_frag_size,
        rgb_to_yuy2_frag, rgb_to_yuy2_frag_size},
/*    {GST_VIDEO_FORMAT_UYVY, yuy2_to_rgb_frag, yuy2_to_rgb_frag_size,
        rgb_to_yuy2_frag, rgb_to_yuy2_frag_size},*/
    {GST_VIDEO_FORMAT_NV12, nv12_to_rgb_frag, nv12_to_rgb_frag_size,
        rgb_to_nv12_frag, rgb_to_nv12_frag_size},
  };
  guint info_i = 0;
  guint i, j;

  /* standard RGB with alpha conversion all components are copied */
  /* *INDENT-OFF* */
  for (i = 0; i < G_N_ELEMENTS (rgbs); i++) {
    const GstVideoFormatInfo *from_finfo = gst_video_format_get_info (rgbs[i]);

    for (j = 0; j < G_N_ELEMENTS (rgbs); j++) {
      const GstVideoFormatInfo *to_finfo = gst_video_format_get_info (rgbs[j]);
      gboolean clobber_alpha = FALSE;

      GST_TRACE ("Initializing info for %s -> %s", from_finfo->name, to_finfo->name);

      /* copying to an RGBx variant means we can store whatever we like in the 'x'
       * component we choose to copy the alpha component like a standard RGBA->RGBA
       * swizzle.
       * Copying from an rgbx to a rgba format means we need to reset the
       * alpha value */
      clobber_alpha = !GST_VIDEO_FORMAT_INFO_HAS_ALPHA (from_finfo) && GST_VIDEO_FORMAT_INFO_HAS_ALPHA (to_finfo);
      shader_infos[info_i++] = (shader_info) {
          .from = rgbs[i],
          .to = rgbs[j],
          .cmd_create_uniform = swizzle_rgb_create_uniform_memory,
          .frag_code = clobber_alpha ? swizzle_and_clobber_alpha_frag : swizzle_frag,
          .frag_size = clobber_alpha ? swizzle_and_clobber_alpha_frag_size : swizzle_frag_size,
          .uniform_size = sizeof (struct RGBUpdateData),
          .notify = (GDestroyNotify) unref_memory_if_set,
          .user_data = NULL,
      };
    }

    for (j = 0; j < G_N_ELEMENTS (yuvs); j++) {
      const GstVideoFormatInfo *to_finfo = gst_video_format_get_info (yuvs[j].format);
      GST_TRACE ("Initializing info for %s -> %s", from_finfo->name, to_finfo->name);
      shader_infos[info_i++] = (shader_info) {
          .from = rgbs[i],
          .to = yuvs[j].format,
          .cmd_create_uniform = yuv_to_rgb_create_uniform_memory,
          .frag_code = yuvs[j].to_frag,
          .frag_size = yuvs[j].to_frag_size,
          .uniform_size = sizeof(struct YUVUpdateData),
          .notify = (GDestroyNotify) unref_memory_if_set,
          .user_data = NULL,
      };
      GST_TRACE ("Initializing info for %s -> %s", to_finfo->name, from_finfo->name);
      shader_infos[info_i++] = (shader_info) {
          .from = yuvs[j].format,
          .to = rgbs[i],
          .cmd_create_uniform = yuv_to_rgb_create_uniform_memory,
          .frag_code = yuvs[j].from_frag,
          .frag_size = yuvs[j].from_frag_size,
          .uniform_size = sizeof(struct YUVUpdateData),
          .notify = (GDestroyNotify) unref_memory_if_set,
          .user_data = NULL,
      };
    }
  }
  /* *INDENT-ON* */
  GST_TRACE ("initialized %u formats", info_i);

  g_assert (info_i == N_SHADER_INFO);
}

static void
gst_vulkan_color_convert_class_init (GstVulkanColorConvertClass * klass)
{
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gst_element_class_set_metadata (gstelement_class, "Vulkan Color Convert",
      "Filter/Video/Convert", "A Vulkan Color Convert",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_src_template);

  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_color_convert_start);
  gstbasetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_vulkan_color_convert_stop);
  gstbasetransform_class->transform_caps =
      gst_vulkan_color_convert_transform_caps;
  gstbasetransform_class->set_caps = gst_vulkan_color_convert_set_caps;
  gstbasetransform_class->transform = gst_vulkan_color_convert_transform;

  fill_shader_info ();
}

static void
gst_vulkan_color_convert_init (GstVulkanColorConvert * conv)
{
}

static void
_init_value_string_list (GValue * list, ...)
{
  GValue item = G_VALUE_INIT;
  gchar *str;
  va_list args;

  g_value_init (list, GST_TYPE_LIST);

  va_start (args, list);
  while ((str = va_arg (args, gchar *))) {
    g_value_init (&item, G_TYPE_STRING);
    g_value_set_string (&item, str);

    gst_value_list_append_value (list, &item);
    g_value_unset (&item);
  }
  va_end (args);
}

static void
_append_value_string_list (GValue * list, ...)
{
  GValue item = G_VALUE_INIT;
  gchar *str;
  va_list args;

  va_start (args, list);
  while ((str = va_arg (args, gchar *))) {
    g_value_init (&item, G_TYPE_STRING);
    g_value_set_string (&item, str);

    gst_value_list_append_value (list, &item);
    g_value_unset (&item);
  }
  va_end (args);
}

static void
_init_supported_formats (GstVulkanDevice * device, gboolean output,
    GValue * supported_formats)
{
  /* Assume if device == NULL that we don't have a Vulkan device and can
   * do the conversion */

  /* Always supported input and output formats */
  _init_value_string_list (supported_formats, "RGBA", "RGB", "RGBx", "BGR",
      "BGRx", "BGRA", "xRGB", "xBGR", "ARGB", "ABGR", NULL);

  _append_value_string_list (supported_formats, "AYUV", "YUY2", /*"UYVY", */
      "NV12", NULL);
}

/* copies the given caps */
static GstCaps *
gst_vulkan_color_convert_transform_format_info (GstVulkanDevice * device,
    gboolean output, GstCaps * caps)
{
  GstStructure *st;
  GstCapsFeatures *f;
  gint i, n;
  GstCaps *res;
  GValue supported_formats = G_VALUE_INIT;
  GValue rgb_formats = G_VALUE_INIT;
  GValue supported_rgb_formats = G_VALUE_INIT;

  /* There are effectively two modes here with the RGB/YUV transition:
   * 1. There is a RGB-like format as input and we can transform to YUV or,
   * 2. No RGB-like format as input so we can only transform to RGB-like formats
   *
   * We also filter down the list of formats depending on what the OpenGL
   * device supports (when provided).
   */

  _init_value_string_list (&rgb_formats, "RGBA", "ARGB", "BGRA", "ABGR", "RGBx",
      "xRGB", "BGRx", "xBGR", "RGB", "BGR", "ARGB64", NULL);
  _init_supported_formats (device, output, &supported_formats);
  gst_value_intersect (&supported_rgb_formats, &rgb_formats,
      &supported_formats);

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    const GValue *format;

    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    format = gst_structure_get_value (st, "format");
    st = gst_structure_copy (st);
    if (GST_VALUE_HOLDS_LIST (format)) {
      gboolean have_rgb_formats = FALSE;
      GValue passthrough_formats = G_VALUE_INIT;
      gint j, len;

      g_value_init (&passthrough_formats, GST_TYPE_LIST);
      len = gst_value_list_get_size (format);
      for (j = 0; j < len; j++) {
        const GValue *val;

        val = gst_value_list_get_value (format, j);
        if (G_VALUE_HOLDS_STRING (val)) {
          const gchar *format_str = g_value_get_string (val);
          GstVideoFormat v_format = gst_video_format_from_string (format_str);
          const GstVideoFormatInfo *t_info =
              gst_video_format_get_info (v_format);
          if (GST_VIDEO_FORMAT_INFO_FLAGS (t_info) & (GST_VIDEO_FORMAT_FLAG_YUV
                  | GST_VIDEO_FORMAT_FLAG_GRAY)) {
            gst_value_list_append_value (&passthrough_formats, val);
          } else if (GST_VIDEO_FORMAT_INFO_FLAGS (t_info) &
              GST_VIDEO_FORMAT_FLAG_RGB) {
            have_rgb_formats = TRUE;
            break;
          }
        }
      }
      if (have_rgb_formats) {
        gst_structure_set_value (st, "format", &supported_formats);
      } else {
        /* add passthrough structure, then the rgb conversion structure */
        gst_structure_set_value (st, "format", &passthrough_formats);
        gst_caps_append_structure_full (res, gst_structure_copy (st),
            gst_caps_features_copy (f));
        gst_structure_set_value (st, "format", &supported_rgb_formats);
      }
      g_value_unset (&passthrough_formats);
    } else if (G_VALUE_HOLDS_STRING (format)) {
      const gchar *format_str = g_value_get_string (format);
      GstVideoFormat v_format = gst_video_format_from_string (format_str);
      const GstVideoFormatInfo *t_info = gst_video_format_get_info (v_format);
      if (GST_VIDEO_FORMAT_INFO_FLAGS (t_info) & (GST_VIDEO_FORMAT_FLAG_YUV |
              GST_VIDEO_FORMAT_FLAG_GRAY)) {
        /* add passthrough structure, then the rgb conversion structure */
        gst_structure_set_value (st, "format", format);
        gst_caps_append_structure_full (res, gst_structure_copy (st),
            gst_caps_features_copy (f));
        gst_structure_set_value (st, "format", &supported_rgb_formats);
      } else {                  /* RGB */
        gst_structure_set_value (st, "format", &supported_formats);
      }
    }
    gst_structure_remove_fields (st, "colorimetry", "chroma-site", NULL);

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  g_value_unset (&supported_formats);
  g_value_unset (&rgb_formats);
  g_value_unset (&supported_rgb_formats);

  return res;
}

static GstCaps *
gst_vulkan_color_convert_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);

  caps = gst_vulkan_color_convert_transform_format_info (vfilter->device,
      direction == GST_PAD_SRC, caps);

  if (filter) {
    GstCaps *tmp;

    tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = tmp;
  }

  return caps;
}

static gboolean
gst_vulkan_color_convert_start (GstBaseTransform * bt)
{
  GstVulkanColorConvert *conv = GST_VULKAN_COLOR_CONVERT (bt);
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->start (bt))
    return FALSE;

  conv->quad = gst_vulkan_full_screen_quad_new (vfilter->queue);

  return TRUE;
}

static gboolean
vulkan_color_convert_can_passthrough_info (const GstVideoInfo * in,
    const GstVideoInfo * out)
{
  gint i;

  if (GST_VIDEO_INFO_FORMAT (in) != GST_VIDEO_INFO_FORMAT (out))
    return FALSE;
  if (GST_VIDEO_INFO_WIDTH (in) != GST_VIDEO_INFO_WIDTH (out))
    return FALSE;
  if (GST_VIDEO_INFO_HEIGHT (in) != GST_VIDEO_INFO_HEIGHT (out))
    return FALSE;
  if (GST_VIDEO_INFO_SIZE (in) != GST_VIDEO_INFO_SIZE (out))
    return FALSE;

  for (i = 0; i < in->finfo->n_planes; i++) {
    if (in->stride[i] != out->stride[i])
      return FALSE;
    if (in->offset[i] != out->offset[i])
      return FALSE;
  }

  if (!gst_video_colorimetry_is_equal (&in->colorimetry, &out->colorimetry))
    return FALSE;
  if (in->chroma_site != out->chroma_site)
    return FALSE;

  return TRUE;
}

static gboolean
gst_vulkan_color_convert_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanColorConvert *conv = GST_VULKAN_COLOR_CONVERT (bt);
  GstVulkanHandle *vert, *frag;
  gboolean passthrough;
  int i;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, in_caps,
          out_caps))
    return FALSE;

  passthrough =
      vulkan_color_convert_can_passthrough_info (&vfilter->in_info,
      &vfilter->out_info);
  gst_base_transform_set_passthrough (bt, passthrough);

  if (!gst_vulkan_full_screen_quad_set_info (conv->quad, &vfilter->in_info,
          &vfilter->out_info))
    return FALSE;

  if (conv->current_shader) {
    conv->current_shader->notify (conv->current_shader);
    conv->current_shader = NULL;
  }

  if (passthrough) {
    conv->current_shader = NULL;
    return TRUE;
  }

  for (i = 0; i < G_N_ELEMENTS (shader_infos); i++) {
    if (shader_infos[i].from != GST_VIDEO_INFO_FORMAT (&vfilter->in_info))
      continue;
    if (shader_infos[i].to != GST_VIDEO_INFO_FORMAT (&vfilter->out_info))
      continue;

    GST_INFO_OBJECT (conv,
        "Found compatible conversion information from %s to %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&vfilter->in_info)),
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT
            (&vfilter->out_info)));
    conv->current_shader = &shader_infos[i];
  }

  if (!conv->current_shader) {
    GST_ERROR_OBJECT (conv, "Could not find a conversion info for the "
        "requested formats");
    return FALSE;
  }

  if (!(vert =
          gst_vulkan_create_shader (vfilter->device, identity_vert,
              identity_vert_size, NULL))) {
    return FALSE;
  }
  if (!(frag =
          gst_vulkan_create_shader (vfilter->device,
              conv->current_shader->frag_code, conv->current_shader->frag_size,
              NULL))) {
    gst_vulkan_handle_unref (vert);
    return FALSE;
  }

  if (!gst_vulkan_full_screen_quad_set_shaders (conv->quad, vert, frag)) {
    gst_vulkan_handle_unref (vert);
    gst_vulkan_handle_unref (frag);
    return FALSE;
  }

  gst_vulkan_handle_unref (vert);
  gst_vulkan_handle_unref (frag);

  return TRUE;
}

static gboolean
gst_vulkan_color_convert_stop (GstBaseTransform * bt)
{
  GstVulkanColorConvert *conv = GST_VULKAN_COLOR_CONVERT (bt);

  if (conv->current_shader) {
    conv->current_shader->notify (conv->current_shader);
    conv->current_shader = NULL;
  }

  gst_clear_object (&conv->quad);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static GstFlowReturn
gst_vulkan_color_convert_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVulkanColorConvert *conv = GST_VULKAN_COLOR_CONVERT (bt);
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanImageView *in_img_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageMemory *render_img_mems[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageView *render_img_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageMemory *out_img_mems[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstBuffer *render_buf = NULL;
  GstVulkanFence *fence = NULL;
  GstVulkanCommandBuffer *cmd_buf;
  GError *error = NULL;
  VkResult err;
  int i;
  guint in_n_mems, out_n_mems;

  fence = gst_vulkan_device_create_fence (vfilter->device, &error);
  if (!fence)
    goto error;

  if (!gst_vulkan_full_screen_quad_set_input_buffer (conv->quad, inbuf, &error))
    goto error;

  in_n_mems = gst_buffer_n_memory (inbuf);
  for (i = 0; i < in_n_mems; i++) {
    GstMemory *img_mem = gst_buffer_peek_memory (inbuf, i);
    if (!gst_is_vulkan_image_memory (img_mem)) {
      g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
          "Input memory must be a GstVulkanImageMemory");
      goto error;
    }
    in_img_views[i] =
        gst_vulkan_get_or_create_image_view ((GstVulkanImageMemory *) img_mem);
    gst_vulkan_trash_list_add (conv->quad->trash_list,
        gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) in_img_views[i]));
  }

  {
    gboolean need_render_buf = FALSE;

    out_n_mems = gst_buffer_n_memory (outbuf);
    for (i = 0; i < out_n_mems; i++) {
      GstMemory *mem = gst_buffer_peek_memory (outbuf, i);
      if (!gst_is_vulkan_image_memory (mem)) {
        g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
            "Output memory must be a GstVulkanImageMemory");
        goto error;
      }
      out_img_mems[i] = (GstVulkanImageMemory *) mem;

      if (GST_VIDEO_INFO_WIDTH (&conv->quad->out_info) ==
          GST_VIDEO_INFO_COMP_WIDTH (&conv->quad->out_info, i)
          && GST_VIDEO_INFO_HEIGHT (&conv->quad->out_info) ==
          GST_VIDEO_INFO_COMP_HEIGHT (&conv->quad->out_info, i)) {
        render_img_mems[i] = out_img_mems[i];
        GST_LOG_OBJECT (conv, "using original output memory %p for plane %u",
            out_img_mems[i], i);
      } else {
        /* we need a scratch buffer because framebuffers can only output to
         * attachments of at least the same size which means no sub-sampled
         * rendering */
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormat vk_format;
        GstMemory *mem;

        vk_format =
            gst_vulkan_format_from_video_info (&conv->quad->out_info, i);

        mem = gst_vulkan_image_memory_alloc (vfilter->device,
            vk_format, GST_VIDEO_INFO_WIDTH (&conv->quad->out_info),
            GST_VIDEO_INFO_HEIGHT (&conv->quad->out_info), tiling,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        render_img_mems[i] = (GstVulkanImageMemory *) mem;
        need_render_buf = TRUE;
        GST_LOG_OBJECT (conv, "using replacement output memory %p for plane %u",
            mem, i);
      }
    }

    if (need_render_buf) {
      render_buf = gst_buffer_new ();
      for (i = 0; i < out_n_mems; i++) {
        gst_buffer_append_memory (render_buf,
            gst_memory_ref ((GstMemory *) render_img_mems[i]));
      }
      gst_vulkan_trash_list_add (conv->quad->trash_list,
          gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
              gst_vulkan_trash_mini_object_unref,
              (GstMiniObject *) render_buf));
    } else {
      render_buf = outbuf;
    }

    for (i = 0; i < out_n_mems; i++) {
      GstMemory *img_mem = gst_buffer_peek_memory (render_buf, i);
      if (!gst_is_vulkan_image_memory (img_mem)) {
        g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
            "Input memory must be a GstVulkanImageMemory");
        goto error;
      }
      render_img_views[i] =
          gst_vulkan_get_or_create_image_view ((GstVulkanImageMemory *)
          img_mem);
      gst_vulkan_trash_list_add (conv->quad->trash_list,
          gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
              gst_vulkan_trash_mini_object_unref,
              (GstMiniObject *) render_img_views[i]));
    }
  }

  if (!gst_vulkan_full_screen_quad_set_output_buffer (conv->quad, render_buf,
          &error))
    goto error;

  {
    GstMemory *uniforms = conv->current_shader->cmd_create_uniform (conv,
        conv->current_shader, in_img_views, render_img_views);
    if (!gst_vulkan_full_screen_quad_set_uniform_buffer (conv->quad, uniforms,
            &error)) {
      gst_memory_unref (uniforms);
      goto error;
    }
    gst_memory_unref (uniforms);
  }

  if (!gst_vulkan_full_screen_quad_prepare_draw (conv->quad, fence, &error))
    goto error;

  if (!(cmd_buf =
          gst_vulkan_command_pool_create (conv->quad->cmd_pool, &error)))
    goto error;

  {
    VkCommandBufferBeginInfo cmd_buf_info = { 0, };

    /* *INDENT-OFF* */
    cmd_buf_info = (VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    /* *INDENT-ON* */

    gst_vulkan_command_buffer_lock (cmd_buf);
    err = vkBeginCommandBuffer (cmd_buf->cmd, &cmd_buf_info);
    if (gst_vulkan_error_to_g_error (err, &error, "vkBeginCommandBuffer") < 0)
      goto error;
  }

  if (!gst_vulkan_full_screen_quad_fill_command_buffer (conv->quad, cmd_buf,
          fence, &error))
    goto unlock_error;

  for (i = 0; i < out_n_mems; i++) {
    if (render_img_mems[i] != out_img_mems[i]) {
      VkImageMemoryBarrier out_image_memory_barrier;
      VkImageMemoryBarrier render_image_memory_barrier;
      VkImageBlit blit;

      /* *INDENT-OFF* */
      render_image_memory_barrier = (VkImageMemoryBarrier) {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = NULL,
          .srcAccessMask = render_img_mems[i]->barrier.parent.access_flags,
          .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
          .oldLayout = render_img_mems[i]->barrier.image_layout,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          /* FIXME: implement exclusive transfers */
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = render_img_mems[i]->image,
          .subresourceRange = render_img_mems[i]->barrier.subresource_range
      };
      out_image_memory_barrier = (VkImageMemoryBarrier) {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = NULL,
          .srcAccessMask = out_img_mems[i]->barrier.parent.access_flags,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
          .oldLayout = out_img_mems[i]->barrier.image_layout,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          /* FIXME: implement exclusive transfers */
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = out_img_mems[i]->image,
          .subresourceRange = out_img_mems[i]->barrier.subresource_range
      };
      blit = (VkImageBlit) {
          .srcSubresource = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
          .srcOffsets = {
              { 0, 0, 0 },
              {
                  GST_VIDEO_INFO_COMP_WIDTH (&conv->quad->out_info, i),
                  GST_VIDEO_INFO_COMP_HEIGHT (&conv->quad->out_info, i),
                  1
              },
          },
          .dstSubresource = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
          .dstOffsets = {
              { 0, 0, 0 },
              {
                  GST_VIDEO_INFO_COMP_WIDTH (&conv->quad->out_info, i),
                  GST_VIDEO_INFO_COMP_HEIGHT (&conv->quad->out_info, i),
                  1
              },
          },
      };
      /* *INDENT-ON* */

      GST_LOG_OBJECT (conv, "blitting plane %u from %p to %p", i,
          render_img_mems[i], out_img_mems[i]);

      vkCmdPipelineBarrier (cmd_buf->cmd,
          render_img_mems[i]->barrier.parent.pipeline_stages,
          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
          &render_image_memory_barrier);

      render_img_mems[i]->barrier.parent.pipeline_stages =
          VK_PIPELINE_STAGE_TRANSFER_BIT;
      render_img_mems[i]->barrier.parent.access_flags =
          render_image_memory_barrier.dstAccessMask;
      render_img_mems[i]->barrier.image_layout =
          render_image_memory_barrier.newLayout;

      vkCmdPipelineBarrier (cmd_buf->cmd,
          out_img_mems[i]->barrier.parent.pipeline_stages,
          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
          &out_image_memory_barrier);

      out_img_mems[i]->barrier.parent.pipeline_stages =
          VK_PIPELINE_STAGE_TRANSFER_BIT;
      out_img_mems[i]->barrier.parent.access_flags =
          out_image_memory_barrier.dstAccessMask;
      out_img_mems[i]->barrier.image_layout =
          out_image_memory_barrier.newLayout;

      /* XXX: This is mostly right for a downsampling pass however if
       * anything is more complicated, then we will need a new render pass */
      vkCmdBlitImage (cmd_buf->cmd, render_img_mems[i]->image,
          render_img_mems[i]->barrier.image_layout, out_img_mems[i]->image,
          out_img_mems[i]->barrier.image_layout, 1, &blit, VK_FILTER_LINEAR);

      /* XXX: try to reuse this image later */
      gst_vulkan_trash_list_add (conv->quad->trash_list,
          gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
              gst_vulkan_trash_mini_object_unref,
              (GstMiniObject *) render_img_mems[i]));
    }
  }

  err = vkEndCommandBuffer (cmd_buf->cmd);
  gst_vulkan_command_buffer_unlock (cmd_buf);
  if (gst_vulkan_error_to_g_error (err, &error, "vkEndCommandBuffer") < 0)
    goto error;

  if (!gst_vulkan_full_screen_quad_submit (conv->quad, cmd_buf, fence, &error))
    goto error;

  gst_vulkan_fence_unref (fence);

  return GST_FLOW_OK;

unlock_error:
  if (cmd_buf) {
    gst_vulkan_command_buffer_unlock (cmd_buf);
    gst_vulkan_command_buffer_unref (cmd_buf);
  }
error:
  gst_clear_mini_object ((GstMiniObject **) & fence);

  GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("%s", error->message), (NULL));
  g_clear_error (&error);
  return GST_FLOW_ERROR;
}
