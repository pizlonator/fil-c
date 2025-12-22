/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
 * Copyright (C) <2019> Jeongki Kim <jeongki.kim@jeongki.kim>
 * Copyright (C) <2022> Seungha Yang <seungha@centricular.com>
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
#  include <config.h>
#endif

#include "gstd3d11-private.h"
#include "gstd3d11device-private.h"
#include "gstd3d11converter-private.h"
#include "gstd3d11converter.h"
#include "gstd3d11device.h"
#include "gstd3d11utils.h"
#include "gstd3d11memory.h"
#include "gstd3d11compile.h"
#include "gstd3d11bufferpool.h"
#include "gstd3d11converter-builder.h"
#include "gstd3d11converter-helper.h"
#include <wrl.h>
#include <string.h>
#include <math.h>
#include <map>
#include <memory>

#ifndef HAVE_DIRECTX_MATH_SIMD
#define _XM_NO_INTRINSICS_
#endif
#include <DirectXMath.h>

/**
 * SECTION:gstd3d11converter
 * @title: GstD3D11Converter
 * @short_description: Direct3D11 video converter object
 *
 * This object performs various video conversion operation
 * via Direct3D11 API
 *
 * Since: 1.22
 */

GST_DEBUG_CATEGORY (gst_d3d11_converter_debug);
#define GST_CAT_DEFAULT gst_d3d11_converter_debug

DEFINE_ENUM_FLAG_OPERATORS (GstD3D11ConverterBackend);

GType
gst_d3d11_converter_backend_get_type (void)
{
  static GType type = 0;
  static const GFlagsValue values[] = {
    {GST_D3D11_CONVERTER_BACKEND_SHADER, "GST_D3D11_CONVERTER_BACKEND_SHADER",
        "shader"},
    {GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR,
        "GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR", "video-processor"},
    {0, nullptr, nullptr}
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_flags_register_static ("GstD3D11ConverterBackend", values);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

GType
gst_d3d11_converter_sampler_filter_get_type (void)
{
  static GType filter_type = 0;
  static const GEnumValue filter_types[] = {
    {D3D11_FILTER_MIN_MAG_MIP_POINT,
        "D3D11_FILTER_MIN_MAG_MIP_POINT", "min-mag-mip-point"},
    {D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT,
        "D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT", "min-linear-mag-mip-point"},
    {D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        "D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT", "min-mag-linear-mip-point"},
    {D3D11_FILTER_ANISOTROPIC, "D3D11_FILTER_ANISOTROPIC", "anisotropic"},
    {0, nullptr, nullptr},
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    filter_type = g_enum_register_static ("GstD3D11ConverterSamplerFilter",
        filter_types);
  } GST_D3D11_CALL_ONCE_END;

  return filter_type;
}

GType
gst_d3d11_converter_alpha_mode_get_type (void)
{
  static GType type = 0;
  static const GEnumValue alpha_mode[] = {
    {GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED,
        "GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED", "unspecified"},
    {GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED,
        "GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED", "premultiplied"},
    {GST_D3D11_CONVERTER_ALPHA_MODE_STRAIGHT,
        "GST_D3D11_CONVERTER_ALPHA_MODE_STRAIGHT", "straight"},
    {0, nullptr, nullptr},
  };

  GST_D3D11_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D11ConverterAlphaMode", alpha_mode);
  } GST_D3D11_CALL_ONCE_END;

  return type;
}

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace DirectX;
/* *INDENT-ON* */

#define CONVERTER_MAX_QUADS 2
#define GAMMA_LUT_SIZE 4096

/* undefined symbols in ancient MinGW headers */
/* D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_METADATA_HDR10 */
#define FEATURE_CAPS_METADATA_HDR10 (0x800)
/* D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ROTATION */
#define FEATURE_CAPS_ROTATION (0x40)
/* D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_MIRROR */
#define PROCESSOR_FEATURE_CAPS_MIRROR (0x200)

struct PSColorSpace
{
  /* + 1 for 16bytes alignment  */
  FLOAT coeffX[4];
  FLOAT coeffY[4];
  FLOAT coeffZ[4];
  FLOAT offset[4];
  FLOAT min[4];
  FLOAT max[4];
};

struct PSConstBuffer
{
  PSColorSpace preCoeff;
  PSColorSpace postCoeff;
  PSColorSpace primariesCoeff;
  FLOAT alpha;
  FLOAT padding[3];
};

struct VertexData
{
  struct
  {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct
  {
    FLOAT u;
    FLOAT v;
  } texture;
};

struct GammaLut
{
  guint16 lut[GAMMA_LUT_SIZE];
};

/* *INDENT-OFF* */
typedef std::shared_ptr<GammaLut> GammaLutPtr;

static const XMFLOAT4X4A g_matrix_identity = XMFLOAT4X4A (
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_90r = XMFLOAT4X4A (
    0.0f, -1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_180 = XMFLOAT4X4A (
    -1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_90l = XMFLOAT4X4A (
    0.0f, 1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_horiz = XMFLOAT4X4A (
    -1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_vert = XMFLOAT4X4A (
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_ul_lr = XMFLOAT4X4A (
    0.0f, -1.0f, 0.0f, 0.0f,
    -1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

static const XMFLOAT4X4A g_matrix_ur_ll = XMFLOAT4X4A (
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
/* *INDENT-ON* */

enum
{
  PROP_0,
  PROP_SRC_X,
  PROP_SRC_Y,
  PROP_SRC_WIDTH,
  PROP_SRC_HEIGHT,
  PROP_DEST_X,
  PROP_DEST_Y,
  PROP_DEST_WIDTH,
  PROP_DEST_HEIGHT,
  PROP_ALPHA,
  PROP_BLEND_STATE,
  PROP_BLEND_FACTOR_RED,
  PROP_BLEND_FACTOR_GREEN,
  PROP_BLEND_FACTOR_BLUE,
  PROP_BLEND_FACTOR_ALPHA,
  PROP_BLEND_SAMPLE_MASK,
  PROP_FILL_BORDER,
  PROP_BORDER_COLOR,
  PROP_SRC_MASTERING_DISPLAY_INFO,
  PROP_SRC_CONTENT_LIGHT_LEVEL,
  PROP_DEST_MASTERING_DISPLAY_INFO,
  PROP_DEST_CONTENT_LIGHT_LEVEL,
  PROP_VIDEO_DIRECTION,
};

struct _GstD3D11ConverterPrivate
{
  _GstD3D11ConverterPrivate ()
  {
    custom_transform = g_matrix_identity;
  }

   ~_GstD3D11ConverterPrivate ()
  {
    g_free (in_mdcv_str);
    g_free (out_mdcv_str);
    g_free (in_cll_str);
    g_free (out_cll_str);

    if (preproc)
      gst_d3d11_converter_helper_free (preproc);

    if (postproc)
      gst_d3d11_converter_helper_free (postproc);

    gst_clear_buffer (&postproc_buf);
    gst_clear_buffer (&piv_inbuf);
  }

  GstVideoInfo in_info;
  GstVideoInfo out_info;

  GstD3D11Format in_d3d11_format;
  GstD3D11Format out_d3d11_format;

  guint num_input_view = 0;
  guint num_output_view = 0;

  GstD3D11ConverterBackend supported_backend = (GstD3D11ConverterBackend) 0;
  CONVERT_TYPE convert_type = CONVERT_TYPE::IDENTITY;

  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11Buffer > const_buffer;
  ComPtr < ID3D11Buffer > vs_const_buffer;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11SamplerState > sampler;
  ComPtr < ID3D11SamplerState > linear_sampler;
  ComPtr < ID3D11RasterizerState > rasterizer;
  ComPtr < ID3D11RasterizerState > msaa_rasterizer;
  PixelShaderList ps;
  D3D11_VIEWPORT viewport[GST_VIDEO_MAX_PLANES];

  ComPtr < ID3D11Texture1D > gamma_dec_lut;
  ComPtr < ID3D11Texture1D > gamma_enc_lut;
  ComPtr < ID3D11ShaderResourceView > gamma_dec_srv;
  ComPtr < ID3D11ShaderResourceView > gamma_enc_srv;

  D3D11_BLEND_DESC blend_desc;
  ComPtr < ID3D11BlendState > blend;

  gint input_texture_width;
  gint input_texture_height;
  gboolean update_src_rect = FALSE;
  gboolean update_dest_rect = FALSE;
  gboolean update_alpha = FALSE;
  gboolean update_transform = FALSE;
  XMFLOAT4X4A custom_transform;

  PSConstBuffer const_data;

  gboolean clear_background = FALSE;
  FLOAT clear_color[4][4];
  GstD3D11ColorMatrix clear_color_matrix;

  GstVideoInfo preproc_info;
  gboolean need_preproc = FALSE;

  GstVideoInfo postproc_info;
  gboolean need_postproc = FALSE;

  GstD3D11ConverterHelper *preproc = nullptr;
  GstD3D11ConverterHelper *postproc = nullptr;
  GstBuffer *postproc_buf = nullptr;

  /* video processor */
  D3D11_VIDEO_COLOR background_color;
  ComPtr < ID3D11VideoDevice > video_device;
  ComPtr < ID3D11VideoContext2 > video_context2;
  ComPtr < ID3D11VideoContext1 > video_context;
  ComPtr < ID3D11VideoProcessorEnumerator1 > enumerator;
  ComPtr < ID3D11VideoProcessor > processor;
  D3D11_VIDEO_PROCESSOR_CAPS processor_caps;
  RECT src_rect;
  RECT dest_rect;
  RECT dest_full_rect;
  gboolean processor_in_use = FALSE;
  gboolean processor_direction_not_supported = FALSE;
  gboolean enable_mirror = FALSE;
  gboolean flip_h = FALSE;
  gboolean flip_v = FALSE;
  gboolean enable_rotation = FALSE;
  D3D11_VIDEO_PROCESSOR_ROTATION rotation =
      D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY;

  /* HDR10 */
  gboolean have_in_hdr10 = FALSE;
  gboolean have_out_hdr10 = FALSE;
  gboolean in_hdr10_updated = FALSE;
  gboolean out_hdr10_updated = FALSE;
  DXGI_HDR_METADATA_HDR10 in_hdr10_meta;
  DXGI_HDR_METADATA_HDR10 out_hdr10_meta;
  gchar *in_mdcv_str = nullptr;
  gchar *out_mdcv_str = nullptr;
  gchar *in_cll_str = nullptr;
  gchar *out_cll_str = nullptr;

  /* Fallback buffer used for processor */
  GstVideoInfo piv_info;
  GstBuffer *piv_inbuf = nullptr;

  GstVideoOrientationMethod video_direction;

  std::mutex prop_lock;

  /* properties */
  gint src_x = 0;
  gint src_y = 0;
  gint src_width = 0;
  gint src_height = 0;
  gint dest_x = 0;
  gint dest_y = 0;
  gint dest_width = 0;
  gint dest_height = 0;
  gdouble alpha = 0;
  gfloat blend_factor[4];
  guint blend_sample_mask = 0;
  gboolean fill_border = FALSE;
  guint64 border_color = 0;
  GstD3D11ConverterAlphaMode src_alpha_mode =
      GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED;
  GstD3D11ConverterAlphaMode dst_alpha_mode =
      GST_D3D11_CONVERTER_ALPHA_MODE_UNSPECIFIED;
};

static void gst_d3d11_converter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_converter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d11_converter_finalize (GObject * object);
static void
gst_d3d11_converter_calculate_border_color (GstD3D11Converter * self);

#define gst_d3d11_converter_parent_class parent_class
G_DEFINE_TYPE (GstD3D11Converter, gst_d3d11_converter, GST_TYPE_OBJECT);

static void
gst_d3d11_converter_class_init (GstD3D11ConverterClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamFlags param_flags =
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  object_class->set_property = gst_d3d11_converter_set_property;
  object_class->get_property = gst_d3d11_converter_get_property;
  object_class->finalize = gst_d3d11_converter_finalize;

  g_object_class_install_property (object_class, PROP_SRC_X,
      g_param_spec_int ("src-x", "Src-X",
          "Source x poisition to start conversion", G_MININT, G_MAXINT, 0,
          param_flags));
  g_object_class_install_property (object_class, PROP_SRC_Y,
      g_param_spec_int ("src-y", "Src-Y",
          "Source y poisition to start conversion", G_MININT, G_MAXINT, 0,
          param_flags));
  g_object_class_install_property (object_class, PROP_SRC_WIDTH,
      g_param_spec_int ("src-width", "Src-Width",
          "Source width to convert", 0, G_MAXINT, 0, param_flags));
  g_object_class_install_property (object_class, PROP_SRC_HEIGHT,
      g_param_spec_int ("src-height", "Src-Height",
          "Source height to convert", 0, G_MAXINT, 0, param_flags));
  g_object_class_install_property (object_class, PROP_DEST_X,
      g_param_spec_int ("dest-x", "Dest-X",
          "x poisition in the destination frame", G_MININT, G_MAXINT, 0,
          param_flags));
  g_object_class_install_property (object_class, PROP_DEST_Y,
      g_param_spec_int ("dest-y", "Dest-Y",
          "y poisition in the destination frame", G_MININT, G_MAXINT, 0,
          param_flags));
  g_object_class_install_property (object_class, PROP_DEST_WIDTH,
      g_param_spec_int ("dest-width", "Dest-Width",
          "Width in the destination frame", 0, G_MAXINT, 0, param_flags));
  g_object_class_install_property (object_class, PROP_DEST_HEIGHT,
      g_param_spec_int ("dest-height", "Dest-Height",
          "Height in the destination frame", 0, G_MAXINT, 0, param_flags));
  g_object_class_install_property (object_class, PROP_ALPHA,
      g_param_spec_double ("alpha", "Alpha",
          "The alpha color value to use", 0, 1.0, 1.0, param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_STATE,
      g_param_spec_pointer ("blend-state", "Blend State",
          "ID3D11BlendState object to use", param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_FACTOR_RED,
      g_param_spec_float ("blend-factor-red", "Blend Factor Red",
          "Blend factor for red component", 0, 1.0, 1.0, param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_FACTOR_GREEN,
      g_param_spec_float ("blend-factor-green", "Blend Factor Green",
          "Blend factor for green component", 0, 1.0, 1.0, param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_FACTOR_BLUE,
      g_param_spec_float ("blend-factor-blue", "Blend Factor Blue",
          "Blend factor for blue component", 0, 1.0, 1.0, param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_FACTOR_ALPHA,
      g_param_spec_float ("blend-factor-alpha", "Blend Factor Alpha",
          "Blend factor for alpha component", 0, 1.0, 1.0, param_flags));
  g_object_class_install_property (object_class, PROP_BLEND_SAMPLE_MASK,
      g_param_spec_uint ("blend-sample-mask", "Blend Sample Mask",
          "Blend sample mask", 0, 0xffffffff, 0xffffffff, param_flags));
  g_object_class_install_property (object_class, PROP_FILL_BORDER,
      g_param_spec_boolean ("fill-border", "Fill border",
          "Fill border with \"border-color\" if destination rectangle does not "
          "fill the complete destination image", FALSE, param_flags));
  g_object_class_install_property (object_class, PROP_BORDER_COLOR,
      g_param_spec_uint64 ("border-color", "Border Color",
          "ARGB representation of the border color to use",
          0, G_MAXUINT64, 0xffff000000000000, param_flags));
  g_object_class_install_property (object_class,
      PROP_SRC_MASTERING_DISPLAY_INFO,
      g_param_spec_string ("src-mastering-display-info",
          "Src Mastering Display Info",
          "String representation of GstVideoMasteringDisplayInfo for source",
          nullptr, param_flags));
  g_object_class_install_property (object_class, PROP_SRC_CONTENT_LIGHT_LEVEL,
      g_param_spec_string ("src-content-light-level",
          "Src Content Light Level",
          "String representation of GstVideoContentLightLevel for src",
          nullptr, param_flags));
  g_object_class_install_property (object_class,
      PROP_DEST_MASTERING_DISPLAY_INFO,
      g_param_spec_string ("dest-mastering-display-info",
          "Dest Mastering Display Info",
          "String representation of GstVideoMasteringDisplayInfo for dest",
          nullptr, param_flags));
  g_object_class_install_property (object_class, PROP_DEST_CONTENT_LIGHT_LEVEL,
      g_param_spec_string ("dest-content-light-level",
          "Src Content Light Level",
          "String representation of GstVideoContentLightLevel for dest",
          nullptr, param_flags));
  g_object_class_install_property (object_class, PROP_VIDEO_DIRECTION,
      g_param_spec_enum ("video-direction", "Video Direction",
          "Video direction", GST_TYPE_VIDEO_ORIENTATION_METHOD,
          GST_VIDEO_ORIENTATION_IDENTITY, param_flags));

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_converter_debug,
      "d3d11converter", 0, "d3d11converter");
}

static void
gst_d3d11_converter_init (GstD3D11Converter * self)
{
  self->priv = new GstD3D11ConverterPrivate ();
}

static void
gst_d3d11_converter_finalize (GObject * object)
{
  GstD3D11Converter *self = GST_D3D11_CONVERTER (object);

  delete self->priv;

  if (self->device)
    gst_object_unref (self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
update_src_rect (GstD3D11Converter * self, gint * old_val,
    const GValue * new_val)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  gint tmp;

  tmp = g_value_get_int (new_val);
  if (tmp != *old_val) {
    priv->update_src_rect = TRUE;
    *old_val = tmp;
  }
}

static void
update_dest_rect (GstD3D11Converter * self, gint * old_val,
    const GValue * new_val)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  gint tmp;

  tmp = g_value_get_int (new_val);
  if (tmp != *old_val) {
    priv->update_dest_rect = TRUE;
    *old_val = tmp;
  }
}

static void
update_alpha (GstD3D11Converter * self, gdouble * old_val,
    const GValue * new_val)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  gdouble tmp;

  tmp = g_value_get_double (new_val);
  if (tmp != *old_val) {
    priv->update_alpha = TRUE;
    *old_val = tmp;
  }
}

static void
gst_d3d11_converter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11Converter *self = GST_D3D11_CONVERTER (object);
  GstD3D11ConverterPrivate *priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  switch (prop_id) {
    case PROP_SRC_X:
      update_src_rect (self, &priv->src_x, value);
      break;
    case PROP_SRC_Y:
      update_src_rect (self, &priv->src_y, value);
      break;
    case PROP_SRC_WIDTH:
      update_src_rect (self, &priv->src_width, value);
      break;
    case PROP_SRC_HEIGHT:
      update_src_rect (self, &priv->src_height, value);
      break;
    case PROP_DEST_X:
      update_dest_rect (self, &priv->dest_x, value);
      break;
    case PROP_DEST_Y:
      update_dest_rect (self, &priv->dest_y, value);
      break;
    case PROP_DEST_WIDTH:
      update_dest_rect (self, &priv->dest_width, value);
      break;
    case PROP_DEST_HEIGHT:
      update_dest_rect (self, &priv->dest_height, value);
      break;
    case PROP_ALPHA:
      update_alpha (self, &priv->alpha, value);
      priv->const_data.alpha = priv->alpha;
      break;
    case PROP_BLEND_STATE:{
      ID3D11BlendState *blend =
          (ID3D11BlendState *) g_value_get_pointer (value);
      priv->blend = nullptr;
      priv->blend = blend;
      if (priv->blend)
        priv->blend->GetDesc (&priv->blend_desc);
      break;
    }
    case PROP_BLEND_FACTOR_RED:
      priv->blend_factor[0] = g_value_get_float (value);
      break;
    case PROP_BLEND_FACTOR_GREEN:
      priv->blend_factor[1] = g_value_get_float (value);
      break;
    case PROP_BLEND_FACTOR_BLUE:
      priv->blend_factor[2] = g_value_get_float (value);
      break;
    case PROP_BLEND_FACTOR_ALPHA:
      priv->blend_factor[3] = g_value_get_float (value);
      break;
    case PROP_BLEND_SAMPLE_MASK:
      priv->blend_sample_mask = g_value_get_uint (value);
      break;
    case PROP_FILL_BORDER:{
      gboolean fill_border = g_value_get_boolean (value);

      if (fill_border != priv->fill_border) {
        priv->update_dest_rect = TRUE;
        priv->fill_border = fill_border;
      }
      break;
    }
    case PROP_BORDER_COLOR:{
      guint64 border_color = g_value_get_uint64 (value);

      if (border_color != priv->border_color) {
        priv->border_color = border_color;
        gst_d3d11_converter_calculate_border_color (self);
      }
      break;
    }
    case PROP_SRC_MASTERING_DISPLAY_INFO:
      g_clear_pointer (&priv->in_mdcv_str, g_free);
      priv->in_mdcv_str = g_value_dup_string (value);
      priv->in_hdr10_updated = TRUE;
      break;
    case PROP_SRC_CONTENT_LIGHT_LEVEL:
      g_clear_pointer (&priv->in_cll_str, g_free);
      priv->in_cll_str = g_value_dup_string (value);
      priv->in_hdr10_updated = TRUE;
      break;
    case PROP_DEST_MASTERING_DISPLAY_INFO:
      g_clear_pointer (&priv->out_mdcv_str, g_free);
      priv->out_mdcv_str = g_value_dup_string (value);
      priv->out_hdr10_updated = TRUE;
      break;
    case PROP_DEST_CONTENT_LIGHT_LEVEL:
      g_clear_pointer (&priv->out_cll_str, g_free);
      priv->out_cll_str = g_value_dup_string (value);
      priv->out_hdr10_updated = TRUE;
      break;
    case PROP_VIDEO_DIRECTION:{
      GstVideoOrientationMethod video_direction =
          (GstVideoOrientationMethod) g_value_get_enum (value);
      if (video_direction != priv->video_direction) {
        priv->video_direction = video_direction;
        priv->update_transform = TRUE;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_converter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11Converter *self = GST_D3D11_CONVERTER (object);
  GstD3D11ConverterPrivate *priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  switch (prop_id) {
    case PROP_SRC_X:
      g_value_set_int (value, priv->src_x);
      break;
    case PROP_SRC_Y:
      g_value_set_int (value, priv->src_y);
      break;
    case PROP_SRC_WIDTH:
      g_value_set_int (value, priv->src_width);
      break;
    case PROP_SRC_HEIGHT:
      g_value_set_int (value, priv->src_height);
      break;
    case PROP_DEST_X:
      g_value_set_int (value, priv->dest_x);
      break;
    case PROP_DEST_Y:
      g_value_set_int (value, priv->dest_y);
      break;
    case PROP_DEST_WIDTH:
      g_value_set_int (value, priv->dest_width);
      break;
    case PROP_DEST_HEIGHT:
      g_value_set_int (value, priv->dest_height);
      break;
    case PROP_ALPHA:
      g_value_set_double (value, priv->alpha);
      break;
    case PROP_BLEND_STATE:
      g_value_set_pointer (value, priv->blend.Get ());
      break;
    case PROP_BLEND_FACTOR_RED:
      g_value_set_float (value, priv->blend_factor[0]);
      break;
    case PROP_BLEND_FACTOR_GREEN:
      g_value_set_float (value, priv->blend_factor[1]);
      break;
    case PROP_BLEND_FACTOR_BLUE:
      g_value_set_float (value, priv->blend_factor[2]);
      break;
    case PROP_BLEND_FACTOR_ALPHA:
      g_value_set_float (value, priv->blend_factor[3]);
      break;
    case PROP_BLEND_SAMPLE_MASK:
      g_value_set_uint (value, priv->blend_sample_mask);
      break;
    case PROP_FILL_BORDER:
      g_value_set_boolean (value, priv->fill_border);
      break;
    case PROP_BORDER_COLOR:
      g_value_set_uint64 (value, priv->border_color);
      break;
    case PROP_SRC_MASTERING_DISPLAY_INFO:
      g_value_set_string (value, priv->in_mdcv_str);
      break;
    case PROP_SRC_CONTENT_LIGHT_LEVEL:
      g_value_set_string (value, priv->in_cll_str);
      break;
    case PROP_DEST_MASTERING_DISPLAY_INFO:
      g_value_set_string (value, priv->out_mdcv_str);
      break;
    case PROP_DEST_CONTENT_LIGHT_LEVEL:
      g_value_set_string (value, priv->out_cll_str);
      break;
    case PROP_VIDEO_DIRECTION:
      g_value_set_enum (value, priv->video_direction);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_d3d11_color_convert_setup_shader (GstD3D11Converter * self,
    const GstVideoInfo * in_info, const GstVideoInfo * out_info,
    D3D11_FILTER sampler_filter)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11Device *device = self->device;
  HRESULT hr;
  D3D11_BUFFER_DESC buffer_desc;
  VertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
  ID3D11Device *device_handle;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11SamplerState > sampler;
  ComPtr < ID3D11SamplerState > linear_sampler;
  ComPtr < ID3D11Buffer > const_buffer;
  ComPtr < ID3D11Buffer > vs_const_buffer;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11RasterizerState > rasterizer;
  ComPtr < ID3D11RasterizerState > msaa_rasterizer;
  D3D11_SUBRESOURCE_DATA subresource;

  memset (&subresource, 0, sizeof (subresource));
  memset (&buffer_desc, 0, sizeof (buffer_desc));

  device_handle = gst_d3d11_device_get_device_handle (device);

  hr = gst_d3d11_device_get_sampler (device, sampler_filter, &sampler);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Trying again with linear sampler");

    hr = gst_d3d11_device_get_sampler (device, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        &sampler);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Couldn't create sampler, hr: 0x%x", (guint) hr);
      return FALSE;
    }
  }

  hr = gst_d3d11_device_get_sampler (device, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
      &linear_sampler);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self,
        "Couldn't create linear sampler, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  auto ps_list = gst_d3d11_get_converter_pixel_shader (device,
      GST_VIDEO_INFO_FORMAT (in_info), GST_VIDEO_INFO_FORMAT (out_info),
      priv->src_alpha_mode == GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED,
      priv->dst_alpha_mode == GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED,
      priv->convert_type);
  if (ps_list.empty ()) {
    GST_ERROR_OBJECT (self, "Couldn't create pixel shader");
    return FALSE;
  }

  hr = gst_d3d11_get_converter_vertex_shader (device, &vs, &layout);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Couldn't create vertex shader");
    return FALSE;
  }

  hr = gst_d3d11_device_get_rasterizer (device, &rasterizer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Couldn't create rasterizer state");
    return FALSE;
  }

  hr = gst_d3d11_device_get_rasterizer_msaa (device, &msaa_rasterizer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Couldn't create rasterizer state");
    return FALSE;
  }

  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Couldn't create MSAA rasterizer state");
    return FALSE;
  }

  /* const buffer */
  if (priv->convert_type != CONVERT_TYPE::IDENTITY ||
      GST_VIDEO_INFO_HAS_ALPHA (out_info)) {
    G_STATIC_ASSERT (sizeof (PSConstBuffer) % 16 == 0);
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.ByteWidth = sizeof (PSConstBuffer);
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    subresource.pSysMem = &priv->const_data;
    subresource.SysMemPitch = sizeof (PSConstBuffer);

    hr = device_handle->CreateBuffer (&buffer_desc, &subresource,
        &const_buffer);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self,
          "Couldn't create constant buffer, hr: 0x%x", (guint) hr);
      return FALSE;
    }
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (g_matrix_identity);
  buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = g_matrix_identity.m;
  subresource.SysMemPitch = sizeof (FLOAT) * 16;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource,
      &vs_const_buffer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self,
        "Couldn't create constant buffer, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  /* bottom left */
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.u = 0.0f;
  vertex_data[0].texture.v = 1.0f;

  /* top left */
  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.u = 0.0f;
  vertex_data[1].texture.v = 0.0f;

  /* top right */
  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.u = 1.0f;
  vertex_data[2].texture.v = 0.0f;

  /* bottom right */
  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.u = 1.0f;
  vertex_data[3].texture.v = 1.0f;

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (VertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = vertex_data;
  subresource.SysMemPitch = sizeof (VertexData) * 4;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &vertex_buffer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self,
        "Couldn't create vertex buffer, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = indices;
  subresource.SysMemPitch = sizeof (WORD) * 6;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &index_buffer);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR ("Couldn't create index buffer, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  /* holds vertex buffer for crop rect update */
  priv->vertex_buffer = vertex_buffer;
  priv->index_buffer = index_buffer;
  priv->const_buffer = const_buffer;
  priv->vs_const_buffer = vs_const_buffer;
  priv->vs = vs;
  priv->layout = layout;
  priv->sampler = sampler;
  priv->linear_sampler = linear_sampler;
  priv->ps = ps_list;
  priv->rasterizer = rasterizer;
  priv->msaa_rasterizer = msaa_rasterizer;

  priv->input_texture_width = GST_VIDEO_INFO_WIDTH (in_info);
  priv->input_texture_height = GST_VIDEO_INFO_HEIGHT (in_info);

  priv->num_input_view = GST_VIDEO_INFO_N_PLANES (in_info);
  priv->num_output_view = GST_VIDEO_INFO_N_PLANES (out_info);

  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (out_info); i++) {
    priv->viewport[i].TopLeftX = 0;
    priv->viewport[i].TopLeftY = 0;
    priv->viewport[i].Width = GST_VIDEO_INFO_COMP_WIDTH (out_info, i);
    priv->viewport[i].Height = GST_VIDEO_INFO_COMP_HEIGHT (out_info, i);
    priv->viewport[i].MinDepth = 0.0f;
    priv->viewport[i].MaxDepth = 1.0f;
  }

  return TRUE;
}

static gboolean
gst_d3d11_converter_apply_orientation (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  ID3D11DeviceContext *context_handle;
  const XMFLOAT4X4A *matrix = &g_matrix_identity;
  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr;

  switch (priv->video_direction) {
    case GST_VIDEO_ORIENTATION_IDENTITY:
    case GST_VIDEO_ORIENTATION_AUTO:
    default:
      break;
    case GST_VIDEO_ORIENTATION_CUSTOM:
      matrix = &priv->custom_transform;
      break;
    case GST_VIDEO_ORIENTATION_90R:
      matrix = &g_matrix_90r;
      break;
    case GST_VIDEO_ORIENTATION_180:
      matrix = &g_matrix_180;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      matrix = &g_matrix_90l;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      matrix = &g_matrix_horiz;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      matrix = &g_matrix_vert;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      matrix = &g_matrix_ul_lr;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      matrix = &g_matrix_ur_ll;
      break;
  }

  context_handle = gst_d3d11_device_get_device_context_handle (self->device);

  hr = context_handle->Map (priv->vs_const_buffer.Get (), 0,
      D3D11_MAP_WRITE_DISCARD, 0, &map);
  if (!gst_d3d11_result (hr, self->device))
    return FALSE;

  memcpy (map.pData, matrix->m, sizeof (FLOAT) * 16);

  context_handle->Unmap (priv->vs_const_buffer.Get (), 0);

  return TRUE;
}

static void
gst_d3d11_converter_update_clear_background (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  const GstVideoInfo *out_info = &priv->out_info;

  if (priv->fill_border && (priv->dest_x != 0 || priv->dest_y != 0 ||
          priv->dest_width != out_info->width ||
          priv->dest_height != out_info->height ||
          priv->video_direction == GST_VIDEO_ORIENTATION_CUSTOM)) {
    GST_DEBUG_OBJECT (self, "Enable background color");
    priv->clear_background = TRUE;
  } else {
    GST_DEBUG_OBJECT (self, "Disable background color");
    priv->clear_background = FALSE;
  }
}

static gboolean
gst_d3d11_converter_update_transform (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if (!priv->update_transform)
    return TRUE;

  priv->update_transform = FALSE;

  gst_d3d11_converter_update_clear_background (self);

  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR)) {
    priv->processor_direction_not_supported = FALSE;
    priv->enable_mirror = FALSE;
    priv->flip_h = FALSE;
    priv->flip_v = FALSE;
    priv->enable_rotation = FALSE;
    priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY;

    /* filtering order is rotation -> mirror */
    switch (priv->video_direction) {
      case GST_VIDEO_ORIENTATION_IDENTITY:
      case GST_VIDEO_ORIENTATION_AUTO:
      case GST_VIDEO_ORIENTATION_CUSTOM:
      default:
        break;
      case GST_VIDEO_ORIENTATION_90R:
        priv->enable_rotation = TRUE;
        priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_90;
        break;
      case GST_VIDEO_ORIENTATION_180:
        priv->enable_rotation = TRUE;
        priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_180;
        break;
      case GST_VIDEO_ORIENTATION_90L:
        priv->enable_rotation = TRUE;
        priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_270;
        break;
      case GST_VIDEO_ORIENTATION_HORIZ:
        priv->enable_mirror = TRUE;
        priv->flip_h = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_VERT:
        priv->enable_mirror = TRUE;
        priv->flip_v = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_UL_LR:
        priv->enable_rotation = TRUE;
        priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_270;
        priv->enable_mirror = TRUE;
        priv->flip_v = TRUE;
        break;
      case GST_VIDEO_ORIENTATION_UR_LL:
        priv->enable_rotation = TRUE;
        priv->rotation = D3D11_VIDEO_PROCESSOR_ROTATION_90;
        priv->enable_mirror = TRUE;
        priv->flip_v = TRUE;
        break;
    }

    if (priv->video_direction != GST_VIDEO_ORIENTATION_CUSTOM) {
      if (priv->enable_rotation &&
          (priv->processor_caps.FeatureCaps & FEATURE_CAPS_ROTATION) == 0) {
        GST_WARNING_OBJECT (self, "Device does not support rotation");
        priv->processor_direction_not_supported = TRUE;
      }

      if (priv->enable_mirror &&
          (priv->processor_caps.FeatureCaps & PROCESSOR_FEATURE_CAPS_MIRROR) ==
          0) {
        GST_WARNING_OBJECT (self, "Device does not support mirror");
        priv->processor_direction_not_supported = TRUE;
      }
    }
  }

  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0)
    return TRUE;

  return gst_d3d11_converter_apply_orientation (self);
}

static gboolean
gst_d3d11_converter_update_src_rect (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  D3D11_MAPPED_SUBRESOURCE map;
  VertexData *vertex_data;
  ID3D11DeviceContext *context_handle;
  HRESULT hr;
  FLOAT u0, u1, v0, v1, off_u, off_v;
  gint texture_width = priv->input_texture_width;
  gint texture_height = priv->input_texture_height;

  if (!priv->update_src_rect)
    return TRUE;

  priv->update_src_rect = FALSE;

  priv->src_rect.left = priv->src_x;
  priv->src_rect.top = priv->src_y;
  priv->src_rect.right = priv->src_x + priv->src_width;
  priv->src_rect.bottom = priv->src_y + priv->src_height;

  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0)
    return TRUE;

  context_handle = gst_d3d11_device_get_device_context_handle (self->device);

  hr = context_handle->Map (priv->vertex_buffer.Get (),
      0, D3D11_MAP_WRITE_DISCARD, 0, &map);

  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map vertex buffer, hr: 0x%x", (guint) hr);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Updating vertex buffer");

  vertex_data = (VertexData *) map.pData;
  /*
   *  (u0, v0) -- (u1, v0)
   *     |            |
   *  (u0, v1) -- (u1, v1)
   */
  off_u = 0.5f / texture_width;
  off_v = 0.5f / texture_height;

  if (priv->src_x > 0)
    u0 = (priv->src_x / (gfloat) texture_width) + off_u;
  else
    u0 = 0.0f;

  if ((priv->src_x + priv->src_width) != texture_width)
    u1 = ((priv->src_x + priv->src_width) / (gfloat) texture_width) - off_u;
  else
    u1 = 1.0f;

  if (priv->src_y > 0)
    v0 = (priv->src_y / (gfloat) texture_height) + off_v;
  else
    v0 = 0.0;

  if ((priv->src_y + priv->src_height) != texture_height)
    v1 = ((priv->src_y + priv->src_height) / (gfloat) texture_height) - off_v;
  else
    v1 = 1.0f;

  /* bottom left */
  vertex_data[0].position.x = -1.0f;
  vertex_data[0].position.y = -1.0f;
  vertex_data[0].position.z = 0.0f;
  vertex_data[0].texture.u = u0;
  vertex_data[0].texture.v = v1;

  /* top left */
  vertex_data[1].position.x = -1.0f;
  vertex_data[1].position.y = 1.0f;
  vertex_data[1].position.z = 0.0f;
  vertex_data[1].texture.u = u0;
  vertex_data[1].texture.v = v0;

  /* top right */
  vertex_data[2].position.x = 1.0f;
  vertex_data[2].position.y = 1.0f;
  vertex_data[2].position.z = 0.0f;
  vertex_data[2].texture.u = u1;
  vertex_data[2].texture.v = v0;

  /* bottom right */
  vertex_data[3].position.x = 1.0f;
  vertex_data[3].position.y = -1.0f;
  vertex_data[3].position.z = 0.0f;
  vertex_data[3].texture.u = u1;
  vertex_data[3].texture.v = v1;

  context_handle->Unmap (priv->vertex_buffer.Get (), 0);

  return TRUE;
}

static gboolean
gst_d3d11_converter_update_dest_rect (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if (!priv->update_dest_rect)
    return TRUE;

  priv->viewport[0].TopLeftX = priv->dest_x;
  priv->viewport[0].TopLeftY = priv->dest_y;
  priv->viewport[0].Width = priv->dest_width;
  priv->viewport[0].Height = priv->dest_height;

  priv->dest_rect.left = priv->dest_x;
  priv->dest_rect.top = priv->dest_y;
  priv->dest_rect.right = priv->dest_x + priv->dest_width;
  priv->dest_rect.bottom = priv->dest_y + priv->dest_height;

  GST_DEBUG_OBJECT (self,
      "Update viewport, TopLeftX: %f, TopLeftY: %f, Width: %f, Height %f",
      priv->viewport[0].TopLeftX, priv->viewport[0].TopLeftY,
      priv->viewport[0].Width, priv->viewport[0].Height);

  gst_d3d11_converter_update_clear_background (self);

  switch (GST_VIDEO_INFO_FORMAT (&priv->postproc_info)) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_12LE:
      priv->viewport[1].TopLeftX = priv->viewport[0].TopLeftX / 2;
      priv->viewport[1].TopLeftY = priv->viewport[0].TopLeftY / 2;
      priv->viewport[1].Width = priv->viewport[0].Width / 2;
      priv->viewport[1].Height = priv->viewport[0].Height / 2;

      for (guint i = 2; i < GST_VIDEO_INFO_N_PLANES (&priv->postproc_info); i++)
        priv->viewport[i] = priv->viewport[1];

      break;
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_12LE:
      priv->viewport[1].TopLeftX = priv->viewport[0].TopLeftX / 2;
      priv->viewport[1].TopLeftY = priv->viewport[0].TopLeftY;
      priv->viewport[1].Width = priv->viewport[0].Width / 2;
      priv->viewport[1].Height = priv->viewport[0].Height;

      for (guint i = 2; i < GST_VIDEO_INFO_N_PLANES (&priv->postproc_info); i++)
        priv->viewport[i] = priv->viewport[1];
      break;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBR_10LE:
    case GST_VIDEO_FORMAT_GBR_12LE:
    case GST_VIDEO_FORMAT_GBR_16LE:
    case GST_VIDEO_FORMAT_GBRA:
    case GST_VIDEO_FORMAT_GBRA_10LE:
    case GST_VIDEO_FORMAT_GBRA_12LE:
      for (guint i = 1; i < GST_VIDEO_INFO_N_PLANES (&priv->postproc_info); i++)
        priv->viewport[i] = priv->viewport[0];
      break;
    default:
      if (priv->num_output_view > 1) {
        g_assert_not_reached ();
        return FALSE;
      }
      break;
  }

  priv->update_dest_rect = FALSE;

  return TRUE;
}

static void
convert_info_gray_to_yuv (const GstVideoInfo * gray, GstVideoInfo * yuv)
{
  GstVideoInfo tmp;

  if (GST_VIDEO_INFO_IS_YUV (gray)) {
    *yuv = *gray;
    return;
  }

  if (gray->finfo->depth[0] == 8) {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444, gray->width, gray->height);
  } else {
    gst_video_info_set_format (&tmp,
        GST_VIDEO_FORMAT_Y444_16LE, gray->width, gray->height);
  }

  tmp.colorimetry.range = gray->colorimetry.range;
  if (tmp.colorimetry.range == GST_VIDEO_COLOR_RANGE_UNKNOWN)
    tmp.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

  tmp.colorimetry.primaries = gray->colorimetry.primaries;
  if (tmp.colorimetry.primaries == GST_VIDEO_COLOR_PRIMARIES_UNKNOWN)
    tmp.colorimetry.primaries = GST_VIDEO_COLOR_PRIMARIES_BT709;

  tmp.colorimetry.transfer = gray->colorimetry.transfer;
  if (tmp.colorimetry.transfer == GST_VIDEO_TRANSFER_UNKNOWN)
    tmp.colorimetry.transfer = GST_VIDEO_TRANSFER_BT709;

  tmp.colorimetry.matrix = gray->colorimetry.matrix;
  if (tmp.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN)
    tmp.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;

  *yuv = tmp;
}

static gboolean
gst_d3d11_converter_calculate_matrix (GstD3D11Converter * self,
    const GstVideoInfo * in_info, const GstVideoInfo * out_info)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11ColorMatrix pre_coeff;
  GstD3D11ColorMatrix post_coeff;
  GstD3D11ColorMatrix primaries_coeff;
  GstVideoInfo rgb_info;

  gst_d3d11_color_matrix_init (&pre_coeff);
  gst_d3d11_color_matrix_init (&post_coeff);
  gst_d3d11_color_matrix_init (&primaries_coeff);

  switch (priv->convert_type) {
    case CONVERT_TYPE::RANGE:
      gst_d3d11_color_range_adjust_matrix_unorm (in_info, out_info,
          &post_coeff);
      break;
    case CONVERT_TYPE::SIMPLE:
      if (GST_VIDEO_INFO_IS_RGB (in_info)) {
        gst_d3d11_rgb_to_yuv_matrix_unorm (in_info, out_info, &post_coeff);
      } else {
        gst_d3d11_yuv_to_rgb_matrix_unorm (in_info, out_info, &post_coeff);
      }
      break;
    case CONVERT_TYPE::GAMMA:
    case CONVERT_TYPE::PRIMARY:
      if (GST_VIDEO_INFO_IS_RGB (in_info)) {
        rgb_info = *in_info;
        if (in_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
          rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

          gst_d3d11_color_range_adjust_matrix_unorm (in_info,
              &rgb_info, &pre_coeff);
        }
      } else {
        gst_video_info_set_format (&rgb_info,
            in_info->finfo->depth[0] == 8 ? GST_VIDEO_FORMAT_RGBA :
            GST_VIDEO_FORMAT_RGBA64_LE, in_info->width, in_info->height);
        rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
        rgb_info.colorimetry.transfer = in_info->colorimetry.transfer;
        rgb_info.colorimetry.primaries = in_info->colorimetry.primaries;

        gst_d3d11_yuv_to_rgb_matrix_unorm (in_info, &rgb_info, &pre_coeff);
      }

      if (priv->convert_type == CONVERT_TYPE::PRIMARY) {
        const GstVideoColorPrimariesInfo *in_pinfo;
        const GstVideoColorPrimariesInfo *out_pinfo;

        in_pinfo =
            gst_video_color_primaries_get_info (in_info->colorimetry.primaries);
        out_pinfo =
            gst_video_color_primaries_get_info (out_info->
            colorimetry.primaries);

        gst_d3d11_color_primaries_matrix_unorm (in_pinfo, out_pinfo,
            &primaries_coeff);
      }

      if (GST_VIDEO_INFO_IS_RGB (out_info)) {
        if (out_info->colorimetry.range == GST_VIDEO_COLOR_RANGE_16_235) {
          rgb_info = *out_info;
          rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;

          gst_d3d11_color_range_adjust_matrix_unorm (&rgb_info,
              out_info, &post_coeff);
        }
      } else {
        gst_d3d11_rgb_to_yuv_matrix_unorm (&rgb_info, out_info, &post_coeff);
      }
      break;
    default:
      break;
  }

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *matrix_dump;
    matrix_dump = gst_d3d11_dump_color_matrix (&pre_coeff);
    GST_DEBUG_OBJECT (self, "PreCoeff \n%s", matrix_dump);
    g_free (matrix_dump);

    matrix_dump = gst_d3d11_dump_color_matrix (&primaries_coeff);
    GST_DEBUG_OBJECT (self, "PrimaryCoeff \n%s", matrix_dump);
    g_free (matrix_dump);

    matrix_dump = gst_d3d11_dump_color_matrix (&post_coeff);
    GST_DEBUG_OBJECT (self, "PostCoeff \n%s", matrix_dump);
    g_free (matrix_dump);
  }

  PSColorSpace *preCoeff = &priv->const_data.preCoeff;
  PSColorSpace *postCoeff = &priv->const_data.postCoeff;
  PSColorSpace *primariesCoeff = &priv->const_data.primariesCoeff;

  for (guint i = 0; i < 3; i++) {
    preCoeff->coeffX[i] = pre_coeff.matrix[0][i];
    preCoeff->coeffY[i] = pre_coeff.matrix[1][i];
    preCoeff->coeffZ[i] = pre_coeff.matrix[2][i];
    preCoeff->offset[i] = pre_coeff.offset[i];
    preCoeff->min[i] = pre_coeff.min[i];
    preCoeff->max[i] = pre_coeff.max[i];

    postCoeff->coeffX[i] = post_coeff.matrix[0][i];
    postCoeff->coeffY[i] = post_coeff.matrix[1][i];
    postCoeff->coeffZ[i] = post_coeff.matrix[2][i];
    postCoeff->offset[i] = post_coeff.offset[i];
    postCoeff->min[i] = post_coeff.min[i];
    postCoeff->max[i] = post_coeff.max[i];

    primariesCoeff->coeffX[i] = primaries_coeff.matrix[0][i];
    primariesCoeff->coeffY[i] = primaries_coeff.matrix[1][i];
    primariesCoeff->coeffZ[i] = primaries_coeff.matrix[2][i];
    primariesCoeff->offset[i] = primaries_coeff.offset[i];
    primariesCoeff->min[i] = primaries_coeff.min[i];
    primariesCoeff->max[i] = primaries_coeff.max[i];
  }

  return TRUE;
}

static GammaLutPtr
gst_d3d11_converter_get_gamma_dec_table (GstVideoTransferFunction func)
{
  static std::mutex lut_lock;
  static std::map < GstVideoTransferFunction, GammaLutPtr > g_gamma_dec_table;

  std::lock_guard < std::mutex > lk (lut_lock);
  auto lut = g_gamma_dec_table.find (func);
  if (lut != g_gamma_dec_table.end ())
    return lut->second;

  const gdouble scale = (gdouble) 1 / (GAMMA_LUT_SIZE - 1);
  auto table = std::make_shared < GammaLut > ();
  for (guint i = 0; i < GAMMA_LUT_SIZE; i++) {
    gdouble val = gst_video_transfer_function_decode (func, i * scale);
    val = rint (val * 65535);
    val = CLAMP (val, 0, 65535);
    table->lut[i] = (guint16) val;
  }

  g_gamma_dec_table[func] = table;
  return table;
}

static GammaLutPtr
gst_d3d11_converter_get_gamma_enc_table (GstVideoTransferFunction func)
{
  static std::mutex lut_lock;
  static std::map < GstVideoTransferFunction, GammaLutPtr > g_gamma_enc_table;

  std::lock_guard < std::mutex > lk (lut_lock);
  auto lut = g_gamma_enc_table.find (func);
  if (lut != g_gamma_enc_table.end ())
    return lut->second;

  const gdouble scale = (gdouble) 1 / (GAMMA_LUT_SIZE - 1);
  auto table = std::make_shared < GammaLut > ();
  for (guint i = 0; i < GAMMA_LUT_SIZE; i++) {
    gdouble val = gst_video_transfer_function_encode (func, i * scale);
    val = rint (val * 65535);
    val = CLAMP (val, 0, 65535);
    table->lut[i] = (guint16) val;
  }

  g_gamma_enc_table[func] = table;
  return table;
}

static gboolean
gst_d3d11_converter_setup_lut (GstD3D11Converter * self,
    const GstVideoInfo * in_info, const GstVideoInfo * out_info)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11Device *device = self->device;
  ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
  D3D11_TEXTURE1D_DESC desc;
  D3D11_SUBRESOURCE_DATA subresource;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
  HRESULT hr;
  ComPtr < ID3D11Texture1D > gamma_dec_lut;
  ComPtr < ID3D11Texture1D > gamma_enc_lut;
  ComPtr < ID3D11ShaderResourceView > gamma_dec_srv;
  ComPtr < ID3D11ShaderResourceView > gamma_enc_srv;
  GammaLutPtr gamma_dec_table;
  GammaLutPtr gamma_enc_table;
  GstVideoTransferFunction in_trc = in_info->colorimetry.transfer;
  GstVideoTransferFunction out_trc = out_info->colorimetry.transfer;

  memset (&desc, 0, sizeof (D3D11_TEXTURE1D_DESC));
  memset (&subresource, 0, sizeof (D3D11_SUBRESOURCE_DATA));
  memset (&srv_desc, 0, sizeof (D3D11_SHADER_RESOURCE_VIEW_DESC));

  gamma_dec_table = gst_d3d11_converter_get_gamma_dec_table (in_trc);
  gamma_enc_table = gst_d3d11_converter_get_gamma_enc_table (out_trc);

  desc.Width = GAMMA_LUT_SIZE;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R16_UNORM;
  desc.Usage = D3D11_USAGE_IMMUTABLE;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  subresource.pSysMem = gamma_dec_table->lut;
  subresource.SysMemPitch = GAMMA_LUT_SIZE * sizeof (guint16);

  srv_desc.Format = DXGI_FORMAT_R16_UNORM;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
  srv_desc.Texture1D.MipLevels = 1;

  hr = device_handle->CreateTexture1D (&desc, &subresource, &gamma_dec_lut);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Failed to create gamma decode LUT");
    return FALSE;
  }

  hr = device_handle->CreateShaderResourceView (gamma_dec_lut.Get (), &srv_desc,
      &gamma_dec_srv);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Failed to create gamma decode LUT SRV");
    return FALSE;
  }

  subresource.pSysMem = gamma_enc_table->lut;
  hr = device_handle->CreateTexture1D (&desc, &subresource, &gamma_enc_lut);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "Failed to create gamma encode LUT");
    return FALSE;
  }

  hr = device_handle->CreateShaderResourceView (gamma_enc_lut.Get (), &srv_desc,
      &gamma_enc_srv);
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (device, "Failed to create gamma decode LUT SRV");
    return FALSE;
  }

  priv->gamma_dec_lut = gamma_dec_lut;
  priv->gamma_enc_lut = gamma_enc_lut;
  priv->gamma_dec_srv = gamma_dec_srv;
  priv->gamma_enc_srv = gamma_enc_srv;

  return TRUE;
}

static gboolean
is_custom_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_BGRA64_LE:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_RBGA:
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

static void
gst_d3d11_converter_calculate_border_color (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11ColorMatrix *m = &priv->clear_color_matrix;
  const GstVideoInfo *out_info = &priv->postproc_info;
  gdouble a;
  gdouble rgb[3];
  gdouble converted[3];
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (out_info);

  a = ((priv->border_color & 0xffff000000000000) >> 48) / (gdouble) G_MAXUINT16;
  rgb[0] =
      ((priv->border_color & 0x0000ffff00000000) >> 32) / (gdouble) G_MAXUINT16;
  rgb[1] =
      ((priv->border_color & 0x00000000ffff0000) >> 16) / (gdouble) G_MAXUINT16;
  rgb[2] = (priv->border_color & 0x000000000000ffff) / (gdouble) G_MAXUINT16;

  for (guint i = 0; i < 3; i++) {
    converted[i] = 0;
    for (guint j = 0; j < 3; j++) {
      converted[i] += m->matrix[i][j] * rgb[j];
    }
    converted[i] += m->offset[i];
    converted[i] = CLAMP (converted[i], m->min[i], m->max[i]);
  }

  GST_DEBUG_OBJECT (self, "Calculated background color ARGB: %f, %f, %f, %f",
      a, converted[0], converted[1], converted[2]);

  /* background color for video processor */
  priv->background_color.RGBA.R = converted[0];
  priv->background_color.RGBA.G = converted[1];
  priv->background_color.RGBA.B = converted[2];
  priv->background_color.RGBA.A = a;

  /* scale down if output is planar high bitdepth format */
  switch (format) {
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_GBR_10LE:
    case GST_VIDEO_FORMAT_GBRA_10LE:
      for (guint i = 0; i < 3; i++) {
        converted[i] /= 64.0;
      }
      a /= 64.0;
      break;
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_GBR_12LE:
    case GST_VIDEO_FORMAT_GBRA_12LE:
      for (guint i = 0; i < 3; i++) {
        converted[i] /= 16.0;
      }
      a /= 16.0;
      break;
    default:
      break;
  }

  if ((GST_VIDEO_INFO_IS_RGB (out_info) &&
          GST_VIDEO_INFO_N_PLANES (out_info) == 1 &&
          !is_custom_format (format)) || GST_VIDEO_INFO_IS_GRAY (out_info)) {
    for (guint i = 0; i < 3; i++)
      priv->clear_color[0][i] = converted[i];
    priv->clear_color[0][3] = a;
  } else {
    switch (format) {
      case GST_VIDEO_FORMAT_VUYA:
        priv->clear_color[0][0] = converted[2];
        priv->clear_color[0][1] = converted[1];
        priv->clear_color[0][2] = converted[0];
        priv->clear_color[0][3] = a;
        break;
      case GST_VIDEO_FORMAT_AYUV:
      case GST_VIDEO_FORMAT_AYUV64:
        priv->clear_color[0][0] = a;
        priv->clear_color[0][1] = converted[0];
        priv->clear_color[0][2] = converted[1];
        priv->clear_color[0][3] = converted[2];
        break;
      case GST_VIDEO_FORMAT_ARGB:
      case GST_VIDEO_FORMAT_xRGB:
        priv->clear_color[0][0] = a;
        priv->clear_color[0][1] = converted[0];
        priv->clear_color[0][2] = converted[1];
        priv->clear_color[0][3] = converted[2];
        break;
      case GST_VIDEO_FORMAT_ABGR:
      case GST_VIDEO_FORMAT_xBGR:
        priv->clear_color[0][0] = a;
        priv->clear_color[0][1] = converted[2];
        priv->clear_color[0][2] = converted[1];
        priv->clear_color[0][3] = converted[0];
        break;
      case GST_VIDEO_FORMAT_RBGA:
        priv->clear_color[0][0] = converted[0];
        priv->clear_color[0][1] = converted[2];
        priv->clear_color[0][2] = converted[1];
        priv->clear_color[0][3] = a;
        break;
      case GST_VIDEO_FORMAT_NV12:
      case GST_VIDEO_FORMAT_NV21:
      case GST_VIDEO_FORMAT_P010_10LE:
      case GST_VIDEO_FORMAT_P012_LE:
      case GST_VIDEO_FORMAT_P016_LE:
        priv->clear_color[0][0] = converted[0];
        priv->clear_color[0][1] = 0;
        priv->clear_color[0][2] = 0;
        priv->clear_color[0][3] = 1.0;
        if (format == GST_VIDEO_FORMAT_NV21) {
          priv->clear_color[1][0] = converted[2];
          priv->clear_color[1][1] = converted[1];
        } else {
          priv->clear_color[1][0] = converted[1];
          priv->clear_color[1][1] = converted[2];
        }
        priv->clear_color[1][2] = 0;
        priv->clear_color[1][3] = 1.0;
        break;
      case GST_VIDEO_FORMAT_I420:
      case GST_VIDEO_FORMAT_YV12:
      case GST_VIDEO_FORMAT_I420_10LE:
      case GST_VIDEO_FORMAT_I420_12LE:
      case GST_VIDEO_FORMAT_Y42B:
      case GST_VIDEO_FORMAT_I422_10LE:
      case GST_VIDEO_FORMAT_I422_12LE:
      case GST_VIDEO_FORMAT_Y444:
      case GST_VIDEO_FORMAT_Y444_10LE:
      case GST_VIDEO_FORMAT_Y444_12LE:
      case GST_VIDEO_FORMAT_Y444_16LE:
        priv->clear_color[0][0] = converted[0];
        priv->clear_color[0][1] = 0;
        priv->clear_color[0][2] = 0;
        priv->clear_color[0][3] = 1.0;
        if (format == GST_VIDEO_FORMAT_YV12) {
          priv->clear_color[1][0] = converted[2];
          priv->clear_color[2][0] = converted[1];
        } else {
          priv->clear_color[1][0] = converted[1];
          priv->clear_color[2][0] = converted[2];
        }
        priv->clear_color[1][1] = 0;
        priv->clear_color[1][2] = 0;
        priv->clear_color[1][3] = 1.0;
        priv->clear_color[2][1] = 0;
        priv->clear_color[2][2] = 0;
        priv->clear_color[2][3] = 1.0;
        break;
      case GST_VIDEO_FORMAT_RGBP:
        priv->clear_color[0][0] = converted[0];
        priv->clear_color[1][0] = converted[1];
        priv->clear_color[2][0] = converted[2];
        break;
      case GST_VIDEO_FORMAT_BGRP:
        priv->clear_color[0][0] = converted[2];
        priv->clear_color[1][0] = converted[1];
        priv->clear_color[2][0] = converted[0];
        break;
      case GST_VIDEO_FORMAT_GBR:
      case GST_VIDEO_FORMAT_GBR_10LE:
      case GST_VIDEO_FORMAT_GBR_12LE:
      case GST_VIDEO_FORMAT_GBR_16LE:
        priv->clear_color[0][0] = converted[1];
        priv->clear_color[1][0] = converted[2];
        priv->clear_color[2][0] = converted[0];
        break;
      case GST_VIDEO_FORMAT_GBRA:
      case GST_VIDEO_FORMAT_GBRA_10LE:
      case GST_VIDEO_FORMAT_GBRA_12LE:
        priv->clear_color[0][0] = converted[1];
        priv->clear_color[1][0] = converted[2];
        priv->clear_color[2][0] = converted[0];
        priv->clear_color[3][0] = a;
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }
}

static gboolean
gst_d3d11_converter_setup_processor (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11Device *device = self->device;
  ID3D11VideoDevice *video_device;
  ID3D11VideoContext *video_context;
  HRESULT hr;
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  ComPtr < ID3D11VideoContext1 > video_context1;
  ComPtr < ID3D11VideoContext2 > video_context2;
  ComPtr < ID3D11VideoProcessorEnumerator > enumerator;
  ComPtr < ID3D11VideoProcessorEnumerator1 > enumerator1;
  ComPtr < ID3D11VideoProcessor > processor;
  UINT support_flags;
  BOOL conversion_supported = TRUE;
  DXGI_COLOR_SPACE_TYPE in_space, out_space;
  DXGI_FORMAT in_dxgi_format = priv->in_d3d11_format.dxgi_format;
  DXGI_FORMAT out_dxgi_format = priv->out_d3d11_format.dxgi_format;
  UINT in_format_flags = priv->in_d3d11_format.format_support[0];
  UINT out_format_flags = priv->out_d3d11_format.format_support[0];

  if (GST_VIDEO_INFO_IS_GRAY (&priv->in_info) ||
      GST_VIDEO_INFO_IS_GRAY (&priv->out_info)) {
    return FALSE;
  }

  /* Not a native DXGI format */
  if (in_dxgi_format == DXGI_FORMAT_UNKNOWN ||
      out_dxgi_format == DXGI_FORMAT_UNKNOWN) {
    return FALSE;
  }

  if (is_custom_format (GST_VIDEO_INFO_FORMAT (&priv->in_info)) ||
      is_custom_format (GST_VIDEO_INFO_FORMAT (&priv->out_info))) {
    return FALSE;
  }

  /* cannot bind to processor in/out view */
  if ((in_format_flags & D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT) == 0 ||
      (out_format_flags & D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT) == 0) {
    return FALSE;
  }

  if (!gst_video_info_to_dxgi_color_space (&priv->in_info, &in_space)) {
    GST_WARNING_OBJECT (self, "Unknown input DXGI colorspace");
    return FALSE;
  }

  if (!gst_video_info_to_dxgi_color_space (&priv->out_info, &out_space)) {
    GST_WARNING_OBJECT (self, "Unknown output DXGI colorspace");
    return FALSE;
  }

  video_device = gst_d3d11_device_get_video_device_handle (self->device);
  if (!video_device) {
    GST_DEBUG_OBJECT (self, "video device interface is not available");
    return FALSE;
  }

  video_context = gst_d3d11_device_get_video_context_handle (self->device);
  if (!video_context) {
    GST_DEBUG_OBJECT (self, "video context interface is not available");
    return FALSE;
  }

  hr = video_context->QueryInterface (IID_PPV_ARGS (&video_context1));
  if (!gst_d3d11_result (hr, device)) {
    GST_DEBUG_OBJECT (self, "ID3D11VideoContext1 interface is not available");
    return FALSE;
  }

  memset (&desc, 0, sizeof (D3D11_VIDEO_PROCESSOR_CONTENT_DESC));

  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputWidth = priv->in_info.width;
  desc.InputHeight = priv->in_info.height;
  desc.OutputWidth = priv->out_info.width;
  desc.OutputHeight = priv->out_info.height;
  /* TODO: make option for this */
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = video_device->CreateVideoProcessorEnumerator (&desc, &enumerator);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Failed to create enumerator");
    return FALSE;
  }

  hr = enumerator.As (&enumerator1);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self,
        "ID3D11VideoProcessorEnumerator1 interface is not available");
    return FALSE;
  }

  support_flags = 0;
  hr = enumerator1->CheckVideoProcessorFormat (in_dxgi_format, &support_flags);
  if (!gst_d3d11_result (hr, device) || (support_flags & 0x1) == 0) {
    GST_DEBUG_OBJECT (self, "Input format is not supported");
    return FALSE;
  }

  support_flags = 0;
  hr = enumerator1->CheckVideoProcessorFormat (out_dxgi_format, &support_flags);
  if (!gst_d3d11_result (hr, device) || (support_flags & 0x2) == 0) {
    GST_DEBUG_OBJECT (self, "Output format is not supported");
    return FALSE;
  }

  hr = enumerator1->CheckVideoProcessorFormatConversion (in_dxgi_format,
      in_space, out_dxgi_format, out_space, &conversion_supported);
  if (!gst_d3d11_result (hr, device) || !conversion_supported) {
    GST_DEBUG_OBJECT (self, "Conversion is not supported");
    return FALSE;
  }

  hr = enumerator1->GetVideoProcessorCaps (&priv->processor_caps);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Failed to query processor caps");
    return FALSE;
  }

  hr = video_device->CreateVideoProcessor (enumerator1.Get (), 0, &processor);
  if (!gst_d3d11_result (hr, device)) {
    GST_WARNING_OBJECT (self, "Failed to create processor");
    return FALSE;
  }

  GstD3D11DeviceLockGuard lk (device);
  /* We don't want auto processing by driver */
  video_context1->VideoProcessorSetStreamAutoProcessingMode
      (processor.Get (), 0, FALSE);
  video_context1->VideoProcessorSetStreamColorSpace1 (processor.Get (),
      0, in_space);
  video_context1->VideoProcessorSetOutputColorSpace1 (processor.Get (),
      out_space);

  priv->video_device = video_device;
  priv->processor = processor;
  hr = video_context1.As (&video_context2);
  if (SUCCEEDED (hr))
    priv->video_context2 = video_context2;
  priv->video_context = video_context1;
  priv->enumerator = enumerator1;

  priv->src_rect.left = 0;
  priv->src_rect.top = 0;
  priv->src_rect.right = priv->in_info.width;
  priv->src_rect.bottom = priv->in_info.height;

  priv->dest_rect.left = 0;
  priv->dest_rect.top = 0;
  priv->dest_rect.right = priv->out_info.width;
  priv->dest_rect.bottom = priv->out_info.height;

  priv->dest_full_rect = priv->dest_rect;

  return TRUE;
}

static GstVideoFormat
get_shader_format (GstVideoFormat format, gboolean is_input)
{
  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_v308:
    case GST_VIDEO_FORMAT_IYU2:
      return GST_VIDEO_FORMAT_AYUV;
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y212_LE:
    case GST_VIDEO_FORMAT_v210:
    case GST_VIDEO_FORMAT_v216:
      return GST_VIDEO_FORMAT_AYUV64;
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_Y412_LE:
      if (!is_input)
        return GST_VIDEO_FORMAT_AYUV64;
      break;
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR16:
    case GST_VIDEO_FORMAT_RGB15:
    case GST_VIDEO_FORMAT_BGR15:
      return GST_VIDEO_FORMAT_RGBA;
    case GST_VIDEO_FORMAT_r210:
      return GST_VIDEO_FORMAT_RGB10A2_LE;
    case GST_VIDEO_FORMAT_BGRA64_LE:
      if (!is_input)
        return GST_VIDEO_FORMAT_RGBA64_LE;
      break;
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      if (!is_input)
        return GST_VIDEO_FORMAT_RGB10A2_LE;
      break;
    default:
      break;
  }

  return format;
}

/**
 * gst_d3d11_converter_new:
 * @device: a #GstD3D11Device
 * @in_info: a #GstVideoInfo
 * @out_info: a #GstVideoInfo
 * @method: (inout) (optional) (nullable): a #GstD3D11ConverterMethod

 * Create a new converter object to convert between @in_info and @out_info
 * with @method. When @method is not specified, converter will configure
 * conversion path for all available method. Otherwise, converter will configure
 * conversion path only for specified method(s) and set @method will be updated
 * with supported method.
 *
 * Returns: (transfer full) (nullable): a #GstD3D11Converter or %NULL if conversion is not possible
 *
 * Since: 1.22
 */
GstD3D11Converter *
gst_d3d11_converter_new (GstD3D11Device * device, const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstStructure * config)
{
  GstD3D11Converter *self;
  GstD3D11ConverterPrivate *priv;
  GstD3D11Format in_d3d11_format;
  GstD3D11Format out_d3d11_format;
  guint wanted_backend = 0;
  gboolean allow_gamma = FALSE;
  gboolean allow_primaries = FALSE;
  D3D11_FILTER sampler_filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  gchar *backend_str;
  GstVideoInfo matrix_in_info;
  GstVideoInfo matrix_out_info;
  GstVideoFormat org_format;
  GstVideoFormat shader_format;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);
  g_return_val_if_fail (in_info != nullptr, nullptr);
  g_return_val_if_fail (out_info != nullptr, nullptr);

  self = (GstD3D11Converter *) g_object_new (GST_TYPE_D3D11_CONVERTER, nullptr);
  gst_object_ref_sink (self);
  priv = self->priv;

  if (config) {
    gint value;
    gst_structure_get_flags (config, GST_D3D11_CONVERTER_OPT_BACKEND,
        GST_TYPE_D3D11_CONVERTER_BACKEND, &wanted_backend);

    if (gst_structure_get_enum (config, GST_D3D11_CONVERTER_OPT_GAMMA_MODE,
            GST_TYPE_VIDEO_GAMMA_MODE, &value) &&
        (GstVideoGammaMode) value != GST_VIDEO_GAMMA_MODE_NONE) {
      allow_gamma = TRUE;
    }

    if (gst_structure_get_enum (config, GST_D3D11_CONVERTER_OPT_PRIMARIES_MODE,
            GST_TYPE_VIDEO_PRIMARIES_MODE, &value) &&
        (GstVideoPrimariesMode) value != GST_VIDEO_PRIMARIES_MODE_NONE) {
      allow_primaries = TRUE;
    }

    gst_structure_get_enum (config, GST_D3D11_CONVERTER_OPT_SAMPLER_FILTER,
        GST_TYPE_D3D11_CONVERTER_SAMPLER_FILTER, (int *) &sampler_filter);

    gst_structure_get_enum (config, GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE,
        GST_TYPE_D3D11_CONVERTER_ALPHA_MODE, (int *) &priv->src_alpha_mode);
    gst_structure_get_enum (config, GST_D3D11_CONVERTER_OPT_DEST_ALPHA_MODE,
        GST_TYPE_D3D11_CONVERTER_ALPHA_MODE, (int *) &priv->dst_alpha_mode);

    gst_structure_free (config);
  }

  if (!wanted_backend) {
    wanted_backend =
        GST_D3D11_CONVERTER_BACKEND_SHADER |
        GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR;
  }

  backend_str = g_flags_to_string (GST_TYPE_D3D11_CONVERTER_BACKEND,
      wanted_backend);

  GST_DEBUG_OBJECT (self,
      "Setup converter with format %s -> %s, wanted backend: %s, "
      "allow gamma conversion: %d, allow primaries conversion: %d ",
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
      gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)),
      backend_str, allow_gamma, allow_primaries);
  g_free (backend_str);

  if (!gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (in_info),
          &in_d3d11_format)) {
    GST_ERROR_OBJECT (self, "%s couldn't be converted to d3d11 format",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)));
    gst_object_unref (self);
    return nullptr;
  }

  if (!gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (out_info),
          &out_d3d11_format)) {
    GST_ERROR_OBJECT (self, "%s couldn't be converted to d3d11 format",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)));
    gst_object_unref (self);
    return nullptr;
  }

  self->device = (GstD3D11Device *) gst_object_ref (device);
  priv->const_data.alpha = 1.0;
  priv->in_info = *in_info;
  priv->preproc_info = *in_info;
  priv->piv_info = *in_info;
  priv->out_info = *out_info;
  priv->postproc_info = *out_info;
  priv->in_d3d11_format = in_d3d11_format;
  priv->out_d3d11_format = out_d3d11_format;

  /* Init properties */
  priv->src_width = GST_VIDEO_INFO_WIDTH (in_info);
  priv->src_height = GST_VIDEO_INFO_HEIGHT (in_info);
  priv->dest_width = GST_VIDEO_INFO_WIDTH (out_info);
  priv->dest_height = GST_VIDEO_INFO_HEIGHT (out_info);
  priv->alpha = 1.0;
  for (guint i = 0; i < G_N_ELEMENTS (priv->blend_factor); i++)
    priv->blend_factor[i] = 1.0;
  priv->blend_sample_mask = 0xffffffff;
  priv->border_color = 0xffff000000000000;

  /* Preprocess packed and subsampled texture */
  org_format = GST_VIDEO_INFO_FORMAT (in_info);
  shader_format = get_shader_format (org_format, TRUE);
  if (org_format != shader_format) {
    GstVideoInfo tmp_info;

    gst_video_info_set_interlaced_format (&tmp_info, shader_format,
        GST_VIDEO_INFO_INTERLACE_MODE (in_info),
        GST_VIDEO_INFO_WIDTH (in_info), GST_VIDEO_INFO_HEIGHT (in_info));
    tmp_info.chroma_site = in_info->chroma_site;
    tmp_info.colorimetry = in_info->colorimetry;
    tmp_info.fps_n = in_info->fps_n;
    tmp_info.fps_d = in_info->fps_d;
    tmp_info.par_n = in_info->par_n;
    tmp_info.par_d = in_info->par_d;

    priv->preproc_info = tmp_info;
    priv->need_preproc = TRUE;
  }

  org_format = GST_VIDEO_INFO_FORMAT (out_info);
  shader_format = get_shader_format (org_format, FALSE);
  if (org_format != shader_format) {
    GstVideoInfo tmp_info;

    gst_video_info_set_interlaced_format (&tmp_info, shader_format,
        GST_VIDEO_INFO_INTERLACE_MODE (out_info),
        GST_VIDEO_INFO_WIDTH (out_info), GST_VIDEO_INFO_HEIGHT (out_info));
    tmp_info.chroma_site = out_info->chroma_site;
    tmp_info.colorimetry = out_info->colorimetry;
    tmp_info.fps_n = out_info->fps_n;
    tmp_info.fps_d = out_info->fps_d;
    tmp_info.par_n = out_info->par_n;
    tmp_info.par_d = out_info->par_d;

    priv->postproc_info = tmp_info;
    priv->need_postproc = TRUE;
  }

  if (GST_VIDEO_INFO_IS_RGB (&priv->postproc_info)) {
    GstVideoInfo rgb_info = priv->postproc_info;
    rgb_info.colorimetry.range = GST_VIDEO_COLOR_RANGE_0_255;
    gst_d3d11_color_range_adjust_matrix_unorm (&rgb_info, &priv->postproc_info,
        &priv->clear_color_matrix);
  } else {
    GstVideoInfo rgb_info;
    GstVideoInfo yuv_info;

    gst_video_info_set_format (&rgb_info, GST_VIDEO_FORMAT_RGBA64_LE,
        priv->postproc_info.width, priv->postproc_info.height);
    convert_info_gray_to_yuv (&priv->postproc_info, &yuv_info);

    if (yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        yuv_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
      GST_WARNING_OBJECT (self, "Invalid matrix is detected");
      yuv_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }

    gst_d3d11_rgb_to_yuv_matrix_unorm (&rgb_info,
        &yuv_info, &priv->clear_color_matrix);
  }

  gst_d3d11_converter_calculate_border_color (self);

  if ((wanted_backend & GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR) != 0) {
    if (gst_d3d11_converter_setup_processor (self)) {
      GST_DEBUG_OBJECT (self, "Video processor is available");
      priv->supported_backend |= GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR;
    }
  }

  if ((wanted_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0)
    goto out;

  priv->convert_type = CONVERT_TYPE::IDENTITY;
  if (GST_VIDEO_INFO_IS_RGB (in_info) != GST_VIDEO_INFO_IS_RGB (out_info)) {
    priv->convert_type = CONVERT_TYPE::SIMPLE;
  } else if (in_info->colorimetry.range != GST_VIDEO_COLOR_RANGE_UNKNOWN &&
      out_info->colorimetry.range != GST_VIDEO_COLOR_RANGE_UNKNOWN &&
      in_info->colorimetry.range != out_info->colorimetry.range) {
    priv->convert_type = CONVERT_TYPE::RANGE;
  }

  if (allow_gamma &&
      in_info->colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN &&
      out_info->colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN &&
      !gst_video_transfer_function_is_equivalent (in_info->colorimetry.transfer,
          GST_VIDEO_INFO_COMP_DEPTH (in_info, 0),
          out_info->colorimetry.transfer, GST_VIDEO_INFO_COMP_DEPTH (out_info,
              0))) {
    GST_DEBUG_OBJECT (self, "Different transfer function %d -> %d",
        in_info->colorimetry.transfer, out_info->colorimetry.transfer);
    priv->convert_type = CONVERT_TYPE::GAMMA;
  }

  if (allow_primaries &&
      in_info->colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN &&
      out_info->colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN &&
      !gst_video_color_primaries_is_equivalent (in_info->colorimetry.primaries,
          out_info->colorimetry.primaries)) {
    GST_DEBUG_OBJECT (self, "Different primaries %d -> %d",
        in_info->colorimetry.primaries, out_info->colorimetry.primaries);
    priv->convert_type = CONVERT_TYPE::PRIMARY;
  }

  if ((priv->convert_type == CONVERT_TYPE::GAMMA ||
          priv->convert_type == CONVERT_TYPE::PRIMARY)) {
    if (!gst_d3d11_converter_setup_lut (self, &priv->preproc_info,
            &priv->postproc_info))
      goto out;
  }

  if (GST_VIDEO_INFO_IS_RGB (&priv->preproc_info)) {
    matrix_in_info = priv->preproc_info;
  } else {
    convert_info_gray_to_yuv (&priv->preproc_info, &matrix_in_info);
    if (matrix_in_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        matrix_in_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
      matrix_in_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }
  }

  if (GST_VIDEO_INFO_IS_RGB (&priv->postproc_info)) {
    matrix_out_info = priv->postproc_info;
  } else {
    convert_info_gray_to_yuv (&priv->postproc_info, &matrix_out_info);
    if (matrix_out_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
        matrix_out_info.colorimetry.matrix == GST_VIDEO_COLOR_MATRIX_RGB) {
      matrix_out_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_BT709;
    }
  }

  if (!gst_d3d11_converter_calculate_matrix (self,
          &matrix_in_info, &matrix_out_info)) {
    goto out;
  }

  if (!gst_d3d11_color_convert_setup_shader (self, &priv->preproc_info,
          &priv->postproc_info, sampler_filter)) {
    goto out;
  }

  priv->preproc = gst_d3d11_converter_helper_new (self->device,
      GST_VIDEO_INFO_FORMAT (in_info),
      GST_VIDEO_INFO_FORMAT (&priv->preproc_info), in_info->width,
      in_info->height);

  priv->postproc = gst_d3d11_converter_helper_new (self->device,
      GST_VIDEO_INFO_FORMAT (&priv->postproc_info),
      GST_VIDEO_INFO_FORMAT (out_info), out_info->width, out_info->height);

  priv->supported_backend |= GST_D3D11_CONVERTER_BACKEND_SHADER;

out:
  if (priv->supported_backend == 0) {
    GST_ERROR_OBJECT (self, "Conversion %s to %s not supported",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)),
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)));
    gst_object_unref (self);
    return nullptr;
  }

  return self;
}

static gboolean
gst_d3d11_converter_convert_internal (GstD3D11Converter * self,
    ID3D11ShaderResourceView * srv[GST_VIDEO_MAX_PLANES],
    ID3D11RenderTargetView * rtv[GST_VIDEO_MAX_PLANES], gboolean multisampled)
{
  GstD3D11ConverterPrivate *priv;
  ComPtr < ID3D11Resource > resource;
  ComPtr < ID3D11Texture2D > texture;
  D3D11_TEXTURE2D_DESC desc;
  ID3D11DeviceContext *context;
  UINT offsets = 0;
  UINT vertex_stride = sizeof (VertexData);
  ID3D11ShaderResourceView *clear_view[GST_VIDEO_MAX_PLANES] = { nullptr, };

  priv = self->priv;
  context = gst_d3d11_device_get_device_context_handle (self->device);

  /* check texture resolution and update crop area */
  srv[0]->GetResource (&resource);
  resource.As (&texture);
  texture->GetDesc (&desc);

  if (desc.Width != (guint) priv->input_texture_width ||
      desc.Height != (guint) priv->input_texture_height) {
    GST_DEBUG_OBJECT (self, "Update vertext buffer, texture resolution: %dx%d",
        desc.Width, desc.Height);

    priv->input_texture_width = desc.Width;
    priv->input_texture_height = desc.Height;
    priv->update_src_rect = TRUE;

    if (!gst_d3d11_converter_update_src_rect (self)) {
      GST_ERROR_OBJECT (self, "Cannot update src rect");
      return FALSE;
    }
  }

  if (priv->const_buffer && priv->update_alpha) {
    D3D11_MAPPED_SUBRESOURCE map;
    PSConstBuffer *const_buffer;
    HRESULT hr;

    hr = context->Map (priv->const_buffer.Get (),
        0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self,
          "Couldn't map constant buffer, hr: 0x%x", (guint) hr);
      return FALSE;
    }

    const_buffer = (PSConstBuffer *) map.pData;
    memcpy (const_buffer, &priv->const_data, sizeof (PSConstBuffer));

    context->Unmap (priv->const_buffer.Get (), 0);
  }
  priv->update_alpha = FALSE;

  if (priv->clear_background) {
    for (guint i = 0; i < priv->num_output_view; i++)
      context->ClearRenderTargetView (rtv[i], priv->clear_color[i]);
  }

  context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D11Buffer *vertex[] = { priv->vertex_buffer.Get () };
  context->IASetVertexBuffers (0, 1, vertex, &vertex_stride, &offsets);
  context->IASetIndexBuffer (priv->index_buffer.Get (),
      DXGI_FORMAT_R16_UINT, 0);

  ID3D11SamplerState *samplers[] = { priv->sampler.Get (),
    priv->linear_sampler.Get ()
  };
  context->PSSetSamplers (0, 2, samplers);

  ID3D11Buffer *vs_const_buffer[] = { priv->vs_const_buffer.Get () };
  context->IASetInputLayout (priv->layout.Get ());
  context->VSSetShader (priv->vs.Get (), nullptr, 0);
  context->VSSetConstantBuffers (0, 1, vs_const_buffer);

  if (priv->const_buffer) {
    ID3D11Buffer *const_buffer[] = { priv->const_buffer.Get () };
    context->PSSetConstantBuffers (0, 1, const_buffer);
  }

  context->PSSetShaderResources (0, priv->num_input_view, srv);
  if (priv->gamma_enc_srv) {
    ID3D11ShaderResourceView *gamma_srv[] = { priv->gamma_dec_srv.Get (),
      priv->gamma_enc_srv.Get ()
    };
    context->PSSetShaderResources (4, 2, gamma_srv);
  }

  auto ps = priv->ps[0];
  context->PSSetShader (ps->shader.Get (), nullptr, 0);
  context->RSSetViewports (ps->num_rtv, priv->viewport);
  if (multisampled)
    context->RSSetState (priv->msaa_rasterizer.Get ());
  else
    context->RSSetState (priv->rasterizer.Get ());

  context->OMSetRenderTargets (ps->num_rtv, rtv, nullptr);
  if (priv->blend) {
    context->OMSetBlendState (priv->blend.Get (),
        priv->blend_factor, priv->blend_sample_mask);
  } else {
    context->OMSetBlendState (nullptr, nullptr, 0xffffffff);
  }
  context->DrawIndexed (6, 0, 0);

  if (priv->ps.size () == 2) {
    guint view_offset = ps->num_rtv;
    ps = priv->ps[1];

    context->PSSetShader (ps->shader.Get (), nullptr, 0);
    context->RSSetViewports (ps->num_rtv, &priv->viewport[view_offset]);
    context->OMSetRenderTargets (ps->num_rtv, &rtv[view_offset], nullptr);
    context->DrawIndexed (6, 0, 0);
  }

  context->PSSetShaderResources (0, 4, clear_view);
  context->OMSetRenderTargets (0, nullptr, nullptr);

  return TRUE;
}

static gboolean
gst_d3d11_converter_check_bind_flags_for_piv (guint bind_flags)
{
  static const guint flags = (D3D11_BIND_DECODER |
      D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_RENDER_TARGET |
      D3D11_BIND_UNORDERED_ACCESS);

  if (bind_flags == 0)
    return TRUE;

  if ((bind_flags & flags) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
gst_d3d11_converter_is_d3d11_buffer (GstD3D11Converter * self,
    GstBuffer * buffer)
{
  if (gst_buffer_n_memory (buffer) == 0) {
    GST_WARNING_OBJECT (self, "Empty buffer");
    return FALSE;
  }

  for (guint i = 0; i < gst_buffer_n_memory (buffer); i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);
    GstD3D11Memory *dmem;

    if (!gst_is_d3d11_memory (mem)) {
      GST_LOG_OBJECT (self, "Memory at %d is not d3d11 memory", i);
      return FALSE;
    }

    dmem = GST_D3D11_MEMORY_CAST (mem);
    if (dmem->device != self->device) {
      GST_LOG_OBJECT (self, "Memory at %d belongs to different device", i);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_d3d11_converter_map_buffer (GstD3D11Converter * self, GstBuffer * buffer,
    GstMapInfo info[GST_VIDEO_MAX_PLANES], GstMapFlags flags)
{
  GstMapFlags map_flags;
  guint num_mapped = 0;

  map_flags = (GstMapFlags) (flags | GST_MAP_D3D11);

  for (num_mapped = 0; num_mapped < gst_buffer_n_memory (buffer); num_mapped++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, num_mapped);

    if (!gst_memory_map (mem, &info[num_mapped], map_flags)) {
      GST_WARNING_OBJECT (self, "Failed to map memory at %d", num_mapped);
      goto error;
    }
  }

  return TRUE;

error:
  for (guint i = 0; i < num_mapped; i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);
    gst_memory_unmap (mem, &info[i]);
  }

  return FALSE;
}

static void
gst_d3d11_converter_unmap_buffer (GstD3D11Converter * self, GstBuffer * buffer,
    GstMapInfo info[GST_VIDEO_MAX_PLANES])
{
  for (guint i = 0; i < gst_buffer_n_memory (buffer); i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);

    gst_memory_unmap (mem, &info[i]);
  }
}

static guint
gst_d3d11_converter_get_srv (GstD3D11Converter * self, GstBuffer * buffer,
    ID3D11ShaderResourceView * srv[GST_VIDEO_MAX_PLANES])
{
  guint num_views = 0;

  for (guint i = 0; i < gst_buffer_n_memory (buffer); i++) {
    GstD3D11Memory *mem = (GstD3D11Memory *) gst_buffer_peek_memory (buffer, i);
    guint num_view_in_mem;

    num_view_in_mem = gst_d3d11_memory_get_shader_resource_view_size (mem);
    if (!num_view_in_mem)
      return 0;

    for (guint j = 0; j < num_view_in_mem; j++) {
      if (num_views >= GST_VIDEO_MAX_PLANES) {
        GST_ERROR_OBJECT (self, "Too many SRV");
        return 0;
      }

      srv[num_views] = gst_d3d11_memory_get_shader_resource_view (mem, j);
      num_views++;
    }
  }

  return num_views;
}

static guint
gst_d3d11_converter_get_rtv (GstD3D11Converter * self, GstBuffer * buffer,
    ID3D11RenderTargetView * rtv[GST_VIDEO_MAX_PLANES])
{
  guint num_views = 0;

  for (guint i = 0; i < gst_buffer_n_memory (buffer); i++) {
    GstD3D11Memory *mem = (GstD3D11Memory *) gst_buffer_peek_memory (buffer, i);
    guint num_view_in_mem;

    num_view_in_mem = gst_d3d11_memory_get_render_target_view_size (mem);
    if (!num_view_in_mem)
      return 0;

    for (guint j = 0; j < num_view_in_mem; j++) {
      if (num_views >= GST_VIDEO_MAX_PLANES) {
        GST_ERROR_OBJECT (self, "Too many SRV");
        return 0;
      }

      rtv[num_views] = gst_d3d11_memory_get_render_target_view (mem, j);
      num_views++;
    }
  }

  return num_views;
}

static void
gst_d3d11_converter_fill_hdr10_meta (const GstVideoMasteringDisplayInfo * mdcv,
    const GstVideoContentLightLevel * cll, DXGI_HDR_METADATA_HDR10 * meta)
{
  meta->RedPrimary[0] = mdcv->display_primaries[0].x;
  meta->RedPrimary[1] = mdcv->display_primaries[0].y;
  meta->GreenPrimary[0] = mdcv->display_primaries[1].x;
  meta->GreenPrimary[1] = mdcv->display_primaries[1].y;
  meta->BluePrimary[0] = mdcv->display_primaries[2].x;
  meta->BluePrimary[1] = mdcv->display_primaries[2].y;
  meta->WhitePoint[0] = mdcv->white_point.x;
  meta->WhitePoint[1] = mdcv->white_point.y;
  meta->MaxMasteringLuminance = mdcv->max_display_mastering_luminance;
  meta->MinMasteringLuminance = mdcv->min_display_mastering_luminance;

  meta->MaxContentLightLevel = cll->max_content_light_level;
  meta->MaxFrameAverageLightLevel = cll->max_frame_average_light_level;
}

/* called with prop lock */
static void
gst_d3d11_converter_update_hdr10_meta (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if (priv->in_hdr10_updated) {
    if (!priv->in_mdcv_str || !priv->in_cll_str) {
      priv->have_in_hdr10 = FALSE;
    } else {
      GstVideoMasteringDisplayInfo mdcv;
      GstVideoContentLightLevel cll;

      if (gst_video_mastering_display_info_from_string (&mdcv,
              priv->in_mdcv_str) &&
          gst_video_content_light_level_from_string (&cll, priv->in_cll_str)) {
        gst_d3d11_converter_fill_hdr10_meta (&mdcv, &cll, &priv->in_hdr10_meta);
        priv->have_in_hdr10 = TRUE;
      } else {
        priv->have_in_hdr10 = FALSE;
      }
    }

    priv->in_hdr10_updated = FALSE;
  }

  if (priv->out_hdr10_updated) {
    if (!priv->in_mdcv_str || !priv->in_cll_str) {
      priv->have_out_hdr10 = FALSE;
    } else {
      GstVideoMasteringDisplayInfo mdcv;
      GstVideoContentLightLevel cll;

      if (gst_video_mastering_display_info_from_string (&mdcv,
              priv->in_mdcv_str) &&
          gst_video_content_light_level_from_string (&cll, priv->in_cll_str)) {
        gst_d3d11_converter_fill_hdr10_meta (&mdcv, &cll, &priv->in_hdr10_meta);
        priv->have_out_hdr10 = TRUE;
      } else {
        priv->have_out_hdr10 = FALSE;
      }
    }

    priv->out_hdr10_updated = FALSE;
  }
}

static gboolean
gst_d3d11_converter_need_blend (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if (priv->blend && priv->blend_desc.RenderTarget[0].BlendEnable) {
    if (priv->alpha != 1.0) {
      return TRUE;
    } else if ((priv->blend_desc.RenderTarget[0].SrcBlend ==
            D3D11_BLEND_BLEND_FACTOR
            || priv->blend_desc.RenderTarget[0].SrcBlend ==
            D3D11_BLEND_INV_BLEND_FACTOR)
        && (priv->blend_factor[0] != 1.0 || priv->blend_factor[1] != 1.0
            || priv->blend_factor[2] != 1.0 || priv->blend_factor[3] != 1.0)) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
gst_d3d11_converter_processor_available (GstD3D11Converter * self)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if ((priv->supported_backend &
          GST_D3D11_CONVERTER_BACKEND_VIDEO_PROCESSOR) == 0)
    return FALSE;

  /* TODO: processor may be able to blend textures */
  if (gst_d3d11_converter_need_blend (self))
    return FALSE;

  /* flip/rotate or affine transform is not supported by processor */
  if (priv->processor_direction_not_supported ||
      priv->video_direction == GST_VIDEO_ORIENTATION_CUSTOM) {
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_converter_piv_available (GstD3D11Converter * self, GstBuffer * in_buf)
{
  GstD3D11Memory *mem;
  D3D11_TEXTURE2D_DESC desc;

  mem = (GstD3D11Memory *) gst_buffer_peek_memory (in_buf, 0);
  gst_d3d11_memory_get_texture_desc (mem, &desc);
  return gst_d3d11_converter_check_bind_flags_for_piv (desc.BindFlags);
}

static GstBuffer *
gst_d3d11_converter_create_buffer (GstD3D11Converter * self,
    const GstVideoInfo * info, guint bind_flags)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstD3D11AllocationParams *params;
  GstBufferPool *pool;
  GstCaps *caps;
  GstStructure *config;
  GstBuffer *buf = nullptr;

  params = gst_d3d11_allocation_params_new (self->device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);

  caps = gst_video_info_to_caps (&priv->piv_info);
  pool = gst_d3d11_buffer_pool_new (self->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, info->size, 0, 0);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_caps_unref (caps);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);
    return nullptr;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to set active");
    gst_object_unref (pool);
    return nullptr;
  }

  gst_buffer_pool_acquire_buffer (pool, &buf, nullptr);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);

  return buf;
}

static gboolean
gst_d3d11_converter_upload_for_processor (GstD3D11Converter * self,
    GstBuffer * in_buf)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  GstVideoFrame frame, fallback_frame;
  GstVideoInfo *piv_info = &priv->piv_info;
  gboolean ret = TRUE;

  if (!gst_video_frame_map (&frame, &priv->in_info, in_buf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map input buffer");
    return FALSE;
  }

  /* Probably cropped buffer */
  if (piv_info->width != GST_VIDEO_FRAME_WIDTH (&frame) ||
      piv_info->height != GST_VIDEO_FRAME_HEIGHT (&frame)) {
    gst_clear_buffer (&priv->piv_inbuf);

    *piv_info = frame.info;
  }

  if (!priv->piv_inbuf)
    priv->piv_inbuf = gst_d3d11_converter_create_buffer (self, piv_info, 0);

  if (!priv->piv_inbuf)
    goto error;

  if (!gst_video_frame_map (&fallback_frame,
          &priv->piv_info, priv->piv_inbuf, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (self, "Couldn't map fallback buffer");
    goto error;
  }

  ret = gst_video_frame_copy (&fallback_frame, &frame);
  gst_video_frame_unmap (&fallback_frame);
  gst_video_frame_unmap (&frame);

  return ret;

error:
  gst_video_frame_unmap (&frame);
  return FALSE;
}

static gboolean
gst_d3d11_converter_do_processor_blt (GstD3D11Converter * self,
    GstBuffer * in_buf, GstBuffer * out_buf)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  ID3D11VideoProcessorInputView *piv = nullptr;
  ID3D11VideoProcessorOutputView *pov = nullptr;
  ID3D11VideoContext1 *video_ctx = priv->video_context.Get ();
  ID3D11VideoProcessor *proc = priv->processor.Get ();
  D3D11_VIDEO_PROCESSOR_STREAM stream = { 0, };
  HRESULT hr;
  GstMemory *in_mem, *out_mem;
  GstD3D11Memory *in_dmem;
  GstD3D11Memory *out_dmem;
  GstMapInfo in_info, out_info;
  gboolean ret = FALSE;

  g_assert (gst_buffer_n_memory (in_buf) == 1);
  g_assert (gst_buffer_n_memory (out_buf) == 1);

  in_mem = gst_buffer_peek_memory (in_buf, 0);
  out_mem = gst_buffer_peek_memory (out_buf, 0);

  if (!gst_memory_map (in_mem, &in_info,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map input buffer");
    return FALSE;
  }

  if (!gst_memory_map (out_mem, &out_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map output buffer");
    gst_memory_unmap (in_mem, &in_info);
    return FALSE;
  }

  in_dmem = GST_D3D11_MEMORY_CAST (in_mem);
  out_dmem = GST_D3D11_MEMORY_CAST (out_mem);

  piv = gst_d3d11_memory_get_processor_input_view (in_dmem,
      priv->video_device.Get (), priv->enumerator.Get ());
  if (!piv) {
    GST_ERROR_OBJECT (self, "PIV is unavailable");
    goto out;
  }

  pov = gst_d3d11_memory_get_processor_output_view (out_dmem,
      priv->video_device.Get (), priv->enumerator.Get ());
  if (!pov) {
    GST_ERROR_OBJECT (self, "POV is unavailable");
    goto out;
  }

  video_ctx->VideoProcessorSetStreamSourceRect (proc, 0, TRUE, &priv->src_rect);
  video_ctx->VideoProcessorSetStreamDestRect (proc, 0, TRUE, &priv->dest_rect);

  if (priv->clear_background) {
    video_ctx->VideoProcessorSetOutputTargetRect (proc,
        TRUE, &priv->dest_full_rect);
    video_ctx->VideoProcessorSetOutputBackgroundColor (proc,
        GST_VIDEO_INFO_IS_YUV (&priv->out_info), &priv->background_color);
  } else {
    video_ctx->VideoProcessorSetOutputTargetRect (proc, TRUE, &priv->dest_rect);
  }

  if (priv->video_context2 &&
      (priv->processor_caps.FeatureCaps & FEATURE_CAPS_METADATA_HDR10) != 0) {
    if (priv->have_in_hdr10) {
      priv->video_context2->VideoProcessorSetStreamHDRMetaData (proc, 0,
          DXGI_HDR_METADATA_TYPE_HDR10, sizeof (DXGI_HDR_METADATA_HDR10),
          &priv->in_hdr10_meta);
    } else {
      priv->video_context2->VideoProcessorSetStreamHDRMetaData (proc, 0,
          DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
    }

    if (priv->have_out_hdr10) {
      priv->video_context2->VideoProcessorSetOutputHDRMetaData (proc,
          DXGI_HDR_METADATA_TYPE_HDR10, sizeof (DXGI_HDR_METADATA_HDR10),
          &priv->in_hdr10_meta);
    }
  }

  if ((priv->processor_caps.FeatureCaps & FEATURE_CAPS_ROTATION) != 0) {
    video_ctx->VideoProcessorSetStreamRotation (proc, 0,
        priv->enable_rotation, priv->rotation);
  }

  if ((priv->processor_caps.FeatureCaps & PROCESSOR_FEATURE_CAPS_MIRROR) != 0) {
    video_ctx->VideoProcessorSetStreamMirror (proc, 0, priv->enable_mirror,
        priv->flip_h, priv->flip_v);
  }

  stream.Enable = TRUE;
  stream.pInputSurface = piv;

  GST_TRACE_OBJECT (self, "Converting using processor");

  hr = video_ctx->VideoProcessorBlt (proc, pov, 0, 1, &stream);
  ret = gst_d3d11_result (hr, self->device);

  priv->processor_in_use = ret;

out:
  gst_memory_unmap (out_mem, &out_info);
  gst_memory_unmap (in_mem, &in_info);

  return ret;
}

static gboolean
gst_d3d11_converter_ensure_postproc_buf (GstD3D11Converter * self,
    const D3D11_TEXTURE2D_DESC * desc)
{
  GstD3D11ConverterPrivate *priv = self->priv;

  if (desc->Width != (UINT) priv->postproc_info.width ||
      desc->Height != (UINT) priv->postproc_info.height) {
    gst_video_info_set_format (&priv->postproc_info,
        GST_VIDEO_INFO_FORMAT (&priv->postproc_info),
        desc->Width, desc->Height);
    gst_clear_buffer (&priv->postproc_buf);
  }

  if (!priv->postproc_buf) {
    priv->postproc_buf = gst_d3d11_converter_create_buffer (self,
        &priv->postproc_info,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

    if (!priv->postproc_buf)
      return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_converter_convert_buffer_internal (GstD3D11Converter * self,
    GstBuffer * in_buf, GstBuffer * out_buf)
{
  GstD3D11ConverterPrivate *priv = self->priv;
  ID3D11ShaderResourceView *srv[GST_VIDEO_MAX_PLANES] = { nullptr, };
  ID3D11RenderTargetView *rtv[GST_VIDEO_MAX_PLANES] = { nullptr, };
  GstD3D11Memory *in_dmem;
  GstD3D11Memory *out_dmem;
  GstMapInfo in_info[GST_VIDEO_MAX_PLANES];
  GstMapInfo out_info[GST_VIDEO_MAX_PLANES];
  D3D11_TEXTURE2D_DESC desc;
  guint num_srv, num_rtv;
  gboolean ret = FALSE;
  gboolean in_d3d11;
  gboolean multisampled = FALSE;
  gboolean rtv_bound = TRUE;
  GstBuffer *shader_output = out_buf;

  std::lock_guard < std::mutex > lk (priv->prop_lock);

  /* Output buffer must be valid D3D11 buffer */
  if (!gst_d3d11_converter_is_d3d11_buffer (self, out_buf)) {
    GST_ERROR_OBJECT (self, "Output is not d3d11 buffer");
    return FALSE;
  }

  if (gst_buffer_n_memory (in_buf) == 0) {
    GST_ERROR_OBJECT (self, "Empty input buffer");
    return FALSE;
  }

  out_dmem = (GstD3D11Memory *) gst_buffer_peek_memory (out_buf, 0);
  if (!gst_d3d11_memory_get_texture_desc (out_dmem, &desc)) {
    GST_ERROR_OBJECT (self, "Failed to get output desc");
    return FALSE;
  }

  if ((desc.BindFlags & D3D11_BIND_RENDER_TARGET) == 0)
    rtv_bound = FALSE;

  if (desc.SampleDesc.Count > 1)
    multisampled = TRUE;

  gst_d3d11_converter_update_hdr10_meta (self);
  /* Update in/out rect */
  if (!gst_d3d11_converter_update_dest_rect (self)) {
    GST_ERROR_OBJECT (self, "Failed to update dest rect");
    return FALSE;
  }

  if (!gst_d3d11_converter_update_src_rect (self)) {
    GST_ERROR_OBJECT (self, "Failed to update src rect");
    return FALSE;
  }

  if (!gst_d3d11_converter_update_transform (self)) {
    GST_ERROR_OBJECT (self, "Failed to update transform matrix");
    return FALSE;
  }

  in_d3d11 = gst_d3d11_converter_is_d3d11_buffer (self, in_buf);
  if (!multisampled && rtv_bound &&
      gst_d3d11_converter_processor_available (self)) {
    gboolean use_processor = FALSE;
    gboolean piv_available = FALSE;

    if (in_d3d11)
      piv_available = gst_d3d11_converter_piv_available (self, in_buf);

    if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0) {
      /* processor only */
      use_processor = TRUE;
    } else if ((priv->src_alpha_mode ==
            GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED ||
            priv->dst_alpha_mode ==
            GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED)
        && GST_VIDEO_INFO_HAS_ALPHA (&priv->in_info)) {
      /* Needs alpha conversion */
      use_processor = FALSE;
    } else if (piv_available) {
      in_dmem = (GstD3D11Memory *) gst_buffer_peek_memory (in_buf, 0);

      if (priv->need_preproc || priv->need_postproc) {
        /* Prefer processor for packed YUV */
        use_processor = TRUE;
      } else if (!gst_d3d11_memory_get_shader_resource_view_size (in_dmem)) {
        /* SRV is unavailable, use processor */
        use_processor = TRUE;
      } else if (priv->video_context2 &&
          (priv->have_in_hdr10 || priv->have_out_hdr10)) {
        /* HDR10 tonemap is needed */
        use_processor = TRUE;
      } else if (priv->processor_in_use) {
        use_processor = TRUE;
      }
    }

    if (use_processor) {
      if (!piv_available) {
        if (!gst_d3d11_converter_upload_for_processor (self, in_buf)) {
          GST_ERROR_OBJECT (self, "Couldn't upload buffer");
          return FALSE;
        }

        in_buf = priv->piv_inbuf;
      }

      return gst_d3d11_converter_do_processor_blt (self, in_buf, out_buf);
    }
  }

  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0) {
    GST_ERROR_OBJECT (self, "Conversion is not supported");
    return FALSE;
  }

  in_buf = gst_d3d11_converter_helper_preproc (priv->preproc, in_buf);
  if (!in_buf) {
    GST_ERROR_OBJECT (self, "Preprocess failed");
    return FALSE;
  }

  if (!rtv_bound || priv->need_postproc) {
    if (!gst_d3d11_converter_ensure_postproc_buf (self, &desc)) {
      GST_ERROR_OBJECT (self, "Postproc buffer failed");
      return FALSE;
    }

    gst_d3d11_converter_helper_update_size (priv->postproc,
        desc.Width, desc.Height);
    shader_output = priv->postproc_buf;
  }

  if (!gst_d3d11_converter_map_buffer (self, in_buf, in_info,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map input buffer");
    return FALSE;
  }

  if (!gst_d3d11_converter_map_buffer (self, shader_output, out_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Couldn't map output buffer");
    gst_d3d11_converter_unmap_buffer (self, in_buf, in_info);
    return FALSE;
  }

  num_rtv = gst_d3d11_converter_get_rtv (self, shader_output, rtv);
  if (!num_rtv) {
    GST_ERROR_OBJECT (self, "RTV is unavailable");
    goto out;
  }

  num_srv = gst_d3d11_converter_get_srv (self, in_buf, srv);
  if (!num_srv) {
    GST_ERROR_OBJECT (self, "SRV is unavailable");
    goto out;
  }

  GST_TRACE_OBJECT (self, "Converting using shader");

  ret = gst_d3d11_converter_convert_internal (self, srv, rtv, multisampled);

out:
  gst_d3d11_converter_unmap_buffer (self, in_buf, in_info);
  gst_d3d11_converter_unmap_buffer (self, shader_output, out_info);

  if (ret) {
    ret = gst_d3d11_converter_helper_postproc (priv->postproc, shader_output,
        out_buf);
  }

  return ret;
}

/**
 * gst_d3d11_converter_convert_buffer:
 * @converter: a #GstD3D11Converter
 * @in_buf: a #GstBuffer
 * @out_buf: a #GstBuffer
 *
 * Converts @in_buf into @out_buf
 *
 * Returns: %TRUE if conversion is successful
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_converter_convert_buffer (GstD3D11Converter * converter,
    GstBuffer * in_buf, GstBuffer * out_buf)
{
  g_return_val_if_fail (GST_IS_D3D11_CONVERTER (converter), FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (in_buf), FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (out_buf), FALSE);

  GstD3D11DeviceLockGuard lk (converter->device);

  return gst_d3d11_converter_convert_buffer_internal (converter,
      in_buf, out_buf);
}

/**
 * gst_d3d11_converter_convert_buffer_unlocked:
 * @converter: a #GstD3D11Converter
 * @in_buf: a #GstBuffer
 * @out_buf: a #GstBuffer
 *
 * Converts @in_buf into @out_buf. Caller should take d3d11 device lock
 * in case that multiple threads can perform GPU processing using the
 * same #GstD3D11Device
 *
 * Returns: %TRUE if conversion is successful
 *
 * Since: 1.22
 */
gboolean
gst_d3d11_converter_convert_buffer_unlocked (GstD3D11Converter * converter,
    GstBuffer * in_buf, GstBuffer * out_buf)
{
  g_return_val_if_fail (GST_IS_D3D11_CONVERTER (converter), FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (in_buf), FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (out_buf), FALSE);

  return gst_d3d11_converter_convert_buffer_internal (converter,
      in_buf, out_buf);
}

/**
 * gst_d3d11_converter_set_transform_matrix:
 * @converter: a #GstD3D11Converter
 * @matrix: the row-major 4x4 transform matrix
 *
 * Apply transform matrix
 *
 * Returns: %TRUE if successful
 *
 * Since: 1.24
 */
gboolean
gst_d3d11_converter_set_transform_matrix (GstD3D11Converter * converter,
    const gfloat matrix[16])
{
  GstD3D11ConverterPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_CONVERTER (converter), FALSE);
  g_return_val_if_fail (matrix, FALSE);

  priv = converter->priv;
  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0) {
    GST_ERROR_OBJECT (converter, "Shader backend is disabled");
    return FALSE;
  }

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  priv->custom_transform = XMFLOAT4X4A (matrix);
  priv->update_transform = TRUE;

  return TRUE;
}

gboolean
gst_d3d11_converter_apply_transform (GstD3D11Converter * converter,
    GstVideoOrientationMethod method, gfloat viewport_width,
    gfloat viewport_height, gfloat fov, gboolean ortho, gfloat rotation_x,
    gfloat rotation_y, gfloat rotation_z, gfloat scale_x, gfloat scale_y)
{
  GstD3D11ConverterPrivate *priv;

  g_return_val_if_fail (GST_IS_D3D11_CONVERTER (converter), FALSE);

  priv = converter->priv;
  if ((priv->supported_backend & GST_D3D11_CONVERTER_BACKEND_SHADER) == 0) {
    GST_ERROR_OBJECT (converter, "Shader backend is disabled");
    return FALSE;
  }

  std::lock_guard < std::mutex > lk (priv->prop_lock);
  gfloat aspect_ratio;
  gboolean rotated = FALSE;
  XMMATRIX rotate_matrix = XMMatrixIdentity ();

  switch (method) {
    case GST_VIDEO_ORIENTATION_IDENTITY:
    case GST_VIDEO_ORIENTATION_AUTO:
    case GST_VIDEO_ORIENTATION_CUSTOM:
    default:
      break;
    case GST_VIDEO_ORIENTATION_90R:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_90r);
      rotated = TRUE;
      break;
    case GST_VIDEO_ORIENTATION_180:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_180);
      break;
    case GST_VIDEO_ORIENTATION_90L:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_90l);
      rotated = TRUE;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_horiz);
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_vert);
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_ul_lr);
      rotated = TRUE;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      rotate_matrix = XMLoadFloat4x4A (&g_matrix_ur_ll);
      rotated = TRUE;
      break;
  }

  if (rotated)
    aspect_ratio = viewport_height / viewport_width;
  else
    aspect_ratio = viewport_width / viewport_height;

  /* Apply user specified transform matrix first, then rotate-method */
  XMMATRIX scale = XMMatrixScaling (scale_x * aspect_ratio, scale_y, 1.0);

  XMMATRIX rotate =
      XMMatrixRotationX (XMConvertToRadians (rotation_x)) *
      XMMatrixRotationY (XMConvertToRadians (-rotation_y)) *
      XMMatrixRotationZ (XMConvertToRadians (-rotation_z));

  XMMATRIX view = XMMatrixLookAtLH (XMVectorSet (0.0, 0.0, -1.0, 0.0),
      XMVectorSet (0.0, 0.0, 0.0, 0.0), XMVectorSet (0.0, 1.0, 0.0, 0.0));

  XMMATRIX proj;
  if (ortho) {
    proj = XMMatrixOrthographicOffCenterLH (-aspect_ratio,
        aspect_ratio, -1.0, 1.0, 0.1, 100.0);
  } else {
    proj = XMMatrixPerspectiveFovLH (XMConvertToRadians (fov),
        aspect_ratio, 0.1, 100.0);
  }

  XMMATRIX mvp = scale * rotate * view * proj * rotate_matrix;
  XMStoreFloat4x4A (&priv->custom_transform, mvp);
  priv->update_transform = TRUE;
  priv->video_direction = GST_VIDEO_ORIENTATION_CUSTOM;

  return TRUE;
}
