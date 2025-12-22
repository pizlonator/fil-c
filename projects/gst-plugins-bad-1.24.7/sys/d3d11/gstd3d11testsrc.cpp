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

/**
 * SECTION:element-d3d11testsrc
 * @title: d3d11testsrc
 *
 * The videotestsrc element is used to produce test video data
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 d3d11testsrc ! queue ! d3d11videosink
 * ```
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d11testsrc.h"
#include "gstd3d11pluginutils.h"
#include <wrl.h>
#include <string.h>
#include <d2d1.h>
#include <math.h>
#include <memory>
#include <vector>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_test_src_debug);
#define GST_CAT_DEFAULT gst_d3d11_test_src_debug

static GstStaticCaps template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_SRC_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE (GST_D3D11_SRC_FORMATS));

typedef enum
{
  GST_D3D11_TEST_SRC_SMPTE,
  GST_D3D11_TEST_SRC_SNOW,
  GST_D3D11_TEST_SRC_BLACK,
  GST_D3D11_TEST_SRC_WHITE,
  GST_D3D11_TEST_SRC_RED,
  GST_D3D11_TEST_SRC_GREEN,
  GST_D3D11_TEST_SRC_BLUE,
  GST_D3D11_TEST_SRC_CHECKERS1,
  GST_D3D11_TEST_SRC_CHECKERS2,
  GST_D3D11_TEST_SRC_CHECKERS4,
  GST_D3D11_TEST_SRC_CHECKERS8,
  GST_D3D11_TEST_SRC_CIRCULAR,
  GST_D3D11_TEST_SRC_BLINK,
  /* sync with videotestsrc */
  GST_D3D11_TEST_SRC_BALL = 18,
} GstD3D11TestSrcPattern;

/**
 * GstD3D11TestSrcPattern:
 *
 * Since: 1.22
 */
#define GST_TYPE_D3D11_TEST_SRC_PATTERN (gst_d3d11_test_src_pattern_get_type ())
static GType
gst_d3d11_test_src_pattern_get_type (void)
{
  static GType pattern_type = 0;

  GST_D3D11_CALL_ONCE_BEGIN {
    static const GEnumValue pattern_types[] = {
      {GST_D3D11_TEST_SRC_SMPTE, "SMPTE 100% color bars", "smpte"},
      {GST_D3D11_TEST_SRC_SNOW, "Random (television snow)", "snow"},
      {GST_D3D11_TEST_SRC_BLACK, "100% Black", "black"},
      {GST_D3D11_TEST_SRC_WHITE, "100% White", "white"},
      {GST_D3D11_TEST_SRC_RED, "Red", "red"},
      {GST_D3D11_TEST_SRC_GREEN, "Green", "green"},
      {GST_D3D11_TEST_SRC_BLUE, "Blue", "blue"},
      {GST_D3D11_TEST_SRC_CHECKERS1, "Checkers 1px", "checkers-1"},
      {GST_D3D11_TEST_SRC_CHECKERS2, "Checkers 2px", "checkers-2"},
      {GST_D3D11_TEST_SRC_CHECKERS4, "Checkers 4px", "checkers-4"},
      {GST_D3D11_TEST_SRC_CHECKERS8, "Checkers 8px", "checkers-8"},

      /**
       * GstD3D11TestSrcPattern::circular:
       *
       * Since: 1.24
       */
      {GST_D3D11_TEST_SRC_CIRCULAR, "Circular", "circular"},

      /**
       * GstD3D11TestSrcPattern::blink:
       *
       * Since: 1.24
       */
      {GST_D3D11_TEST_SRC_BLINK, "Blink", "blink"},

      /**
       * GstD3D11TestSrcPattern::ball:
       *
       * Since: 1.24
       */
      {GST_D3D11_TEST_SRC_BALL, "Moving ball", "ball"},
      {0, nullptr, nullptr},
    };

    pattern_type = g_enum_register_static ("GstD3D11TestSrcPattern",
        pattern_types);
  } GST_D3D11_CALL_ONCE_END;

  return pattern_type;
}

typedef struct
{
  union
  {
    struct
    {
      FLOAT r;
      FLOAT g;
      FLOAT b;
      FLOAT a;
    };
    FLOAT color[4];
  };
} ColorValue;

static const ColorValue color_table[] = {
  /* white */
  {1.0f, 1.0f, 1.0f, 1.0f},
  /* yellow */
  {1.0f, 1.0f, 0.0f, 1.0f},
  /* cyan */
  {0.0f, 1.0f, 1.0f, 1.0f},
  /* green */
  {0.0f, 1.0f, 0.0f, 1.0f},
  /* magenta */
  {1.0f, 0.0f, 1.0f, 1.0f},
  /* red */
  {1.0f, 0.0f, 0.0f, 1.0f},
  /* blue */
  {0.0f, 0.0f, 1.0f, 1.0f},
  /* black */
  {0.0f, 0.0f, 0.0f, 1.0f},
  /* -I */
  {0.0, 0.0f, 0.5f, 1.0f},
  /* +Q */
  {0.0f, 0.5, 1.0f, 1.0f},
  /* superblack */
  {0.0f, 0.0f, 0.0f, 1.0f},
  /* 7.421875% grey */
  {19. / 256.0f, 19. / 256.0f, 19. / 256.0, 1.0f},
};

enum
{
  COLOR_WHITE = 0,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_GREEN,
  COLOR_MAGENTA,
  COLOR_RED,
  COLOR_BLUE,
  COLOR_BLACK,
  COLOR_NEG_I,
  COLOR_POS_Q,
  COLOR_SUPER_BLACK,
  COLOR_DARK_GREY,
};

struct SnowConstBuffer
{
  FLOAT time = 0.0f;
  FLOAT alpha = 1.0f;
  FLOAT padding[2];
};

struct CheckerConstBuffer
{
  FLOAT width;
  FLOAT height;
  FLOAT checker_size;
  FLOAT alpha = 1.0f;
};

struct GstD3D11TestSrcQuad
{
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11Buffer > const_buffer;
  guint vertex_stride = 0;
  guint index_count = 0;
  gboolean is_checker = FALSE;
  gboolean is_snow = FALSE;
  CheckerConstBuffer checker_const_buffer;
  SnowConstBuffer snow_const_buffer;
};

struct StaticColor
{
  ColorValue value;
  gboolean is_valid = FALSE;
};

/* *INDENT-OFF* */
struct GstD3D11TestSrcRender
{
  StaticColor static_color[2];
  std::vector < std::shared_ptr < GstD3D11TestSrcQuad >> quad;
  ComPtr <ID3D11RasterizerState> rs;
  GstD3D11TestSrcPattern pattern;
};
/* *INDENT-ON* */

struct _GstD3D11TestSrc
{
  GstBaseSrc src;

  GstD3D11Device *device;
  gboolean downstream_supports_d3d11;

  GstVideoInfo info;
  GstD3D11Converter *converter;
  GstBufferPool *render_pool;
  GstBufferPool *convert_pool;

  guint adapter_index;
  GstD3D11TestSrcPattern pattern;
  GstD3D11TestSrcRender *render;
  D3D11_VIEWPORT viewport;
  ID2D1Factory *d2d_factory;
  gint64 token;
  gfloat alpha;
  GstD3D11AlphaMode alpha_mode;

  gboolean reverse;
  gint64 n_frames;
  gint64 accum_frames;
  GstClockTime accum_rtime;
  GstClockTime running_time;
};

struct UvVertexData
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

struct ColorVertexData
{
  struct
  {
    FLOAT x;
    FLOAT y;
    FLOAT z;
  } position;
  struct
  {
    FLOAT r;
    FLOAT g;
    FLOAT b;
    FLOAT a;
  } color;
};

static gboolean
setup_snow_render (GstD3D11TestSrc * self, GstD3D11TestSrcRender * render,
    guint on_smpte)
{
  HRESULT hr;
  D3D11_BUFFER_DESC buffer_desc;
  D3D11_SUBRESOURCE_DATA subresource;
  UvVertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
  ID3D11Device *device_handle =
      gst_d3d11_device_get_device_handle (self->device);
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11Buffer > const_buffer;

  memset (&buffer_desc, 0, sizeof (buffer_desc));
  memset (&subresource, 0, sizeof (subresource));

  hr = gst_d3d11_get_vertex_shader_coord (self->device, &vs, &layout);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile vertext shader");
    return FALSE;
  }

  hr = gst_d3d11_get_pixel_shader_snow (self->device, &ps);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile pixel shader");
    return FALSE;
  }

  if (on_smpte) {
    FLOAT left, right, top, bottom;
    FLOAT left_u, right_u, top_v, bottom_v;

    left = 0.5f;
    right = 1.0f;
    top = -0.5f;
    bottom = -1.0f;

    left_u = 3.0f / 4.0f;
    right_u = 1.0f;
    top_v = 3.0f / 4.0f;
    bottom_v = 1.0f;

    /* bottom left */
    vertex_data[0].position.x = left;
    vertex_data[0].position.y = bottom;
    vertex_data[0].position.z = 0.0f;
    vertex_data[0].texture.u = left_u;
    vertex_data[0].texture.v = bottom_v;

    /* top left */
    vertex_data[1].position.x = left;
    vertex_data[1].position.y = top;
    vertex_data[1].position.z = 0.0f;
    vertex_data[1].texture.u = left_u;
    vertex_data[1].texture.v = top_v;

    /* top right */
    vertex_data[2].position.x = right;
    vertex_data[2].position.y = top;
    vertex_data[2].position.z = 0.0f;
    vertex_data[2].texture.u = right_u;
    vertex_data[2].texture.v = top_v;

    /* bottom right */
    vertex_data[3].position.x = right;
    vertex_data[3].position.y = bottom;
    vertex_data[3].position.z = 0.0f;
    vertex_data[3].texture.u = right_u;
    vertex_data[3].texture.v = bottom_v;
  } else {
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
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (UvVertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = vertex_data;
  subresource.SysMemPitch = sizeof (UvVertexData) * 4;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &vertex_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create vertex buffer");
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = indices;
  subresource.SysMemPitch = sizeof (WORD) * 6;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &index_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create index buffer");
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (SnowConstBuffer);
  buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = device_handle->CreateBuffer (&buffer_desc, nullptr, &const_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create constant buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D11TestSrcQuad > ();

  quad->ps = ps;
  quad->vs = vs;
  quad->layout = layout;
  quad->vertex_buffer = vertex_buffer;
  quad->index_buffer = index_buffer;
  quad->const_buffer = const_buffer;
  quad->vertex_stride = sizeof (UvVertexData);
  quad->index_count = 6;
  quad->is_snow = TRUE;
  quad->snow_const_buffer.time = 0;
  quad->snow_const_buffer.alpha = self->alpha;

  render->quad.push_back (quad);

  return TRUE;
}

static gboolean
setup_smpte_render (GstD3D11TestSrc * self, GstD3D11TestSrcRender * render)
{
  HRESULT hr;
  D3D11_BUFFER_DESC buffer_desc;
  D3D11_SUBRESOURCE_DATA subresource;
  ColorVertexData vertex_data[4 * 20];
  WORD indices[6 * 20];
  ID3D11Device *device_handle =
      gst_d3d11_device_get_device_handle (self->device);
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  guint num_vertex = 0;
  guint num_index = 0;

  memset (&buffer_desc, 0, sizeof (buffer_desc));
  memset (&subresource, 0, sizeof (subresource));

  hr = gst_d3d11_get_vertex_shader_color (self->device, &vs, &layout);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile vertext shader");
    return FALSE;
  }

  hr = gst_d3d11_get_pixel_shader_color (self->device, &ps);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile pixel shader");
    return FALSE;
  }

  /* top row */
  for (guint i = 0; i < 7; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 2.0f / 7.0f;
    guint base = i * 4;
    guint idx_base = i * 6;
    const ColorValue *color = &color_table[i];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = 1.0f;
    bottom = -1.0f / 3.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = self->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = self->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = self->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = self->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 7;
  num_index += 6 * 7;

  /* middle row */
  for (guint i = 0; i < 7; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 2.0f / 7.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if ((i % 2) != 0)
      color = &color_table[COLOR_BLACK];
    else
      color = &color_table[COLOR_BLUE - i];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = -1.0f / 3.0f;
    bottom = -0.5f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = self->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = self->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = self->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = self->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 7;
  num_index += 6 * 7;

  /* bottom row, left three */
  for (guint i = 0; i < 3; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 1.0f / 3.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if (i == 0)
      color = &color_table[COLOR_NEG_I];
    else if (i == 1)
      color = &color_table[COLOR_WHITE];
    else
      color = &color_table[COLOR_POS_Q];

    left = -1.0f + i * scale;
    right = -1.0f + (i + 1) * scale;
    top = -0.5f;
    bottom = -1.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = self->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = self->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = self->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = self->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }
  num_vertex += 4 * 3;
  num_index += 6 * 3;

  /* bottom row, middle three */
  for (guint i = 0; i < 3; i++) {
    FLOAT left, right, top, bottom;
    FLOAT scale = 1.0f / 6.0f;
    guint base = i * 4 + num_vertex;
    guint idx_base = i * 6 + num_index;
    const ColorValue *color;

    if (i == 0)
      color = &color_table[COLOR_SUPER_BLACK];
    else if (i == 1)
      color = &color_table[COLOR_BLACK];
    else
      color = &color_table[COLOR_DARK_GREY];

    left = i * scale;
    right = (i + 1) * scale;
    top = -0.5f;
    bottom = -1.0f;

    /* bottom left */
    vertex_data[base].position.x = left;
    vertex_data[base].position.y = bottom;
    vertex_data[base].position.z = 0.0f;
    vertex_data[base].color.r = color->r;
    vertex_data[base].color.g = color->g;
    vertex_data[base].color.b = color->b;
    vertex_data[base].color.a = self->alpha;

    /* top left */
    vertex_data[base + 1].position.x = left;
    vertex_data[base + 1].position.y = top;
    vertex_data[base + 1].position.z = 0.0f;
    vertex_data[base + 1].color = vertex_data[base].color;
    vertex_data[base + 1].color.a = self->alpha;

    /* top right */
    vertex_data[base + 2].position.x = right;
    vertex_data[base + 2].position.y = top;
    vertex_data[base + 2].position.z = 0.0f;
    vertex_data[base + 2].color = vertex_data[base].color;
    vertex_data[base + 2].color.a = self->alpha;

    /* bottom right */
    vertex_data[base + 3].position.x = right;
    vertex_data[base + 3].position.y = bottom;
    vertex_data[base + 3].position.z = 0.0f;
    vertex_data[base + 3].color = vertex_data[base].color;
    vertex_data[base + 3].color.a = self->alpha;

    /* clockwise indexing */
    indices[idx_base] = base;   /* bottom left */
    indices[idx_base + 1] = base + 1;   /* top left */
    indices[idx_base + 2] = base + 2;   /* top right */

    indices[idx_base + 3] = base + 3;   /* bottom right */
    indices[idx_base + 4] = base;       /* bottom left  */
    indices[idx_base + 5] = base + 2;   /* top right */
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (ColorVertexData) * 4 * 20;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = vertex_data;
  subresource.SysMemPitch = sizeof (ColorVertexData) * 4 * 20;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &vertex_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create vertex buffer");
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6 * 20;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = indices;
  subresource.SysMemPitch = sizeof (WORD) * 6 * 20;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &index_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create index buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D11TestSrcQuad > ();

  quad->ps = ps;
  quad->vs = vs;
  quad->layout = layout;
  quad->vertex_buffer = vertex_buffer;
  quad->index_buffer = index_buffer;
  quad->vertex_stride = sizeof (ColorVertexData);
  quad->index_count = 6 * 20;

  render->quad.push_back (quad);

  return setup_snow_render (self, render, TRUE);
}

static gboolean
setup_checker_render (GstD3D11TestSrc * self, GstD3D11TestSrcRender * render,
    guint checker_size)
{
  HRESULT hr;
  D3D11_BUFFER_DESC buffer_desc;
  D3D11_SUBRESOURCE_DATA subresource;
  UvVertexData vertex_data[4];
  const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
  ID3D11Device *device_handle =
      gst_d3d11_device_get_device_handle (self->device);
  ComPtr < ID3D11PixelShader > ps;
  ComPtr < ID3D11VertexShader > vs;
  ComPtr < ID3D11InputLayout > layout;
  ComPtr < ID3D11Buffer > vertex_buffer;
  ComPtr < ID3D11Buffer > index_buffer;
  ComPtr < ID3D11Buffer > const_buffer;

  memset (&buffer_desc, 0, sizeof (buffer_desc));
  memset (&subresource, 0, sizeof (subresource));

  hr = gst_d3d11_get_vertex_shader_coord (self->device, &vs, &layout);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile vertext shader");
    return FALSE;
  }

  hr = gst_d3d11_get_pixel_shader_checker (self->device, &ps);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to compile pixel shader");
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
  buffer_desc.ByteWidth = sizeof (UvVertexData) * 4;
  buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = vertex_data;
  subresource.SysMemPitch = sizeof (UvVertexData) * 4;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &vertex_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create vertex buffer");
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (WORD) * 6;
  buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  subresource.pSysMem = indices;
  subresource.SysMemPitch = sizeof (WORD) * 6;

  hr = device_handle->CreateBuffer (&buffer_desc, &subresource, &index_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create index buffer");
    return FALSE;
  }

  buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  buffer_desc.ByteWidth = sizeof (CheckerConstBuffer);
  buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = device_handle->CreateBuffer (&buffer_desc, nullptr, &const_buffer);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Failed to create constant buffer");
    return FALSE;
  }

  auto quad = std::make_shared < GstD3D11TestSrcQuad > ();

  quad->ps = ps;
  quad->vs = vs;
  quad->layout = layout;
  quad->vertex_buffer = vertex_buffer;
  quad->index_buffer = index_buffer;
  quad->const_buffer = const_buffer;
  quad->vertex_stride = sizeof (UvVertexData);
  quad->index_count = 6;
  quad->is_checker = TRUE;
  quad->checker_const_buffer.width = self->info.width;
  quad->checker_const_buffer.height = self->info.height;
  quad->checker_const_buffer.checker_size = checker_size;
  quad->checker_const_buffer.alpha = self->alpha;

  render->quad.push_back (quad);

  return TRUE;
}

static gboolean
setup_d2d_render (GstD3D11TestSrc * self, GstD3D11TestSrcRender * render)
{
  HRESULT hr;
  ComPtr < ID2D1Factory > d2d_factory;

  if (self->d2d_factory)
    return TRUE;

  hr = D2D1CreateFactory (D2D1_FACTORY_TYPE_MULTI_THREADED,
      IID_PPV_ARGS (&d2d_factory));

  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create D2D factory");
    return FALSE;
  }

  self->d2d_factory = d2d_factory.Detach ();

  return TRUE;
}

enum
{
  PROP_0,
  PROP_ADAPTER,
  PROP_IS_LIVE,
  PROP_PATTERN,
  PROP_ALPHA,
  PROP_ALPHA_MODE,
};

#define DEFAULT_ADAPTER -1
#define DEFAULT_PATTERN GST_D3D11_TEST_SRC_SMPTE
#define DEFAULT_ALPHA 1.0f
#define DEFAULT_ALPHA_MODE GST_D3D11_ALPHA_MODE_UNSPECIFIED

static void gst_d3d11_test_src_dispose (GObject * object);
static void gst_d3d11_test_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_d3d11_test_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void gst_d3d11_test_src_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_d3d11_test_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_d3d11_test_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static GstCaps *gst_d3d11_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_d3d11_test_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_d3d11_test_src_decide_allocation (GstBaseSrc *
    bsrc, GstQuery * query);
static gboolean gst_d3d11_test_src_start (GstBaseSrc * bsrc);
static gboolean gst_d3d11_test_src_stop (GstBaseSrc * bsrc);
static gboolean gst_d3d11_test_src_src_query (GstBaseSrc * bsrc,
    GstQuery * query);
static void gst_d3d11_test_src_get_times (GstBaseSrc * bsrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static GstFlowReturn gst_d3d11_test_src_create (GstBaseSrc * bsrc,
    guint64 offset, guint size, GstBuffer ** buf);

#define gst_d3d11_test_src_parent_class parent_class
G_DEFINE_TYPE (GstD3D11TestSrc, gst_d3d11_test_src, GST_TYPE_BASE_SRC);

static void
gst_d3d11_test_src_class_init (GstD3D11TestSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstCaps *caps;

  gobject_class->dispose = gst_d3d11_test_src_dispose;
  gobject_class->set_property = gst_d3d11_test_src_set_property;
  gobject_class->get_property = gst_d3d11_test_src_get_property;

  g_object_class_install_property (gobject_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "DXGI Adapter index (-1 for any device)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_PATTERN,
      g_param_spec_enum ("pattern", "Pattern",
          "Type of test pattern to generate", GST_TYPE_D3D11_TEST_SRC_PATTERN,
          DEFAULT_PATTERN,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11TestSrc:alpha:
   *
   * Global alpha value to apply
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ALPHA,
      g_param_spec_float ("alpha", "Alpha", "Global alpha value to use",
          0, 1, DEFAULT_ALPHA,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  /**
   * GstD3D11TestSrc:alpha-mode:
   *
   * Alpha mode to use
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ALPHA_MODE,
      g_param_spec_enum ("alpha-mode", "Alpha Mode",
          "alpha mode to use", GST_TYPE_D3D11_ALPHA_MODE, DEFAULT_ALPHA_MODE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_test_src_set_context);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Test Source", "Source/Video",
      "Creates a test video stream", "Seungha Yang <seungha@centricular.com>");

  caps = gst_d3d11_get_updated_template_caps (&template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  basesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_d3d11_test_src_is_seekable);
  basesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_do_seek);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_fixate);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_set_caps);
  basesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_test_src_decide_allocation);
  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_stop);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_src_query);
  basesrc_class->get_times = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_get_times);
  basesrc_class->create = GST_DEBUG_FUNCPTR (gst_d3d11_test_src_create);

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_test_src_debug, "d3d11testsrc", 0,
      "d3d11testsrc");

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_TEST_SRC_PATTERN,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_ALPHA_MODE,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d11_test_src_init (GstD3D11TestSrc * self)
{
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);

  self->adapter_index = DEFAULT_ADAPTER;
  self->pattern = DEFAULT_PATTERN;
  self->alpha = DEFAULT_ALPHA;
  self->alpha_mode = DEFAULT_ALPHA_MODE;
}

static void
gst_d3d11_test_src_dispose (GObject * object)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (object);

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      self->adapter_index = g_value_get_int (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (self), g_value_get_boolean (value));
      break;
    case PROP_PATTERN:
      self->pattern = (GstD3D11TestSrcPattern) g_value_get_enum (value);
      break;
    case PROP_ALPHA:
      self->alpha = g_value_get_float (value);
      break;
    case PROP_ALPHA_MODE:
      self->alpha_mode = (GstD3D11AlphaMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, self->adapter_index);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (self)));
      break;
    case PROP_PATTERN:
      g_value_set_enum (value, self->pattern);
      break;
    case PROP_ALPHA:
      g_value_set_float (value, self->alpha);
      break;
    case PROP_ALPHA_MODE:
      g_value_set_enum (value, self->alpha_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_test_src_set_context (GstElement * element, GstContext * context)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (element);

  gst_d3d11_handle_set_context (element,
      context, self->adapter_index, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d11_test_src_is_seekable (GstBaseSrc * bsrc)
{
  return TRUE;
}

static gboolean
gst_d3d11_test_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);
  GstClockTime position;

  segment->time = segment->start;
  position = segment->position;
  self->reverse = segment->rate < 0;

  /* now move to the position indicated */
  if (self->info.fps_n) {
    self->n_frames = gst_util_uint64_scale (position,
        self->info.fps_n, self->info.fps_d * GST_SECOND);
  } else {
    self->n_frames = 0;
  }
  self->accum_frames = 0;
  self->accum_rtime = 0;
  if (self->info.fps_n) {
    self->running_time = gst_util_uint64_scale (self->n_frames,
        self->info.fps_d * GST_SECOND, self->info.fps_n);
  } else {
    /* FIXME : Not sure what to set here */
    self->running_time = 0;
  }

  return TRUE;
}

static GstCaps *
gst_d3d11_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *s;

  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (s, "width", 320);
  gst_structure_fixate_field_nearest_int (s, "height", 240);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", 30, 1);

  return GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);
}

static void
gst_d3d11_test_src_clear_resource (GstD3D11TestSrc * self)
{
  if (self->render_pool) {
    gst_buffer_pool_set_active (self->render_pool, FALSE);
    gst_clear_object (&self->render_pool);
  }

  if (self->convert_pool) {
    gst_buffer_pool_set_active (self->convert_pool, FALSE);
    gst_clear_object (&self->convert_pool);
  }

  if (self->render) {
    delete self->render;
    self->render = nullptr;
  }

  gst_clear_object (&self->converter);
}

static gboolean
gst_d3d11_test_src_setup_resource (GstD3D11TestSrc * self, GstCaps * caps)
{
  GstVideoInfo draw_info;
  GstCaps *draw_caps;
  GstD3D11AllocationParams *params;
  GstD3D11TestSrcRender *render;
  GstStructure *config;
  HRESULT hr;
  GstD3D11Format device_format;
  guint bind_flags = 0;

  config = gst_structure_new ("converter-config",
      GST_D3D11_CONVERTER_OPT_BACKEND, GST_TYPE_D3D11_CONVERTER_BACKEND,
      GST_D3D11_CONVERTER_BACKEND_SHADER,
      GST_D3D11_CONVERTER_OPT_DEST_ALPHA_MODE,
      GST_TYPE_D3D11_CONVERTER_ALPHA_MODE, self->alpha_mode, nullptr);

  /* D2D uses premultiplied alpha */
  if (self->pattern == GST_D3D11_TEST_SRC_CIRCULAR ||
      self->pattern == GST_D3D11_TEST_SRC_BALL) {
    gst_structure_set (config, GST_D3D11_CONVERTER_OPT_SRC_ALPHA_MODE,
        GST_TYPE_D3D11_CONVERTER_ALPHA_MODE,
        GST_D3D11_CONVERTER_ALPHA_MODE_PREMULTIPLIED, nullptr);
  }

  gst_video_info_set_format (&draw_info, GST_VIDEO_FORMAT_BGRA,
      self->info.width, self->info.height);
  self->converter = gst_d3d11_converter_new (self->device,
      &draw_info, &self->info, config);

  if (!self->converter) {
    GST_ERROR_OBJECT (self, "Failed to create converter");
    goto error;
  }

  draw_caps = gst_video_info_to_caps (&draw_info);
  params = gst_d3d11_allocation_params_new (self->device, &draw_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);

  self->render_pool = gst_d3d11_buffer_pool_new_with_options (self->device,
      draw_caps, params, 0, 0);
  gst_d3d11_allocation_params_free (params);
  gst_caps_unref (draw_caps);

  if (!self->render_pool
      || !gst_buffer_pool_set_active (self->render_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to configure draw pool");
    goto error;
  }

  gst_d3d11_device_get_format (self->device,
      GST_VIDEO_INFO_FORMAT (&self->info), &device_format);

  if ((device_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  } else if ((device_format.format_support[0] &
          D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0) {
    bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
  }

  params = gst_d3d11_allocation_params_new (self->device, &self->info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  self->convert_pool = gst_d3d11_buffer_pool_new_with_options (self->device,
      caps, params, 0, 0);
  gst_d3d11_allocation_params_free (params);

  if (!self->convert_pool ||
      !gst_buffer_pool_set_active (self->convert_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to configure draw pool");
    goto error;
  }

  self->viewport.TopLeftX = 0;
  self->viewport.TopLeftY = 0;
  self->viewport.Width = self->info.width;
  self->viewport.Height = self->info.height;
  self->viewport.MinDepth = 0.0f;
  self->viewport.MaxDepth = 1.0f;

  self->render = render = new GstD3D11TestSrcRender ();
  render->pattern = self->pattern;

  hr = gst_d3d11_device_get_rasterizer (self->device, &render->rs);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get rasterizer state");
    goto error;
  }

  switch (self->pattern) {
    case GST_D3D11_TEST_SRC_SMPTE:
      if (!setup_smpte_render (self, render))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_SNOW:
      if (!setup_snow_render (self, render, FALSE))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_BLACK:
      render->static_color[0].value = color_table[COLOR_BLACK];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_WHITE:
      render->static_color[0].value = color_table[COLOR_WHITE];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_RED:
      render->static_color[0].value = color_table[COLOR_RED];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_GREEN:
      render->static_color[0].value = color_table[COLOR_GREEN];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_BLUE:
      render->static_color[0].value = color_table[COLOR_BLUE];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_CHECKERS1:
      if (!setup_checker_render (self, render, 1))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_CHECKERS2:
      if (!setup_checker_render (self, render, 2))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_CHECKERS4:
      if (!setup_checker_render (self, render, 4))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_CHECKERS8:
      if (!setup_checker_render (self, render, 8))
        goto error;
      break;
    case GST_D3D11_TEST_SRC_BLINK:
      render->static_color[0].value = color_table[COLOR_BLACK];
      render->static_color[0].value.a = self->alpha;
      render->static_color[0].is_valid = TRUE;
      render->static_color[1].value = color_table[COLOR_WHITE];
      render->static_color[1].value.a = self->alpha;
      render->static_color[1].is_valid = TRUE;
      break;
    case GST_D3D11_TEST_SRC_CIRCULAR:
    case GST_D3D11_TEST_SRC_BALL:
      if (!setup_d2d_render (self, render))
        goto error;
      break;
  }

  return TRUE;

error:
  gst_d3d11_test_src_clear_resource (self);
  return FALSE;
}

static gboolean
gst_d3d11_test_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);
  GstCapsFeatures *features;

  GST_DEBUG_OBJECT (self, "Set caps %" GST_PTR_FORMAT, caps);

  gst_d3d11_test_src_clear_resource (self);

  features = gst_caps_get_features (caps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    self->downstream_supports_d3d11 = TRUE;
  } else {
    self->downstream_supports_d3d11 = FALSE;
  }

  GST_OBJECT_LOCK (self);
  gst_video_info_from_caps (&self->info, caps);
  GST_OBJECT_UNLOCK (self);
  if (self->info.fps_d <= 0 || self->info.fps_n <= 0) {
    GST_ERROR_OBJECT (self, "Invalid framerate %d/%d", self->info.fps_n,
        self->info.fps_d);
    return FALSE;
  }

  gst_base_src_set_blocksize (bsrc, GST_VIDEO_INFO_SIZE (&self->info));

  return gst_d3d11_test_src_setup_resource (self, caps);
}

static gboolean
gst_d3d11_test_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);
  GstBufferPool *pool = nullptr;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;
  GstVideoInfo vinfo;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_ERROR_OBJECT (self, "No output caps");
    return FALSE;
  }

  gst_video_info_from_caps (&vinfo, caps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    size = GST_VIDEO_INFO_SIZE (&vinfo);

    min = max = 0;
    update_pool = FALSE;
  }

  if (pool && self->downstream_supports_d3d11) {
    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != self->device)
        gst_clear_object (&pool);
    }
  }

  if (!pool) {
    if (self->downstream_supports_d3d11)
      pool = gst_d3d11_buffer_pool_new (self->device);
    else
      pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (self->downstream_supports_d3d11) {
    GstD3D11Format device_format;
    guint bind_flags = 0;

    gst_d3d11_device_get_format (self->device, GST_VIDEO_INFO_FORMAT (&vinfo),
        &device_format);

    if ((device_format.format_support[0] &
            D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }

    if ((device_format.format_support[0] &
            D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0) {
      bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if ((device_format.format_support[0] & D3D11_BIND_SHADER_RESOURCE) != 0)
      bind_flags |= D3D11_BIND_SHADER_RESOURCE;

    d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
    if (!d3d11_params) {
      d3d11_params = gst_d3d11_allocation_params_new (self->device, &vinfo,
          GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
    } else {
      gst_d3d11_allocation_params_set_bind_flags (d3d11_params, bind_flags);
    }

    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set config");
    gst_clear_object (&pool);
    return FALSE;
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_d3d11_test_src_start (GstBaseSrc * bsrc)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);

  if (!gst_d3d11_ensure_element_data (GST_ELEMENT (bsrc), self->adapter_index,
          &self->device)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        ("Failed to prepare device"), (nullptr));
    return FALSE;
  }

  self->running_time = 0;
  self->reverse = FALSE;
  self->n_frames = 0;
  self->accum_frames = 0;
  self->accum_rtime = 0;
  self->token = gst_d3d11_create_user_token ();

  gst_video_info_init (&self->info);

  return TRUE;
}

static gboolean
gst_d3d11_test_src_stop (GstBaseSrc * bsrc)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);

  gst_d3d11_test_src_clear_resource (self);
  gst_clear_object (&self->device);
  GST_D3D11_CLEAR_COM (self->d2d_factory);

  return TRUE;
}

static gboolean
gst_d3d11_test_src_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_d3d11_handle_context_query (GST_ELEMENT_CAST (self), query,
              self->device)) {
        return TRUE;
      }
      break;
    case GST_QUERY_LATENCY:
      GST_OBJECT_LOCK (self);
      if (self->info.fps_n > 0 && self->info.fps_d > 0) {
        GstClockTime latency;

        latency =
            gst_util_uint64_scale (GST_SECOND, self->info.fps_d,
            self->info.fps_n);
        GST_OBJECT_UNLOCK (self);
        gst_query_set_latency (query,
            gst_base_src_is_live (bsrc), latency, GST_CLOCK_TIME_NONE);
        GST_DEBUG_OBJECT (self, "Reporting latency of %" GST_TIME_FORMAT,
            GST_TIME_ARGS (latency));
        return TRUE;
      }
      GST_OBJECT_UNLOCK (self);
      break;
    case GST_QUERY_DURATION:
      if (bsrc->num_buffers > 0) {
        GstFormat format;

        gst_query_parse_duration (query, &format, nullptr);
        if (format != GST_FORMAT_TIME)
          return FALSE;

        GST_OBJECT_LOCK (self);
        if (format == GST_FORMAT_TIME && self->info.fps_n > 0 &&
            self->info.fps_d > 0) {
          gint64 dur;
          dur = gst_util_uint64_scale_int_round (bsrc->num_buffers
              * GST_SECOND, self->info.fps_d, self->info.fps_n);
          gst_query_set_duration (query, GST_FORMAT_TIME, dur);
          GST_OBJECT_UNLOCK (self);
          return TRUE;
        }

        GST_OBJECT_UNLOCK (self);
      }
      break;
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
}

static void
gst_d3d11_test_src_get_times (GstBaseSrc * bsrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (bsrc)) {
    GstClockTime timestamp = GST_BUFFER_PTS (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

struct GstD3D11TestSrcD2DData
{
  ID2D1RenderTarget *target;
  ID2D1Brush *brush;
  ID2D1Factory *factory;
};

static void
gst_d3d11_test_src_d2d_data_free (GstD3D11TestSrcD2DData * data)
{
  GST_D3D11_CLEAR_COM (data->brush);
  GST_D3D11_CLEAR_COM (data->target);
  GST_D3D11_CLEAR_COM (data->factory);

  g_free (data);
}

static gboolean
gst_d3d11_test_src_draw_ball (GstD3D11TestSrc * self,
    ID3D11DeviceContext * context, GstD3D11Memory * mem)
{
  GstD3D11TestSrcD2DData *data;
  HRESULT hr;
  gdouble rad;
  FLOAT x, y;
  ID2D1RadialGradientBrush *ball_brush;

  data = (GstD3D11TestSrcD2DData *)
      gst_d3d11_memory_get_token_data (mem, self->token);

  if (!data) {
    ComPtr < IDXGISurface > surface;
    ComPtr < ID2D1RenderTarget > d2d_target;
    ComPtr < ID2D1GradientStopCollection > collection;
    ComPtr < ID2D1RadialGradientBrush > brush;
    ID3D11Texture2D *texture;
    D2D1_RENDER_TARGET_PROPERTIES props;
    D2D1_GRADIENT_STOP stops[3];

    props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = 0;
    props.dpiY = 0;
    props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    texture = (ID3D11Texture2D *) gst_d3d11_memory_get_resource_handle (mem);

    hr = texture->QueryInterface (IID_PPV_ARGS (&surface));
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't get DXGI surface");
      return FALSE;
    }

    hr = self->d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (),
        props, &d2d_target);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't get D2D render target");
      return FALSE;
    }

    stops[0].color = D2D1::ColorF (D2D1::ColorF::White, self->alpha);
    stops[0].position = 0.0f;
    stops[1].color = D2D1::ColorF (D2D1::ColorF::Snow, self->alpha);
    stops[1].position = 0.3f;
    stops[2].color = D2D1::ColorF (D2D1::ColorF::Black, self->alpha);
    stops[2].position = 1.0f;

    hr = d2d_target->CreateGradientStopCollection (stops, 3, D2D1_GAMMA_1_0,
        D2D1_EXTEND_MODE_CLAMP, &collection);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create gradient stop collection");
      return FALSE;
    }

    hr = d2d_target->CreateRadialGradientBrush (D2D1::
        RadialGradientBrushProperties (D2D1::Point2F (0, 0), D2D1::Point2F (0,
                0), 20, 20), collection.Get (), &brush);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create brush");
      return FALSE;
    }

    data = g_new0 (GstD3D11TestSrcD2DData, 1);
    data->target = d2d_target.Detach ();
    data->brush = brush.Detach ();
    data->factory = self->d2d_factory;
    self->d2d_factory->AddRef ();

    gst_d3d11_memory_set_token_data (mem, self->token, data,
        (GDestroyNotify) gst_d3d11_test_src_d2d_data_free);
  }

  rad = (gdouble) self->n_frames / 200;
  rad = 2 * G_PI * rad;
  x = 20 + (0.5 + 0.5 * sin (rad)) * (self->info.width - 40);
  y = 20 + (0.5 + 0.5 * sin (rad * sqrt (2))) * (self->info.height - 40);

  ball_brush = (ID2D1RadialGradientBrush *) data->brush;
  ball_brush->SetCenter (D2D1::Point2F (x, y));

  data->target->BeginDraw ();
  data->target->Clear (D2D1::ColorF (D2D1::ColorF::Black));
  data->target->FillEllipse (D2D1::Ellipse (D2D1::Point2F (x, y), 20, 20),
      data->brush);
  data->target->EndDraw ();

  return TRUE;
}

static gboolean
gst_d3d11_test_src_draw_circular (GstD3D11TestSrc * self,
    ID3D11DeviceContext * context, GstD3D11Memory * mem)
{
  GstD3D11TestSrcD2DData *data;
  HRESULT hr;
  FLOAT x, y;
  FLOAT rad;

  rad = ((FLOAT) MAX (self->info.width, self->info.height)) / 2;
  x = (FLOAT) self->info.width / 2;
  y = (FLOAT) self->info.height / 2;

  data = (GstD3D11TestSrcD2DData *)
      gst_d3d11_memory_get_token_data (mem, self->token);

  if (!data) {
    ComPtr < IDXGISurface > surface;
    ComPtr < ID2D1RenderTarget > d2d_target;
    ComPtr < ID2D1GradientStopCollection > collection;
    ComPtr < ID2D1RadialGradientBrush > brush;
    ID3D11Texture2D *texture;
    D2D1_RENDER_TARGET_PROPERTIES props;
    D2D1_GRADIENT_STOP stops[129];
    FLOAT position = 1.0f;

    props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = 0;
    props.dpiY = 0;
    props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    texture = (ID3D11Texture2D *) gst_d3d11_memory_get_resource_handle (mem);

    hr = texture->QueryInterface (IID_PPV_ARGS (&surface));
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't get DXGI surface");
      return FALSE;
    }

    hr = self->d2d_factory->CreateDxgiSurfaceRenderTarget (surface.Get (),
        props, &d2d_target);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't get D2D render target");
      return FALSE;
    }

    for (guint i = 0; i < G_N_ELEMENTS (stops); i++) {
      FLOAT diff;
      if ((i % 2) == 0)
        stops[i].color = D2D1::ColorF (D2D1::ColorF::Black, self->alpha);
      else
        stops[i].color = D2D1::ColorF (D2D1::ColorF::White, self->alpha);

      stops[i].position = position;
      diff = position / G_N_ELEMENTS (stops) * 2;
      position -= diff;
    }

    hr = d2d_target->CreateGradientStopCollection (stops, G_N_ELEMENTS (stops),
        &collection);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create gradient stop collection");
      return FALSE;
    }

    hr = d2d_target->CreateRadialGradientBrush (D2D1::
        RadialGradientBrushProperties (D2D1::Point2F (x, y), D2D1::Point2F (0,
                0), rad, rad), collection.Get (), &brush);
    if (!gst_d3d11_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create brush");
      return FALSE;
    }

    data = g_new0 (GstD3D11TestSrcD2DData, 1);
    data->target = d2d_target.Detach ();
    data->brush = brush.Detach ();
    data->factory = self->d2d_factory;
    self->d2d_factory->AddRef ();

    gst_d3d11_memory_set_token_data (mem, self->token, data,
        (GDestroyNotify) gst_d3d11_test_src_d2d_data_free);
  }

  data->target->BeginDraw ();
  data->target->Clear (D2D1::ColorF (D2D1::ColorF::Black));
  data->target->FillEllipse (D2D1::Ellipse (D2D1::Point2F (x, y), rad, rad),
      data->brush);
  data->target->EndDraw ();

  return TRUE;
}

static gboolean
gst_d3d11_test_src_draw_pattern (GstD3D11TestSrc * self,
    ID3D11DeviceContext * context, GstD3D11Memory * mem,
    ID3D11RenderTargetView * rtv, GstClockTime pts)
{
  GstD3D11TestSrcRender *render = self->render;
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE map;
  UINT offsets = 0;

  if (render->static_color[0].is_valid) {
    if (render->static_color[1].is_valid && (self->n_frames % 2) == 1)
      context->ClearRenderTargetView (rtv, render->static_color[1].value.color);
    else
      context->ClearRenderTargetView (rtv, render->static_color[0].value.color);
    return TRUE;
  }

  if (render->pattern == GST_D3D11_TEST_SRC_BALL)
    return gst_d3d11_test_src_draw_ball (self, context, mem);
  else if (render->pattern == GST_D3D11_TEST_SRC_CIRCULAR)
    return gst_d3d11_test_src_draw_circular (self, context, mem);

  context->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->RSSetViewports (1, &self->viewport);
  context->RSSetState (render->rs.Get ());
  context->OMSetRenderTargets (1, &rtv, nullptr);
  context->OMSetBlendState (nullptr, nullptr, 0xffffffff);

  /* *INDENT-OFF* */
  for (auto quad : render->quad) {
    if (quad->const_buffer) {
      hr = context->Map (quad->const_buffer.Get (),
          0, D3D11_MAP_WRITE_DISCARD, 0, &map);
      if (!gst_d3d11_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Failed to map constant buffer");
        return FALSE;
      }

      if (quad->is_snow) {
        SnowConstBuffer *const_buf = (SnowConstBuffer *) map.pData;
        const_buf->time = (FLOAT) pts / GST_SECOND;
        const_buf->alpha = self->alpha;
      } else if (quad->is_checker) {
        CheckerConstBuffer *const_buf = (CheckerConstBuffer *) map.pData;
        quad->checker_const_buffer.alpha = self->alpha;
        memcpy (const_buf, &quad->checker_const_buffer,
            sizeof (CheckerConstBuffer));
      }

      context->Unmap (quad->const_buffer.Get (), 0);

      ID3D11Buffer *const_bufs[] = { quad->const_buffer.Get () };
      context->PSSetConstantBuffers (0, 1, const_bufs);
    } else {
      context->PSSetConstantBuffers (0, 0, nullptr);
    }

    context->IASetInputLayout (quad->layout.Get ());
    ID3D11Buffer *vertex_buf[] = { quad->vertex_buffer.Get () };
    context->IASetVertexBuffers (0, 1, vertex_buf,
        &quad->vertex_stride, &offsets);
    context->IASetIndexBuffer (quad->index_buffer.Get (), DXGI_FORMAT_R16_UINT,
        0);

    context->VSSetShader (quad->vs.Get (), nullptr, 0);
    context->PSSetShader (quad->ps.Get (), nullptr, 0);

    context->DrawIndexed (quad->index_count, 0, 0);
  }
  /* *INDENT-ON* */

  context->OMSetRenderTargets (0, nullptr, nullptr);

  return TRUE;
}

static GstFlowReturn
gst_d3d11_test_src_create (GstBaseSrc * bsrc, guint64 offset,
    guint size, GstBuffer ** buf)
{
  GstD3D11TestSrc *self = GST_D3D11_TEST_SRC (bsrc);
  GstBuffer *buffer = nullptr;
  GstBuffer *render_buffer = nullptr;
  GstBuffer *convert_buffer = nullptr;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstClockTime pts;
  GstClockTime next_time;
  GstMapInfo render_info;
  ID3D11DeviceContext *context_handle =
      gst_d3d11_device_get_device_context_handle (self->device);
  GstMemory *mem;
  GstD3D11Memory *dmem;
  ID3D11RenderTargetView *pattern_rtv;
  gboolean convert_ret;
  GstD3D11DeviceLockGuard lk (self->device);

  ret = GST_BASE_SRC_CLASS (parent_class)->alloc (bsrc, offset, size, &buffer);
  if (ret != GST_FLOW_OK)
    return ret;

  ret =
      gst_buffer_pool_acquire_buffer (self->render_pool, &render_buffer,
      nullptr);
  if (ret != GST_FLOW_OK)
    goto error;

  if (self->downstream_supports_d3d11) {
    convert_buffer = buffer;
  } else {
    ret = gst_buffer_pool_acquire_buffer (self->convert_pool,
        &convert_buffer, nullptr);
    if (ret != GST_FLOW_OK)
      goto error;
  }

  mem = gst_buffer_peek_memory (render_buffer, 0);

  if (!gst_memory_map (mem, &render_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map render buffer");
    goto error;
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  pattern_rtv = gst_d3d11_memory_get_render_target_view (dmem, 0);
  if (!pattern_rtv) {
    GST_ERROR_OBJECT (self, "RTV is not available");
    gst_memory_unmap (mem, &render_info);
    goto error;
  }

  pts = self->accum_rtime + self->running_time;
  gst_d3d11_test_src_draw_pattern (self, context_handle, dmem, pattern_rtv,
      pts);
  gst_memory_unmap (mem, &render_info);
  convert_ret = gst_d3d11_converter_convert_buffer_unlocked (self->converter,
      render_buffer, convert_buffer);

  if (!convert_ret) {
    GST_ERROR_OBJECT (self, "Failed to convert buffer");
    goto error;
  }

  if (self->downstream_supports_d3d11) {
    convert_buffer = nullptr;
  } else {
    gst_d3d11_buffer_copy_into (buffer, convert_buffer, &self->info);
    gst_clear_buffer (&convert_buffer);
  }

  gst_clear_buffer (&render_buffer);

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buffer) = self->accum_frames + self->n_frames;
  if (self->reverse) {
    self->n_frames--;
  } else {
    self->n_frames++;
  }
  GST_BUFFER_OFFSET_END (buffer) = GST_BUFFER_OFFSET (buffer) + 1;

  next_time = gst_util_uint64_scale (self->n_frames,
      self->info.fps_d * GST_SECOND, self->info.fps_n);
  if (self->reverse) {
    /* We already decremented to next frame */
    GstClockTime prev_pts = gst_util_uint64_scale (self->n_frames + 2,
        self->info.fps_d * GST_SECOND, self->info.fps_n);

    GST_BUFFER_DURATION (buffer) = prev_pts - GST_BUFFER_PTS (buffer);
  } else {
    GST_BUFFER_DURATION (buffer) = next_time - self->running_time;
  }

  self->running_time = next_time;
  *buf = buffer;

  return GST_FLOW_OK;

error:
  gst_clear_buffer (&buffer);
  gst_clear_buffer (&render_buffer);
  if (!self->downstream_supports_d3d11)
    gst_clear_buffer (&convert_buffer);

  return ret;
}
