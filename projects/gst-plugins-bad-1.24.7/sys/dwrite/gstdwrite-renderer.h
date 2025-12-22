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

#pragma once

#include <gst/gst.h>
#include "gstdwrite-enums.h"
#include "gstdwrite-effect.h"

DEFINE_GUID (IID_IGstDWriteTextRenderer, 0xb969dc25, 0xf7d2,
    0x4cf8, 0x92, 0x0f, 0x5a, 0x27, 0xd1, 0x6d, 0x03, 0x6a);

class IGstDWriteTextRenderer : public IDWriteTextRenderer
{
public:
  static STDMETHODIMP CreateInstance (IDWriteFactory * factory,
                                      IGstDWriteTextRenderer ** renderer);

  /* IUnknown */
  STDMETHODIMP_ (ULONG) AddRef (void);
  STDMETHODIMP_ (ULONG) Release (void);
  STDMETHODIMP QueryInterface (REFIID riid,
                               void ** object);

  /* IDWritePixelSnapping */
  STDMETHODIMP IsPixelSnappingDisabled (void * context,
                                        BOOL * is_disabled);
  STDMETHODIMP GetCurrentTransform     (void * context,
                                        DWRITE_MATRIX * transform);
  STDMETHODIMP GetPixelsPerDip         (void * context,
                                        FLOAT * pixels_per_dip);

  /* IDWriteTextRenderer */
  STDMETHODIMP DrawGlyphRun      (void * context,
                                  FLOAT origin_x,
                                  FLOAT origin_y,
                                  DWRITE_MEASURING_MODE mode,
                                  DWRITE_GLYPH_RUN const *glyph_run,
                                  DWRITE_GLYPH_RUN_DESCRIPTION const *glyph_run_desc,
                                  IUnknown * client_effect);
  STDMETHODIMP DrawUnderline     (void * context,
                                  FLOAT origin_x,
                                  FLOAT origin_y,
                                  DWRITE_UNDERLINE const * underline,
                                  IUnknown * client_effect);
  STDMETHODIMP DrawStrikethrough (void * context,
                                  FLOAT origin_x,
                                  FLOAT origin_y,
                                  DWRITE_STRIKETHROUGH const * strikethrough,
                                  IUnknown * client_effect);
  STDMETHODIMP DrawInlineObject  (void * context,
                                  FLOAT origin_x,
                                  FLOAT origin_y,
                                  IDWriteInlineObject * inline_object,
                                  BOOL is_sideways,
                                  BOOL is_right_to_left,
                                  IUnknown * client_effect);

  /* Entry point for drawing text */
  STDMETHODIMP Draw (const D2D1_POINT_2F & origin,
                     const RECT & client_rect,
                     IDWriteTextLayout * layout,
                     ID2D1RenderTarget * target);

private:
  IGstDWriteTextRenderer (void);
  virtual ~IGstDWriteTextRenderer (void);

private:
  IDWriteFactory *factory_;
  ULONG ref_count_ = 1;
};
