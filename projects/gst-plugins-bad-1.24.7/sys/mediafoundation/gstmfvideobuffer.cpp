/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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

#include "gstmfvideobuffer.h"
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_mf_video_buffer_debug);
#define GST_CAT_DEFAULT gst_mf_video_buffer_debug

/* *INDENT-OFF* */
IGstMFVideoBuffer::IGstMFVideoBuffer ()
  : ref_count_ (1)
  , current_len_ (0)
  , contiguous_len_ (0)
  , data_ (nullptr)
  , contiguous_data_ (nullptr)
  , info_ (nullptr)
  , contiguous_info_ (nullptr)
  , locked_ (false)
  , wrapped_ (false)
  , user_data_ (nullptr)
  , notify_ (nullptr)
{
}

IGstMFVideoBuffer::~IGstMFVideoBuffer ()
{
  if (info_)
    gst_video_info_free (info_);
  if (contiguous_info_)
    gst_video_info_free (contiguous_info_);

  g_free (contiguous_data_);

  if (!wrapped_)
    g_free (data_);
}

HRESULT
IGstMFVideoBuffer::CreateInstance (GstVideoInfo * info,
    IMFMediaBuffer ** buffer)
{
  HRESULT hr = S_OK;
  IGstMFVideoBuffer * self;

  if (!info || !buffer)
    return E_INVALIDARG;

  self = new IGstMFVideoBuffer ();

  if (!self)
    return E_OUTOFMEMORY;

  hr = self->Initialize (info);

  if (SUCCEEDED (hr))
    hr = self->QueryInterface (IID_PPV_ARGS (buffer));

  self->Release ();

  return hr;
}

HRESULT
IGstMFVideoBuffer::CreateInstanceWrapped (GstVideoInfo * info,
    BYTE * data, DWORD length, IMFMediaBuffer ** buffer)
{
  HRESULT hr = S_OK;
  IGstMFVideoBuffer * self;

  if (!info || !data || length == 0 || !buffer)
    return E_INVALIDARG;

  self = new IGstMFVideoBuffer ();

  if (!self)
    return E_OUTOFMEMORY;

  hr = self->InitializeWrapped (info, data, length);

  if (SUCCEEDED (hr))
    hr = self->QueryInterface (IID_PPV_ARGS (buffer));

  self->Release ();

  return hr;
}

HRESULT
IGstMFVideoBuffer::Initialize (GstVideoInfo * info)
{
  if (!info)
    return E_INVALIDARG;

  info_ = gst_video_info_copy (info);
  contiguous_info_ = gst_video_info_new ();

  /* check if padding is required */
  gst_video_info_set_format (contiguous_info_, GST_VIDEO_INFO_FORMAT (info_),
      GST_VIDEO_INFO_WIDTH (info_), GST_VIDEO_INFO_HEIGHT (info_));

  contiguous_ = GST_VIDEO_INFO_SIZE (info_) ==
      GST_VIDEO_INFO_SIZE (contiguous_info_);

  contiguous_len_ = GST_VIDEO_INFO_SIZE (contiguous_info_);
  /* NOTE: {Set,Get}CurrentLength will not be applied for
   * IMF2DBuffer interface */

  current_len_ = contiguous_len_;

  data_ = (BYTE *) g_malloc0 (GST_VIDEO_INFO_SIZE (info_));

  if (!data_)
    return E_OUTOFMEMORY;

  return S_OK;
}

HRESULT
IGstMFVideoBuffer::InitializeWrapped (GstVideoInfo * info, BYTE * data,
    DWORD length)
{
  if (!info || !data || length == 0)
    return E_INVALIDARG;

  if (length < GST_VIDEO_INFO_SIZE (info))
    return E_INVALIDARG;

  info_ = gst_video_info_copy (info);
  contiguous_info_ = gst_video_info_new ();

  /* check if padding is required */
  gst_video_info_set_format (contiguous_info_, GST_VIDEO_INFO_FORMAT (info_),
      GST_VIDEO_INFO_WIDTH (info_), GST_VIDEO_INFO_HEIGHT (info_));

  contiguous_ = GST_VIDEO_INFO_SIZE (info_) ==
      GST_VIDEO_INFO_SIZE (contiguous_info_);

  contiguous_len_ = GST_VIDEO_INFO_SIZE (contiguous_info_);

  current_len_ = contiguous_len_;

  data_ = data;
  wrapped_ = true;

  return S_OK;
}

HRESULT
IGstMFVideoBuffer::SetUserData (gpointer user_data, GDestroyNotify notify)
{
  GDestroyNotify old_notify = notify_;
  gpointer old_user_data = user_data_;

  if (old_notify)
    old_notify (old_user_data);

  user_data_ = user_data;
  notify_ = notify;

  return S_OK;
}

HRESULT
IGstMFVideoBuffer::GetUserData (gpointer * user_data)
{
  if (!user_data)
    return E_INVALIDARG;

  *user_data = user_data_;

  return S_OK;
}

/* IUnknown interface */
STDMETHODIMP_ (ULONG)
IGstMFVideoBuffer::AddRef (void)
{
  GST_TRACE ("%p, %d", this, ref_count_);
  return InterlockedIncrement (&ref_count_);
}

STDMETHODIMP_ (ULONG)
IGstMFVideoBuffer::Release (void)
{
  ULONG ref_count;

  GST_TRACE ("%p, %d", this, ref_count_);
  ref_count = InterlockedDecrement (&ref_count_);

  if (ref_count == 0) {
    GDestroyNotify old_notify = notify_;
    gpointer old_user_data = user_data_;

    GST_TRACE ("Delete instance %p", this);
    delete this;

    if (old_notify)
      old_notify (old_user_data);
  }

  return ref_count;
}

STDMETHODIMP
IGstMFVideoBuffer::QueryInterface (REFIID riid, void ** object)
{
  if (!object)
    return E_POINTER;

  if (riid == IID_IUnknown) {
    GST_TRACE ("query IUnknown interface %p", this);
    *object = static_cast<IUnknown *> (static_cast<IMFMediaBuffer *> (this));
  } else if (riid == __uuidof(IMFMediaBuffer)) {
    GST_TRACE ("query IMFMediaBuffer interface %p", this);
    *object = static_cast<IMFMediaBuffer *> (this);
  } else if (riid == __uuidof(IMF2DBuffer)) {
    GST_TRACE ("query IMF2DBuffer interface %p", this);
    *object = static_cast<IMF2DBuffer *> (this);
  } else if (riid == __uuidof(IGstMFVideoBuffer)) {
    GST_TRACE ("query IGstMFVideoBuffer interface %p", this);
    *object = this;
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  AddRef();

  return S_OK;
}

/* IMFMediaBuffer interface */
STDMETHODIMP
IGstMFVideoBuffer::Lock (BYTE ** buffer, DWORD * max_length,
    DWORD * current_length)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  if (locked_) {
    GST_LOG ("%p, Already locked", this);
    return S_OK;
  }

  locked_ = true;

  if (contiguous_) {
    *buffer = data_;
    goto done;
  }

  /* IMFMediaBuffer::Lock method should return contiguous memory */
  if (!contiguous_data_)
    contiguous_data_ = (BYTE *) g_malloc0 (contiguous_len_);

  ContiguousCopyToUnlocked (contiguous_data_, contiguous_len_);
  *buffer = contiguous_data_;

done:
  if (max_length)
    *max_length = contiguous_len_;
  if (current_length)
    *current_length = current_len_;

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::Unlock (void)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  if (!locked_) {
    GST_LOG ("%p, No previous Lock call", this);
    return S_OK;
  }

  locked_ = false;

  if (contiguous_) {
    GST_TRACE ("%p, Have configured contiguous data", this);
    return S_OK;
  }

  /* copy back to original data */
  ContiguousCopyFromUnlocked (contiguous_data_, contiguous_len_);

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::GetCurrentLength (DWORD * length)
{
  std::lock_guard<std::mutex> lock(lock_);

  *length = current_len_;

  GST_TRACE ("%p, %d", this, current_len_);

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::SetCurrentLength (DWORD length)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p %d", this, length);

  if (length > contiguous_len_) {
    GST_LOG ("%p, Requested length %d is larger than contiguous_len %d",
        this, length, contiguous_len_);
    return E_INVALIDARG;
  }

  current_len_ = length;

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::GetMaxLength (DWORD * length)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  *length = contiguous_len_;

  return S_OK;
}

/* IMF2DBuffer */
STDMETHODIMP
IGstMFVideoBuffer::Lock2D (BYTE ** buffer, LONG * pitch)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  if (locked_) {
    GST_LOG ("%p, Already locked", this);
    return MF_E_INVALIDREQUEST;
  }

  locked_ = true;

  *buffer = data_;
  *pitch = GST_VIDEO_INFO_PLANE_STRIDE (info_, 0);

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::Unlock2D (void)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  if (!locked_) {
    GST_LOG ("%p, No previous Lock2D call", this);
    return S_OK;
  }

  locked_ = false;

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::GetScanline0AndPitch (BYTE ** buffer, LONG * pitch)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  /* Lock2D must be called before */
  if (!locked_) {
    GST_LOG ("%p, Invalid call, Lock2D must be called before", this);
    return ERROR_INVALID_FUNCTION;
  }

  *buffer = data_;
  *pitch = GST_VIDEO_INFO_PLANE_STRIDE (info_, 0);

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::IsContiguousFormat (BOOL * contiguous)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  *contiguous = contiguous_;

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::GetContiguousLength (DWORD * length)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  *length = contiguous_len_;

  return S_OK;
}

STDMETHODIMP
IGstMFVideoBuffer::ContiguousCopyTo (BYTE * dest_buffer,
    DWORD dest_buffer_length)
{
  std::lock_guard<std::mutex> lock(lock_);

  return ContiguousCopyToUnlocked (dest_buffer, dest_buffer_length);
}

STDMETHODIMP
IGstMFVideoBuffer::ContiguousCopyFrom (const BYTE * src_buffer,
    DWORD src_buffer_length)
{
  std::lock_guard<std::mutex> lock(lock_);

  GST_TRACE ("%p", this);

  return ContiguousCopyFromUnlocked (src_buffer, src_buffer_length);
}

HRESULT
IGstMFVideoBuffer::ContiguousCopyToUnlocked (BYTE * dest_buffer,
    DWORD dest_buffer_length)
{
  GST_TRACE ("%p", this);

  if (!dest_buffer || dest_buffer_length < contiguous_len_)
    return E_INVALIDARG;

  if (contiguous_) {
    memcpy (dest_buffer, data_, current_len_);
    return S_OK;
  }

  for (gint i = 0; i < GST_VIDEO_INFO_N_PLANES (info_); i++) {
    BYTE *src, *dst;
    guint src_stride, dst_stride;
    guint width, height;

    src = data_ + GST_VIDEO_INFO_PLANE_OFFSET (info_, i);
    dst = dest_buffer + GST_VIDEO_INFO_PLANE_OFFSET (contiguous_info_, i);

    src_stride = GST_VIDEO_INFO_PLANE_STRIDE (info_, i);
    dst_stride = GST_VIDEO_INFO_PLANE_STRIDE (contiguous_info_, i);

    width = GST_VIDEO_INFO_COMP_WIDTH (info_, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (info_, i);
    height = GST_VIDEO_INFO_COMP_HEIGHT (info_, i);

    for (gint j  = 0; j < height; j++) {
      memcpy (dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }

  return S_OK;
}

HRESULT
IGstMFVideoBuffer::ContiguousCopyFromUnlocked (const BYTE * src_buffer,
    DWORD src_buffer_length)
{
  gint offset;

  GST_TRACE ("%p", this);

  if (!src_buffer)
    return E_INVALIDARG;

  /* Nothing to copy */
  if (src_buffer_length == 0)
    return S_OK;

  if (contiguous_) {
    memcpy (data_, src_buffer, src_buffer_length);
    return S_OK;
  }

  for (gint i = 0; i < GST_VIDEO_INFO_N_PLANES (info_); i++) {
    BYTE *dst;
    guint src_stride, dst_stride;
    guint width, height;

    offset = GST_VIDEO_INFO_PLANE_OFFSET (contiguous_info_, i);

    dst = data_ + GST_VIDEO_INFO_PLANE_OFFSET (info_, i);

    src_stride = GST_VIDEO_INFO_PLANE_STRIDE (contiguous_info_, i);
    dst_stride = GST_VIDEO_INFO_PLANE_STRIDE (info_, i);

    width = GST_VIDEO_INFO_COMP_WIDTH (info_, i)
        * GST_VIDEO_INFO_COMP_PSTRIDE (info_, i);
    height = GST_VIDEO_INFO_COMP_HEIGHT (info_, i);

    for (gint j  = 0; j < height; j++) {
      gint to_copy = 0;

      if (offset + width < src_buffer_length)
        to_copy = width;
      else
        to_copy = (gint) src_buffer_length - offset;

      if (to_copy <= 0)
        return S_OK;

      memcpy (dst, src_buffer + offset, to_copy);

      offset += src_stride;
      dst += dst_stride;
    }
  }

  return S_OK;
}

/* *INDENT-ON* */
