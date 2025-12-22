/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdwrite-renderer.h"
#include <wrl.h>
#include <vector>

GST_DEBUG_CATEGORY_EXTERN (gst_dwrite_debug);
#define GST_CAT_DEFAULT gst_dwrite_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;

enum class RenderPath
{
  BACKGROUND,
  TEXT,
};

struct RenderContext
{
  RenderPath render_path;
  ID2D1Factory *factory;
  ID2D1RenderTarget *target;
  RECT client_rect;
  std::vector<DWRITE_LINE_METRICS> line_metrics;
  UINT line_index = 0;
  UINT char_index = 0;
  ComPtr<ID2D1Geometry> bg_rect;
  D2D1_COLOR_F bg_color = D2D1::ColorF (D2D1::ColorF::Black, 0.0);
};
/* *INDENT-ON* */

static HRESULT
CombineTwoGeometries (ID2D1Factory * factory, ID2D1Geometry * a,
    ID2D1Geometry * b, ID2D1Geometry ** result)
{
  HRESULT hr;
  ComPtr < ID2D1GeometrySink > sink;
  ComPtr < ID2D1PathGeometry > geometry;

  hr = factory->CreatePathGeometry (&geometry);
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't create path geometry, 0x%x", (guint) hr);
    return hr;
  }

  hr = geometry->Open (&sink);
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't open path geometry, 0x%x", (guint) hr);
    return hr;
  }

  hr = a->CombineWithGeometry (b,
      D2D1_COMBINE_MODE_UNION, nullptr, sink.Get ());
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't combine geometry, 0x%x", (guint) hr);
    return hr;
  }

  hr = sink->Close ();
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't close sink, 0x%x", (guint) hr);
    return hr;
  }

  *result = geometry.Detach ();

  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::CreateInstance (IDWriteFactory * factory,
    IGstDWriteTextRenderer ** renderer)
{
  IGstDWriteTextRenderer *self = new IGstDWriteTextRenderer ();

  if (!self)
    return E_OUTOFMEMORY;

  self->factory_ = factory;
  factory->AddRef ();
  *renderer = self;

  return S_OK;
}

/* IUnknown */
STDMETHODIMP_ (ULONG)
    IGstDWriteTextRenderer::AddRef (void)
{
  return InterlockedIncrement (&ref_count_);
}

STDMETHODIMP_ (ULONG)
    IGstDWriteTextRenderer::Release (void)
{
  ULONG ref_count;

  ref_count = InterlockedDecrement (&ref_count_);

  if (ref_count == 0)
    delete this;

  return ref_count;
}

STDMETHODIMP
IGstDWriteTextRenderer::QueryInterface (REFIID riid, void **object)
{
  if (riid == __uuidof (IUnknown)) {
    *object = static_cast < IUnknown * >
        (static_cast < IGstDWriteTextRenderer * >(this));
  } else if (riid == __uuidof (IDWritePixelSnapping)) {
    *object = static_cast < IDWritePixelSnapping * >
        (static_cast < IGstDWriteTextRenderer * >(this));
  } else if (riid == IID_IGstDWriteTextRenderer) {
    *object = static_cast < IDWriteTextRenderer * >
        (static_cast < IGstDWriteTextRenderer * >(this));
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  AddRef ();

  return S_OK;
}

/* IDWritePixelSnapping */
STDMETHODIMP
    IGstDWriteTextRenderer::IsPixelSnappingDisabled (void *context,
    BOOL * is_disabled)
{
  *is_disabled = FALSE;
  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::GetCurrentTransform (void *context,
    DWRITE_MATRIX * transform)
{
  RenderContext *render_ctx;

  g_assert (context != nullptr);

  render_ctx = (RenderContext *) context;
  render_ctx->target->GetTransform (reinterpret_cast <
      D2D1_MATRIX_3X2_F * >(transform));

  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::GetPixelsPerDip (void *context,
    FLOAT * pixels_per_dip)
{
  *pixels_per_dip = 1.0f;

  return S_OK;
}

/* IDWriteTextRenderer */
STDMETHODIMP
    IGstDWriteTextRenderer::DrawGlyphRun (void *context, FLOAT origin_x,
    FLOAT origin_y, DWRITE_MEASURING_MODE mode,
    DWRITE_GLYPH_RUN const *glyph_run,
    DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_desc,
    IUnknown * client_effect)
{
  ComPtr < ID2D1PathGeometry > geometry;
  ComPtr < ID2D1GeometrySink > sink;
  ComPtr < ID2D1TransformedGeometry > transformed;
  ComPtr < ID2D1TransformedGeometry > shadow_transformed;
  ComPtr < IGstDWriteTextEffect > effect;
  ComPtr < ID2D1SolidColorBrush > brush;
  ComPtr < ID2D1SolidColorBrush > outline_brush;
  ComPtr < ID2D1SolidColorBrush > shadow_brush;
  RenderContext *render_ctx;
  ID2D1RenderTarget *target;
  ID2D1Factory *factory;
  HRESULT hr;
  RECT client_rect;
  D2D1_COLOR_F fg_color = D2D1::ColorF (D2D1::ColorF::Black);
  BOOL enable_color_font = FALSE;

  g_assert (context != nullptr);

  render_ctx = (RenderContext *) context;
  client_rect = render_ctx->client_rect;
  target = render_ctx->target;
  factory = render_ctx->factory;

  if (client_effect)
    client_effect->QueryInterface (IID_IGstDWriteTextEffect, &effect);

  if (render_ctx->render_path == RenderPath::BACKGROUND) {
    D2D1_COLOR_F color;
    BOOL enabled;
    DWRITE_FONT_METRICS font_metrics;
    FLOAT run_width = 0;
    FLOAT adjust, ascent, descent;
    D2D1_RECT_F bg_rect;
    ComPtr < ID2D1SolidColorBrush > bg_brush;
    DWRITE_LINE_METRICS line_metrics =
        render_ctx->line_metrics.at (render_ctx->line_index);

    if (effect &&
        render_ctx->char_index + line_metrics.trailingWhitespaceLength <
        line_metrics.length) {
      effect->GetBrushColor (GST_DWRITE_BRUSH_BACKGROUND, &color, &enabled);
      if (enabled) {
        ComPtr < ID2D1RectangleGeometry > rect_geometry;
        ComPtr < ID2D1PathGeometry > path_geometry;
        ComPtr < ID2D1GeometrySink > path_sink;

        for (UINT32 i = 0; i < glyph_run->glyphCount; i++)
          run_width += glyph_run->glyphAdvances[i];

        glyph_run->fontFace->GetMetrics (&font_metrics);
        adjust = glyph_run->fontEmSize / font_metrics.designUnitsPerEm;
        ascent = adjust * font_metrics.ascent;
        descent = adjust * font_metrics.descent;

        bg_rect =
            D2D1::RectF (origin_x, origin_y - ascent, origin_x + run_width,
            origin_y + descent);

        hr = factory->CreateRectangleGeometry (bg_rect, &rect_geometry);
        if (FAILED (hr))
          return hr;

        hr = factory->CreatePathGeometry (&path_geometry);
        if (FAILED (hr))
          return hr;

        hr = path_geometry->Open (&path_sink);
        if (FAILED (hr))
          return hr;

        hr = rect_geometry->Outline (nullptr, path_sink.Get ());
        if (FAILED (hr))
          return hr;

        path_sink->Close ();

        if (render_ctx->bg_rect) {
          if (render_ctx->bg_color.r == color.r &&
              render_ctx->bg_color.g == color.g &&
              render_ctx->bg_color.b == color.b &&
              render_ctx->bg_color.a == color.a) {
            ComPtr < ID2D1Geometry > combined;
            hr = CombineTwoGeometries (factory,
                render_ctx->bg_rect.Get (), path_geometry.Get (), &combined);
            if (FAILED (hr))
              return hr;

            render_ctx->bg_rect = combined;
          } else {
            target->CreateSolidColorBrush (render_ctx->bg_color, &bg_brush);
            target->FillGeometry (render_ctx->bg_rect.Get (), bg_brush.Get ());
            render_ctx->bg_rect = nullptr;
          }
        }

        if (!render_ctx->bg_rect) {
          render_ctx->bg_rect = path_geometry;
          render_ctx->bg_color = color;
        }
      }
    }

    render_ctx->char_index += glyph_run_desc->stringLength;
    if (render_ctx->char_index >= line_metrics.length) {
      render_ctx->line_index++;
      render_ctx->char_index = 0;
    }

    return S_OK;
  }

  hr = factory->CreatePathGeometry (&geometry);
  if (FAILED (hr))
    return hr;

  hr = geometry->Open (&sink);
  if (FAILED (hr))
    return hr;

  hr = glyph_run->fontFace->GetGlyphRunOutline (glyph_run->fontEmSize,
      glyph_run->glyphIndices, glyph_run->glyphAdvances,
      glyph_run->glyphOffsets, glyph_run->glyphCount, glyph_run->isSideways,
      glyph_run->bidiLevel % 2, sink.Get ());

  if (FAILED (hr))
    return hr;

  sink->Close ();

  hr = factory->CreateTransformedGeometry (geometry.Get (),
      D2D1::Matrix3x2F::Translation (origin_x, origin_y), &transformed);

  if (FAILED (hr))
    return hr;

  if (effect) {
    D2D1_COLOR_F color;
    BOOL enabled;

    effect->GetBrushColor (GST_DWRITE_BRUSH_FORGROUND, &color, &enabled);
    if (enabled) {
      target->CreateSolidColorBrush (color, &brush);
      fg_color = color;
    }

    effect->GetBrushColor (GST_DWRITE_BRUSH_OUTLINE, &color, &enabled);
    if (enabled)
      target->CreateSolidColorBrush (color, &outline_brush);

    effect->GetBrushColor (GST_DWRITE_BRUSH_SHADOW, &color, &enabled);
    if (enabled)
      target->CreateSolidColorBrush (color, &shadow_brush);

    effect->GetEnableColorFont (&enable_color_font);
  } else {
    target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black), &brush);
    outline_brush = brush;
  }

#ifdef HAVE_DWRITE_COLOR_FONT
  if (enable_color_font) {
    const DWRITE_GLYPH_IMAGE_FORMATS supported_formats =
        (DWRITE_GLYPH_IMAGE_FORMATS) (DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
        DWRITE_GLYPH_IMAGE_FORMATS_CFF |
        DWRITE_GLYPH_IMAGE_FORMATS_COLR |
        DWRITE_GLYPH_IMAGE_FORMATS_SVG |
        DWRITE_GLYPH_IMAGE_FORMATS_PNG |
        DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
        DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
        DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8);

    ComPtr < IDWriteColorGlyphRunEnumerator1 > glyph_run_enum;
    ComPtr < IDWriteFactory4 > factory4;
    ComPtr < ID2D1DeviceContext4 > ctx4;
    hr = factory_->QueryInterface (IID_PPV_ARGS (&factory4));
    if (SUCCEEDED (hr))
      hr = target->QueryInterface (IID_PPV_ARGS (&ctx4));

    if (SUCCEEDED (hr)) {
      hr = factory4->TranslateColorGlyphRun (D2D1::Point2 (origin_x, origin_y),
          glyph_run, glyph_run_desc, supported_formats,
          DWRITE_MEASURING_MODE_NATURAL, nullptr, 0, &glyph_run_enum);
      if (hr != DWRITE_E_NOCOLOR && SUCCEEDED (hr)) {
        ComPtr < ID2D1SolidColorBrush > tmp_brush;
        BOOL has_run = FALSE;
        do {
          DWRITE_COLOR_GLYPH_RUN1 const *color_run;
          hr = glyph_run_enum->MoveNext (&has_run);

          if (FAILED (hr))
            return hr;

          if (!has_run)
            return S_OK;

          hr = glyph_run_enum->GetCurrentRun (&color_run);
          if (FAILED (hr))
            return hr;

          const auto cur_origin = D2D1::Point2F (color_run->baselineOriginX,
              color_run->baselineOriginY);
          switch (color_run->glyphImageFormat) {
            case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
            case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
            case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
            case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
              ctx4->DrawColorBitmapGlyphRun (color_run->glyphImageFormat,
                  cur_origin, &color_run->glyphRun,
                  DWRITE_MEASURING_MODE_NATURAL);
              break;
            case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
            {
              ComPtr < ID2D1SolidColorBrush > svg_brush;
              if (brush)
                svg_brush = brush;
              else if (outline_brush)
                svg_brush = outline_brush;

              ctx4->DrawSvgGlyphRun (cur_origin, &color_run->glyphRun,
                  svg_brush.Get ());
              break;
            }
            case DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE:
            case DWRITE_GLYPH_IMAGE_FORMATS_CFF:
            case DWRITE_GLYPH_IMAGE_FORMATS_COLR:
            default:
              if (!tmp_brush) {
                hr = target->CreateSolidColorBrush (fg_color, &tmp_brush);
                if (FAILED (hr))
                  return hr;
              }

              if (color_run->paletteIndex == 0xffff)
                tmp_brush->SetColor (fg_color);
              else
                tmp_brush->SetColor (color_run->runColor);

              target->DrawGlyphRun (cur_origin, &color_run->glyphRun,
                  tmp_brush.Get ());
              break;
          }

        } while (has_run && SUCCEEDED (hr));

        return S_OK;
      }
    }
  }
#endif /* HAVE_DWRITE_COLOR_FONT */

  if (shadow_brush) {
    FLOAT adjust = glyph_run->fontEmSize * 0.06;
    hr = factory->CreateTransformedGeometry (geometry.Get (),
        D2D1::Matrix3x2F::Translation (origin_x + adjust, origin_y + adjust),
        &shadow_transformed);

    if (FAILED (hr))
      return hr;
  }

  if (shadow_brush)
    target->FillGeometry (shadow_transformed.Get (), shadow_brush.Get ());

  if (outline_brush)
    target->DrawGeometry (transformed.Get (), outline_brush.Get ());

  if (brush)
    target->FillGeometry (transformed.Get (), brush.Get ());

  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::DrawUnderline (void *context, FLOAT origin_x,
    FLOAT origin_y, DWRITE_UNDERLINE const *underline, IUnknown * client_effect)
{
  ComPtr < ID2D1RectangleGeometry > geometry;
  ComPtr < ID2D1TransformedGeometry > transformed;
  ComPtr < IGstDWriteTextEffect > effect;
  ComPtr < ID2D1SolidColorBrush > brush;
  RenderContext *render_ctx;
  ID2D1RenderTarget *target;
  ID2D1Factory *factory;
  HRESULT hr;

  g_assert (context != nullptr);

  render_ctx = (RenderContext *) context;
  if (render_ctx->render_path == RenderPath::BACKGROUND)
    return S_OK;

  target = render_ctx->target;
  factory = render_ctx->factory;

  hr = factory->CreateRectangleGeometry (D2D1::RectF (0, underline->offset,
          underline->width, underline->offset + underline->thickness),
      &geometry);
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't create geometry, 0x%x", (guint) hr);
    return hr;
  }

  hr = factory->CreateTransformedGeometry (geometry.Get (),
      D2D1::Matrix3x2F::Translation (origin_x, origin_y), &transformed);
  if (FAILED (hr)) {
    GST_WARNING ("Couldn't create transformed geometry, 0x%x", (guint) hr);
    return hr;
  }

  if (client_effect)
    client_effect->QueryInterface (IID_IGstDWriteTextEffect, &effect);

  if (effect) {
    D2D1_COLOR_F color;
    BOOL enabled;

    effect->GetBrushColor (GST_DWRITE_BRUSH_UNDERLINE, &color, &enabled);
    if (enabled)
      target->CreateSolidColorBrush (color, &brush);
  } else {
    target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black), &brush);
  }

  if (brush)
    target->FillGeometry (transformed.Get (), brush.Get ());

  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::DrawStrikethrough (void *context, FLOAT origin_x,
    FLOAT origin_y, DWRITE_STRIKETHROUGH const *strikethrough,
    IUnknown * client_effect)
{
  ComPtr < ID2D1RectangleGeometry > geometry;
  ComPtr < ID2D1TransformedGeometry > transformed;
  ComPtr < IGstDWriteTextEffect > effect;
  ComPtr < ID2D1SolidColorBrush > brush;
  RenderContext *render_ctx;
  ID2D1RenderTarget *target;
  ID2D1Factory *factory;
  HRESULT hr;

  g_assert (context != nullptr);

  render_ctx = (RenderContext *) context;
  if (render_ctx->render_path == RenderPath::BACKGROUND)
    return S_OK;

  target = render_ctx->target;
  factory = render_ctx->factory;

  hr = factory->CreateRectangleGeometry (D2D1::RectF (0,
          strikethrough->offset, strikethrough->width,
          strikethrough->offset + strikethrough->thickness), &geometry);

  if (FAILED (hr)) {
    GST_WARNING ("Couldn't create geometry, 0x%x", (guint) hr);
    return hr;
  }

  hr = factory->CreateTransformedGeometry (geometry.Get (),
      D2D1::Matrix3x2F::Translation (origin_x, origin_y), &transformed);

  if (FAILED (hr)) {
    GST_WARNING ("Couldn't create transformed geometry, 0x%x", (guint) hr);
    return hr;
  }

  if (client_effect)
    client_effect->QueryInterface (IID_IGstDWriteTextEffect, &effect);

  if (effect) {
    D2D1_COLOR_F color;
    BOOL enabled;

    effect->GetBrushColor (GST_DWRITE_BRUSH_STRIKETHROUGH, &color, &enabled);
    if (enabled)
      target->CreateSolidColorBrush (color, &brush);
  } else {
    target->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black), &brush);
  }

  if (brush)
    target->FillGeometry (transformed.Get (), brush.Get ());

  return S_OK;
}

STDMETHODIMP
    IGstDWriteTextRenderer::DrawInlineObject (void *context, FLOAT origin_x,
    FLOAT origin_y, IDWriteInlineObject * inline_object, BOOL is_sideways,
    BOOL is_right_to_left, IUnknown * client_effect)
{
  GST_WARNING ("Not implemented");
  return E_NOTIMPL;
}

IGstDWriteTextRenderer::IGstDWriteTextRenderer (void)
{
}

IGstDWriteTextRenderer::~IGstDWriteTextRenderer (void)
{
  factory_->Release ();
}

STDMETHODIMP
    IGstDWriteTextRenderer::Draw (const D2D1_POINT_2F & origin,
    const RECT & client_rect, IDWriteTextLayout * layout,
    ID2D1RenderTarget * target)
{
  HRESULT hr;
  RenderContext context;
  ComPtr < ID2D1Factory > d2d_factory;
  UINT32 num_lines = 0;

  g_return_val_if_fail (layout != nullptr, E_INVALIDARG);
  g_return_val_if_fail (target != nullptr, E_INVALIDARG);

  hr = layout->GetLineMetrics (nullptr, 0, &num_lines);
  if (hr != E_NOT_SUFFICIENT_BUFFER)
    return hr;

  context.line_metrics.resize (num_lines);
  hr = layout->GetLineMetrics (&context.line_metrics[0],
      context.line_metrics.size (), &num_lines);
  if (FAILED (hr))
    return hr;

  target->GetFactory (&d2d_factory);
  context.render_path = RenderPath::BACKGROUND;
  context.client_rect = client_rect;
  context.target = target;
  context.factory = d2d_factory.Get ();

  hr = layout->Draw (&context, this, origin.x, origin.y);

  if (FAILED (hr)) {
    GST_WARNING ("Background Draw failed with 0x%x", (guint) hr);
    return hr;
  }

  if (context.bg_rect) {
    ComPtr < ID2D1SolidColorBrush > bg_brush;
    target->CreateSolidColorBrush (context.bg_color, &bg_brush);
    target->FillGeometry (context.bg_rect.Get (), bg_brush.Get ());
    context.bg_rect = nullptr;
  }

  context.render_path = RenderPath::TEXT;
  hr = layout->Draw (&context, this, origin.x, origin.y);

  if (FAILED (hr)) {
    GST_WARNING ("Draw failed with 0x%x", (guint) hr);
  }

  return hr;
}
