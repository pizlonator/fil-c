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

enum GST_DWRITE_BRUSH_TARGET
{
  GST_DWRITE_BRUSH_FORGROUND = 0,
  GST_DWRITE_BRUSH_OUTLINE,
  GST_DWRITE_BRUSH_UNDERLINE,
  GST_DWRITE_BRUSH_STRIKETHROUGH,
  GST_DWRITE_BRUSH_SHADOW,
  GST_DWRITE_BRUSH_BACKGROUND,
  GST_DWRITE_BRUSH_LAST
};

DEFINE_GUID (IID_IGstDWriteTextEffect, 0x23c579ae, 0x2e18,
    0x11ed, 0xa2, 0x61, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);
class IGstDWriteTextEffect : public IUnknown
{
public:
  static STDMETHODIMP         CreateInstance (IGstDWriteTextEffect ** effect);
  static STDMETHODIMP_ (BOOL) IsEnabledColor (const D2D1_COLOR_F & color);

  /* IUnknown */
  STDMETHODIMP_ (ULONG) AddRef  (void);
  STDMETHODIMP_ (ULONG) Release (void);
  STDMETHODIMP QueryInterface   (REFIID riid,
                                 void ** object);

  /* effect methods */
  STDMETHODIMP Clone         (IGstDWriteTextEffect ** effect);
  STDMETHODIMP GetBrushColor (GST_DWRITE_BRUSH_TARGET target,
                              D2D1_COLOR_F * color,
                              BOOL * enabled);
  STDMETHODIMP SetBrushColor (GST_DWRITE_BRUSH_TARGET target,
                              const D2D1_COLOR_F * color);

  STDMETHODIMP SetEnableColorFont (BOOL enable);

  STDMETHODIMP GetEnableColorFont (BOOL * enable);

private:
  IGstDWriteTextEffect (void);
  virtual ~IGstDWriteTextEffect (void);

private:
  ULONG ref_count_ = 1;
  D2D1_COLOR_F brush_[GST_DWRITE_BRUSH_LAST];
  BOOL enable_color_font_ = FALSE;
};
