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

#ifndef __GST_MF_VIDEO_BUFFER_H__
#define __GST_MF_VIDEO_BUFFER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <windows.h>
#include <mfobjects.h>
#include <mferror.h>
#include <mutex>

#ifndef __cplusplus
#error IGstMFVideoBuffer interface doesn't provide C API
#endif

/* Define UUID for QueryInterface() */
class DECLSPEC_UUID("ce922806-a8a6-4e1e-871f-e0cdd5fc9899")
IGstMFVideoBuffer : public IMFMediaBuffer, public IMF2DBuffer
{
public:
  static HRESULT CreateInstance (GstVideoInfo * info,
                                 IMFMediaBuffer ** buffer);

  static HRESULT CreateInstanceWrapped (GstVideoInfo * info,
                                        BYTE * data,
                                        DWORD length,
                                        IMFMediaBuffer ** buffer);

  /* notify will be called right after this object is destroyed */
  HRESULT SetUserData (gpointer user_data,
                       GDestroyNotify notify);

  HRESULT GetUserData (gpointer * user_data);

  /* IUnknown interface */
  STDMETHODIMP_ (ULONG) AddRef (void);
  STDMETHODIMP_ (ULONG) Release (void);
  STDMETHODIMP QueryInterface (REFIID riid,
                               void ** object);

  /* IMFMediaBuffer interface
   *
   * Caller of this interface expects returned raw memory layout via Lock()
   * has no padding with default stride. If stored memory layout consists of
   * non-default stride and/or with some padding, then Lock() / Unlock() would
   * cause memory copy therefore.
   * Caller should avoid to use this interface as much as possible
   * if IMF2DBuffer interface available.
   */
  STDMETHODIMP Lock (BYTE ** buffer,
                     DWORD * max_length,
                     DWORD * current_length);
  STDMETHODIMP Unlock (void);
  STDMETHODIMP GetCurrentLength (DWORD * length);
  STDMETHODIMP SetCurrentLength (DWORD length);
  STDMETHODIMP GetMaxLength (DWORD * length);

  /* IMF2DBuffer interface
   *
   * this interface supports any raw memory layout with non-default stride.
   * But more complex layout (padding at bottom for instance) is not supported.
   */
  STDMETHODIMP Lock2D (BYTE ** buffer,
                       LONG * pitch);
  STDMETHODIMP Unlock2D (void);
  STDMETHODIMP GetScanline0AndPitch (BYTE ** buffer,
                                     LONG * pitch);
  STDMETHODIMP IsContiguousFormat (BOOL * contiguous);
  STDMETHODIMP GetContiguousLength (DWORD * length);
  STDMETHODIMP ContiguousCopyTo (BYTE * dest_buffer,
                                 DWORD dest_buffer_length);
  STDMETHODIMP ContiguousCopyFrom (const BYTE * src_buffer,
                                   DWORD src_buffer_length);

private:
  IGstMFVideoBuffer (void);
  ~IGstMFVideoBuffer (void);

  HRESULT Initialize (GstVideoInfo * info);
  HRESULT InitializeWrapped (GstVideoInfo * info,
                             BYTE * data,
                             DWORD length);
  HRESULT ContiguousCopyToUnlocked (BYTE * dest_buffer,
                                    DWORD dest_buffer_length);
  HRESULT ContiguousCopyFromUnlocked (const BYTE * src_buffer,
                                      DWORD src_buffer_length);

private:
  ULONG ref_count_;
  DWORD current_len_;
  DWORD contiguous_len_;
  BYTE *data_;
  BYTE *contiguous_data_;
  GstVideoInfo *info_;
  GstVideoInfo *contiguous_info_;
  BOOL contiguous_;
  std::mutex lock_;
  bool locked_;
  bool wrapped_;

  gpointer user_data_;
  GDestroyNotify notify_;
};

#endif /* __GST_MF_VIDEO_BUFFER_H__ */
