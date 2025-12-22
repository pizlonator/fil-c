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

#include "gstdwrite-effect.h"

/* *INDENT-OFF* */
STDMETHODIMP
IGstDWriteTextEffect::CreateInstance (IGstDWriteTextEffect ** effect)
{
  IGstDWriteTextEffect *self = new IGstDWriteTextEffect ();
  if (!self)
    return E_OUTOFMEMORY;

  *effect = self;

  return S_OK;
}

STDMETHODIMP_ (BOOL)
IGstDWriteTextEffect::IsEnabledColor (const D2D1_COLOR_F & color)
{
  if (color.r != 0 || color.g != 0 || color.b != 0 || color.a != 0)
    return TRUE;

  return FALSE;
}

STDMETHODIMP_ (ULONG)
IGstDWriteTextEffect::AddRef (void)
{
  return InterlockedIncrement (&ref_count_);
}

STDMETHODIMP_ (ULONG)
IGstDWriteTextEffect::Release (void)
{
  ULONG ref_count;

  ref_count = InterlockedDecrement (&ref_count_);

  if (ref_count == 0)
    delete this;

  return ref_count;
}

STDMETHODIMP
IGstDWriteTextEffect::QueryInterface (REFIID riid, void ** object)
{
  if (riid == IID_IUnknown) {
    *object = static_cast<IUnknown *>
        (static_cast<IGstDWriteTextEffect *> (this));
  } else if (riid == IID_IGstDWriteTextEffect) {
    *object = this;
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  AddRef ();

  return S_OK;
}

STDMETHODIMP
IGstDWriteTextEffect::Clone (IGstDWriteTextEffect ** effect)
{
  IGstDWriteTextEffect *copy = new IGstDWriteTextEffect ();

  if (!copy)
    return E_OUTOFMEMORY;

  for (UINT i = 0; i < GST_DWRITE_BRUSH_LAST; i++)
    copy->brush_[i] = this->brush_[i];

  copy->enable_color_font_ = enable_color_font_;

  *effect = copy;

  return S_OK;
}

STDMETHODIMP
IGstDWriteTextEffect::GetBrushColor (GST_DWRITE_BRUSH_TARGET target,
    D2D1_COLOR_F * color, BOOL * enabled)
{
  if (color)
    *color = brush_[target];

  if (enabled) {
    if (IGstDWriteTextEffect::IsEnabledColor (brush_[target]))
      *enabled = TRUE;
    else
      *enabled = FALSE;
  }

  return S_OK;
}

STDMETHODIMP
IGstDWriteTextEffect::SetBrushColor (GST_DWRITE_BRUSH_TARGET target,
    const D2D1_COLOR_F * color)
{
  if (!color)
    brush_[target] = D2D1::ColorF (D2D1::ColorF::Black, 0);
  else
    brush_[target] = *color;

  return S_OK;
}

STDMETHODIMP
IGstDWriteTextEffect::SetEnableColorFont (BOOL enable)
{
  enable_color_font_ = enable;

  return S_OK;
}

STDMETHODIMP
IGstDWriteTextEffect::GetEnableColorFont (BOOL * enable)
{
  if (!enable)
    return E_INVALIDARG;

  *enable = enable_color_font_ ;

  return S_OK;
}

IGstDWriteTextEffect::IGstDWriteTextEffect (void)
{
  for (UINT32 i = 0; i < GST_DWRITE_BRUSH_LAST; i++)
    brush_[i] = D2D1::ColorF (D2D1::ColorF::Black);

  /* Disable custom shadow effects by default */
  brush_[GST_DWRITE_BRUSH_SHADOW] = D2D1::ColorF (D2D1::ColorF::Black, 0);
}

IGstDWriteTextEffect::~IGstDWriteTextEffect (void)
{
}
/* *INDENT-ON* */
