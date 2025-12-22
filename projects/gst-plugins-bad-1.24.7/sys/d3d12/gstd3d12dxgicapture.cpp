/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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

/*
 * The MIT License (MIT)
 *
 * Copyright (c) Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstd3d12dxgicapture.h"
#include "gstd3d12pluginutils.h"
#include <d3d11_3.h>
#include <directx/d3dx12.h>
#include <string.h>
#include <mutex>
#include <vector>
#include <memory>
#include <future>
#include <wrl.h>
#include "PSMain_sample.h"
#include "VSMain_coord.h"

#define _XM_NO_INTRINSICS_
#include <DirectXMath.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d12_screen_capture_debug);
#define GST_CAT_DEFAULT gst_d3d12_screen_capture_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace DirectX;

/* List of GstD3D12DxgiCapture weakref */
static std::mutex g_g_dupl_list_lock;
static GList *g_dupl_list = nullptr;

/* Below implementation were taken from Microsoft sample
 * https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/DXGIDesktopDuplication
 */

/* List of expected error cases */
/* These are the errors we expect from general Dxgi API due to a transition */
static HRESULT SystemTransitionsExpectedErrors[] = {
  DXGI_ERROR_DEVICE_REMOVED,
  DXGI_ERROR_ACCESS_LOST,
  static_cast<HRESULT>(WAIT_ABANDONED),
  S_OK
};

/* These are the errors we expect from IDXGIOutput1::DuplicateOutput
 * due to a transition */
static HRESULT CreateDuplicationExpectedErrors[] = {
  DXGI_ERROR_DEVICE_REMOVED,
  static_cast<HRESULT>(E_ACCESSDENIED),
  DXGI_ERROR_SESSION_DISCONNECTED,
  S_OK
};

/* These are the errors we expect from IDXGIOutputDuplication methods
 * due to a transition */
static HRESULT FrameInfoExpectedErrors[] = {
  DXGI_ERROR_DEVICE_REMOVED,
  DXGI_ERROR_ACCESS_LOST,
  S_OK
};

static GstFlowReturn
flow_return_from_hr (ID3D11Device * device,
    HRESULT hr, HRESULT * expected_errors = nullptr)
{
  HRESULT translated_hr = hr;

  /* On an error check if the DX device is lost */
  if (device) {
    HRESULT remove_reason = device->GetDeviceRemovedReason ();

    switch (remove_reason) {
      case DXGI_ERROR_DEVICE_REMOVED:
      case DXGI_ERROR_DEVICE_RESET:
      case static_cast<HRESULT>(E_OUTOFMEMORY):
        /* Our device has been stopped due to an external event on the GPU so
         * map them all to device removed and continue processing the condition
         */
        translated_hr = DXGI_ERROR_DEVICE_REMOVED;
        break;
      case S_OK:
        /* Device is not removed so use original error */
        break;
      default:
        /* Device is removed but not a error we want to remap */
        translated_hr = remove_reason;
        break;
    }
  }

  /* Check if this error was expected or not */
  if (expected_errors) {
    HRESULT* rst = expected_errors;

    while (*rst != S_OK) {
      if (*rst == translated_hr)
        return GST_D3D12_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR;

      rst++;
    }
  }

  return GST_FLOW_ERROR;
}

struct PtrInfo
{
  PtrInfo ()
  {
    LastTimeStamp.QuadPart = 0;
    position_info.Visible = FALSE;
  }

  void buildMonochrom ()
  {
    width_ = shape_info.Width;
    height_ = shape_info.Height / 2;
    stride_ = width_ * 4;
    UINT pstride = 4;
    auto size = height_ * stride_;
    texture_.resize (size);
    xor_texture_.resize (size);

    const BYTE black[] = { 0, 0, 0, 0xff };
    const BYTE white[] = { 0xff, 0xff, 0xff, 0xff };
    const BYTE transparent[] = { 0, 0, 0, 0 };

    for (UINT row = 0; row < height_; row++) {
      for (UINT col = 0; col < width_; col++) {
        auto src_pos = (row * shape_info.Pitch) + (col / 8);
        auto and_mask = (shape_buffer[src_pos] >> (7 - (col % 8))) & 0x1;
        auto xor_mask = (shape_buffer[src_pos + size] >> (7 - (col % 8))) & 0x1;
        auto dst_pos = (row * stride_) + (col * pstride);

        if (and_mask) {
          memcpy (texture_.data () + dst_pos,
              transparent, sizeof (transparent));

          if (xor_mask) {
            memcpy (xor_texture_.data () + dst_pos, white, sizeof (white));
          } else {
            memcpy (xor_texture_.data () + dst_pos, transparent,
                sizeof (transparent));
          }
        } else {
          memcpy (texture_.data () + dst_pos, black, sizeof (black));
          memcpy (xor_texture_.data () + dst_pos, transparent,
                sizeof (transparent));
        }
      }
    }
  }

  void buildMaskedColor ()
  {
    width_ = shape_info.Width;
    height_ = shape_info.Height;
    stride_ = shape_info.Pitch;

    auto size = height_ * stride_;
    UINT pstride = 4;
    texture_.resize (size);
    xor_texture_.resize (size);
    memcpy (texture_.data (), shape_buffer.data (), size);
    memcpy (xor_texture_.data (), shape_buffer.data (), size);

    for (UINT row = 0; row < height_; row++) {
      for (UINT col = 0; col < width_; col++) {
        auto mask_pos = row * stride_ + col * pstride + 3;
        texture_[mask_pos] = shape_buffer[mask_pos] ? 0 : 0xff;
        xor_texture_[mask_pos] = shape_buffer[mask_pos] ? 0xff : 0;
      }
    }
  }

  void BuildTexture ()
  {
    texture_.clear ();
    xor_texture_.clear ();
    width_ = 0;
    height_ = 0;
    stride_ = 0;

    switch (shape_info.Type) {
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
      {
        width_ = shape_info.Width;
        height_ = shape_info.Height;
        stride_ = shape_info.Pitch;

        auto size = stride_ * height_;
        texture_.resize (size);
        memcpy (texture_.data (), shape_buffer.data (), size);
        break;
      }
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        buildMonochrom ();
        break;
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        buildMaskedColor ();
        break;
      default:
        GST_WARNING ("Unexpected shape type %u", shape_info.Type);
        break;
    }

    token_++;
  }

  std::vector<BYTE> shape_buffer;
  DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info;
  DXGI_OUTDUPL_POINTER_POSITION position_info;
  LARGE_INTEGER LastTimeStamp;

  UINT width_ = 0;
  UINT height_ = 0;
  UINT stride_ = 0;
  std::vector<BYTE> texture_;
  std::vector<BYTE> xor_texture_;
  UINT64 token_ = 0;
};

struct MoveRectData
{
  RECT src_rect;
  RECT dst_rect;
  D3D12_BOX box;
};

struct VERTEX
{
  XMFLOAT3 Pos;
  XMFLOAT2 TexCoord;
};

class DesktopDupCtx
{
public:
  DesktopDupCtx () {}

  GstFlowReturn Init (HMONITOR monitor)
  {
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    ComPtr<IDXGIOutput1> output1;

    HRESULT hr = gst_d3d12_screen_capture_find_output_for_monitor (monitor,
        &adapter, &output);
    if (FAILED (hr)) {
      GST_ERROR ("Couldn't get adapter and output for monitor");
      return GST_FLOW_ERROR;
    }

    hr = output.As (&output1);
    if (FAILED (hr)) {
      GST_ERROR ("Couldn't get IDXGIOutput1 interface, hr 0x%x", (guint) hr);
      return GST_FLOW_ERROR;
    }

    HDESK hdesk = OpenInputDesktop (0, FALSE, GENERIC_ALL);
    if (hdesk) {
      if (!SetThreadDesktop (hdesk)) {
        GST_WARNING ("SetThreadDesktop() failed, error %lu", GetLastError());
      }

      CloseDesktop (hdesk);
    } else {
      GST_WARNING ("OpenInputDesktop() failed, error %lu", GetLastError());
    }

    static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };

    hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
        G_N_ELEMENTS (feature_levels), D3D11_SDK_VERSION, &device_, nullptr,
        nullptr);
    if (FAILED (hr)) {
      hr = D3D11CreateDevice (adapter.Get (), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
          D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature_levels[1],
          G_N_ELEMENTS (feature_levels) - 1, D3D11_SDK_VERSION, &device_,
          nullptr, nullptr);
    }

    if (FAILED (hr)) {
      GST_ERROR ("Couldn't create d3d11 device");
      return GST_FLOW_ERROR;
    }

    /* FIXME: Use DuplicateOutput1 to avoid potentail color conversion */
    hr = output1->DuplicateOutput(device_.Get(), &dupl_);
    if (FAILED (hr)) {
      if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
        GST_ERROR ("Hit the max allowed number of Desktop Duplication session");
        return GST_FLOW_ERROR;
      }

      /* Seems to be one limitation of Desktop Duplication API design
       * See
       * https://docs.microsoft.com/en-US/troubleshoot/windows-client/shell-experience/error-when-dda-capable-app-is-against-gpu
       */
      if (hr == DXGI_ERROR_UNSUPPORTED) {
        GST_WARNING ("IDXGIOutput1::DuplicateOutput returned "
            "DXGI_ERROR_UNSUPPORTED, possiblely application is run against a "
            "discrete GPU");
        return GST_D3D12_SCREEN_CAPTURE_FLOW_UNSUPPORTED;
      }

      return flow_return_from_hr (device_.Get(), hr,
          CreateDuplicationExpectedErrors);
    }

    dupl_->GetDesc (&output_desc_);

    return GST_FLOW_OK;
  }

  void ReleaseFrame ()
  {
    if (acquired_frame_) {
      acquired_frame_ = nullptr;
      dupl_->ReleaseFrame ();
    }
  }

  GstFlowReturn AcquireNextFrame (IDXGIResource ** resource)
  {
    HRESULT hr;

    move_rect_.clear ();
    dirty_rect_.clear ();
    dirty_vertex_.clear ();

    if (acquired_frame_) {
      acquired_frame_ = nullptr;
      dupl_->ReleaseFrame ();
    }

    hr = dupl_->AcquireNextFrame(0, &frame_info_, &acquired_frame_);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
      return GST_FLOW_OK;

    if (FAILED (hr)) {
      GST_WARNING ("AcquireNextFrame failed with 0x%x", (guint) hr);
      return flow_return_from_hr (device_.Get (), hr, FrameInfoExpectedErrors);
    }

    metadata_buffer_.resize (frame_info_.TotalMetadataBufferSize);
    if (frame_info_.TotalMetadataBufferSize > 0) {
      UINT buf_size = frame_info_.TotalMetadataBufferSize;
      hr = dupl_->GetFrameMoveRects (buf_size,
         (DXGI_OUTDUPL_MOVE_RECT *) metadata_buffer_.data (), &buf_size);
      if (FAILED (hr)) {
        GST_ERROR ("Couldn't get move rect, hr: 0x%x", (guint) hr);
        return flow_return_from_hr (device_.Get (),
            hr, FrameInfoExpectedErrors);
      }

      auto move_count = buf_size / sizeof (DXGI_OUTDUPL_MOVE_RECT);
      buildMoveRects (move_count);

      auto dirty_rects = metadata_buffer_.data () + buf_size;
      buf_size = frame_info_.TotalMetadataBufferSize - buf_size;

      hr = dupl_->GetFrameDirtyRects (buf_size, (RECT *) dirty_rects,
          &buf_size);
      if (FAILED (hr)) {
        GST_ERROR ("Couldn't get dirty rect, hr: 0x%x", (guint) hr);
        return flow_return_from_hr (device_.Get (),
            hr, FrameInfoExpectedErrors);
      }

      auto dirty_count = buf_size / sizeof (RECT);
      buildDirtyVertex ((RECT *) dirty_rects, dirty_count);
    }

    if (frame_info_.LastMouseUpdateTime.QuadPart != 0) {
      ptr_info_.position_info = frame_info_.PointerPosition;
      ptr_info_.LastTimeStamp = frame_info_.LastMouseUpdateTime;

      if (frame_info_.PointerShapeBufferSize > 0) {
        UINT buf_size;
        ptr_info_.shape_buffer.resize (frame_info_.PointerShapeBufferSize);
        hr = dupl_->GetFramePointerShape (frame_info_.PointerShapeBufferSize,
            (void *) ptr_info_.shape_buffer.data (), &buf_size,
            &ptr_info_.shape_info);
        if (FAILED (hr)) {
          return flow_return_from_hr(device_.Get (),
              hr, FrameInfoExpectedErrors);
        }

        ptr_info_.BuildTexture ();
      }
    }

    *resource = acquired_frame_.Get ();
    (*resource)->AddRef ();

    return GST_FLOW_OK;
  }

  void GetSize (guint * width, guint * height)
  {
    *width = output_desc_.ModeDesc.Width;
    *height = output_desc_.ModeDesc.Height;
  }

  DXGI_OUTDUPL_DESC GetDesc ()
  {
    return output_desc_;
  }

  UINT GetMoveCount ()
  {
    return move_rect_.size ();
  }

  const std::vector<MoveRectData> & GetMoveRects ()
  {
    return move_rect_;
  }

  UINT GetDirtyCount ()
  {
    return dirty_rect_.size ();
  }

  const std::vector<RECT> & GetDirtyRects ()
  {
    return dirty_rect_;
  }

  const std::vector<VERTEX> & GetDirtyVertex ()
  {
    return dirty_vertex_;
  }

  const PtrInfo & GetPointerInfo ()
  {
    return ptr_info_;
  }

private:
  void buildMoveRects (UINT move_count)
  {
    INT width = (INT) output_desc_.ModeDesc.Width;
    INT height = (INT) output_desc_.ModeDesc.Height;

    for (UINT i = 0; i < move_count; i++) {
      DXGI_OUTDUPL_MOVE_RECT *move_rect =
          ((DXGI_OUTDUPL_MOVE_RECT *) metadata_buffer_.data ()) + i;
      RECT src_rect;
      RECT dst_rect;

      switch (output_desc_.Rotation) {
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        case DXGI_MODE_ROTATION_IDENTITY:
          src_rect.left = move_rect->SourcePoint.x;
          src_rect.top = move_rect->SourcePoint.y;
          src_rect.right = move_rect->SourcePoint.x +
              move_rect->DestinationRect.right - move_rect->DestinationRect.left;
          src_rect.bottom = move_rect->SourcePoint.y +
              move_rect->DestinationRect.bottom - move_rect->DestinationRect.top;
          dst_rect = move_rect->DestinationRect;
          break;
        case DXGI_MODE_ROTATION_ROTATE90:
          src_rect.left = height - (move_rect->SourcePoint.y +
              move_rect->DestinationRect.bottom - move_rect->DestinationRect.top);
          src_rect.top = move_rect->SourcePoint.x;
          src_rect.right = height - move_rect->SourcePoint.y;
          src_rect.bottom = move_rect->SourcePoint.x +
              move_rect->DestinationRect.right - move_rect->DestinationRect.left;

          dst_rect.left = height - move_rect->DestinationRect.bottom;
          dst_rect.top = move_rect->DestinationRect.left;
          dst_rect.right = height - move_rect->DestinationRect.top;
          dst_rect.bottom = move_rect->DestinationRect.right;
          break;
        case DXGI_MODE_ROTATION_ROTATE180:
          src_rect.left = width - (move_rect->SourcePoint.x +
              move_rect->DestinationRect.right - move_rect->DestinationRect.left);
          src_rect.top = height - (move_rect->SourcePoint.y +
              move_rect->DestinationRect.bottom - move_rect->DestinationRect.top);
          src_rect.right = width - move_rect->SourcePoint.x;
          src_rect.bottom = height - move_rect->SourcePoint.y;

          dst_rect.left = width - move_rect->DestinationRect.right;
          dst_rect.top = height - move_rect->DestinationRect.bottom;
          dst_rect.right = width - move_rect->DestinationRect.left;
          dst_rect.bottom =  height - move_rect->DestinationRect.top;
          break;
        case DXGI_MODE_ROTATION_ROTATE270:
          src_rect.left = move_rect->SourcePoint.x;
          src_rect.top = width - (move_rect->SourcePoint.x +
              move_rect->DestinationRect.right - move_rect->DestinationRect.left);
          src_rect.right = move_rect->SourcePoint.y +
              move_rect->DestinationRect.bottom - move_rect->DestinationRect.top;
          src_rect.bottom = width - move_rect->SourcePoint.x;

          dst_rect.left = move_rect->DestinationRect.top;
          dst_rect.top = width - move_rect->DestinationRect.right;
          dst_rect.right = move_rect->DestinationRect.bottom;
          dst_rect.bottom =  width - move_rect->DestinationRect.left;
          break;
        default:
          continue;
      }

      MoveRectData rect_data = { };
      rect_data.src_rect = src_rect;
      rect_data.dst_rect = dst_rect;
      rect_data.box.left = src_rect.left;
      rect_data.box.top = src_rect.top;
      rect_data.box.right = src_rect.right;
      rect_data.box.bottom = src_rect.bottom;
      rect_data.box.front = 0;
      rect_data.box.back = 1;

      move_rect_.push_back (rect_data);
    }
  }

  void buildDirtyVertex (RECT * rects, UINT num_rect)
  {
    if (num_rect == 0)
      return;

    dirty_vertex_.resize (num_rect * 6);
    FLOAT width = output_desc_.ModeDesc.Width;
    FLOAT height = output_desc_.ModeDesc.Height;
    FLOAT center_x = width / 2.0f;
    FLOAT center_y = height / 2.0f;

    for (UINT i = 0; i < num_rect; i++) {
      RECT dirty = rects[i];
      RECT dest_dirty = dirty;
      UINT base = i * 6;

      dirty_rect_.push_back (dirty);

      switch (output_desc_.Rotation) {
        case DXGI_MODE_ROTATION_ROTATE90:
          dest_dirty.left = width - dirty.bottom;
          dest_dirty.top = dirty.left;
          dest_dirty.right = width - dirty.top;
          dest_dirty.bottom = dirty.right;

          dirty_vertex_[base].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.bottom / height);
          dirty_vertex_[base + 1].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.bottom / height);
          dirty_vertex_[base + 2].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.top / height);
          dirty_vertex_[base + 5].TexCoord = XMFLOAT2(
              dirty.left / width, dirty.top / height);
          break;
        case DXGI_MODE_ROTATION_ROTATE180:
          dest_dirty.left = width - dirty.right;
          dest_dirty.top = height - dirty.bottom;
          dest_dirty.right = width - dirty.left;
          dest_dirty.bottom = height - dirty.top;

          dirty_vertex_[base].TexCoord = XMFLOAT2(
              dirty.right / width, dirty.top / height);
          dirty_vertex_[base + 1].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.bottom / height);
          dirty_vertex_[base + 2].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.top / height);
          dirty_vertex_[base + 5].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.bottom / height);
          break;
        case DXGI_MODE_ROTATION_ROTATE270:
          dest_dirty.left = dirty.top;
          dest_dirty.top = height - dirty.right;
          dest_dirty.right = dirty.bottom;
          dest_dirty.bottom = height - dirty.left;

          dirty_vertex_[base].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.top / height);
          dirty_vertex_[base + 1].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.top / height);
          dirty_vertex_[base + 2].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.bottom / height);
          dirty_vertex_[base + 5].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.bottom / height);
          break;
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        case DXGI_MODE_ROTATION_IDENTITY:
        default:
          dirty_vertex_[base].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.bottom / height);
          dirty_vertex_[base + 1].TexCoord = XMFLOAT2 (
              dirty.left / width, dirty.top / height);
          dirty_vertex_[base + 2].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.bottom / height);
          dirty_vertex_[base + 5].TexCoord = XMFLOAT2 (
              dirty.right / width, dirty.top / height);
          break;
      }

      /* Set positions */
      dirty_vertex_[base].Pos = XMFLOAT3 (
          (dest_dirty.left - center_x) / center_x,
          -1 * (dest_dirty.bottom - center_y) / center_y, 0.0f);
      dirty_vertex_[base + 1].Pos = XMFLOAT3 (
          (dest_dirty.left - center_x) / center_x,
          -1 * (dest_dirty.top - center_y) / center_y, 0.0f);
      dirty_vertex_[base + 2].Pos = XMFLOAT3 (
          (dest_dirty.right - center_x) / center_x,
          -1 * (dest_dirty.bottom - center_y) / center_y, 0.0f);
      dirty_vertex_[base + 3].Pos = dirty_vertex_[base + 2].Pos;
      dirty_vertex_[base + 4].Pos = dirty_vertex_[base + 1].Pos;
      dirty_vertex_[base + 5].Pos = XMFLOAT3 (
          (dest_dirty.right - center_x) / center_x,
          -1 * (dest_dirty.top - center_y) / center_y, 0.0f);

      dirty_vertex_[base + 3].TexCoord = dirty_vertex_[base + 2].TexCoord;
      dirty_vertex_[base + 4].TexCoord = dirty_vertex_[base + 1].TexCoord;
    }
  }

private:
  PtrInfo ptr_info_;
  DXGI_OUTDUPL_DESC output_desc_;
  DXGI_OUTDUPL_FRAME_INFO frame_info_;
  ComPtr<IDXGIOutputDuplication> dupl_;
  ComPtr<ID3D11Device> device_;
  ComPtr<IDXGIResource> acquired_frame_;
  std::vector<MoveRectData> move_rect_;
  std::vector<VERTEX> dirty_vertex_;
  std::vector<RECT> dirty_rect_;

  /* frame metadata */
  std::vector<BYTE> metadata_buffer_;
};

struct GstD3D12DxgiCapturePrivate
{
  GstD3D12DxgiCapturePrivate ()
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    fence_data_pool = gst_d3d12_fence_data_pool_new ();
  }

  ~GstD3D12DxgiCapturePrivate ()
  {
    if (device) {
      auto fence_to_wait = MAX (fence_val, mouse_fence_val);
      gst_d3d12_device_fence_wait (device, D3D12_COMMAND_LIST_TYPE_DIRECT,
          fence_to_wait, event_handle);
    }
    CloseHandle (event_handle);
    gst_clear_buffer (&mouse_buf);
    gst_clear_buffer (&mouse_xor_buf);
    gst_clear_object (&ca_pool);
    gst_clear_object (&fence_data_pool);
    gst_clear_object (&mouse_blend);
    gst_clear_object (&mouse_xor_blend);
    gst_clear_object (&device);
  }

  GstD3D12Device *device = nullptr;

  std::unique_ptr<DesktopDupCtx> ctx;
  ComPtr<IDXGIOutput1> output;
  ComPtr<IDXGIResource> last_resource;
  ComPtr<ID3D12Resource> shared_resource;
  ComPtr<ID3D12Resource> move_frame;
  ComPtr<ID3D12Resource> processed_frame;
  ComPtr<ID3D12RootSignature> rs;
  ComPtr<ID3D12PipelineState> pso;
  GstD3D12CommandAllocatorPool *ca_pool = nullptr;
  GstD3D12FenceDataPool *fence_data_pool;
  GstD3D12FenceData *mouse_fence_data = nullptr;
  ComPtr<ID3D12GraphicsCommandList> cl;
  ComPtr<ID3D12GraphicsCommandList> mouse_cl;
  ComPtr<ID3D12DescriptorHeap> srv_heap;
  ComPtr<ID3D12DescriptorHeap> rtv_heap;
  ComPtr<ID3D12Resource> dirty_vertex_buf;
  UINT dirty_vertex_size = 0;
  GstBuffer *mouse_buf = nullptr;
  GstBuffer *mouse_xor_buf = nullptr;
  D3D12_VIEWPORT viewport;
  D3D12_RECT scissor_rect;
  D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;

  GstD3D12Converter *mouse_blend = nullptr;
  GstD3D12Converter *mouse_xor_blend = nullptr;

  HMONITOR monitor_handle = nullptr;
  RECT desktop_coordinates = { };

  guint cached_width = 0;
  guint cached_height = 0;

  HANDLE event_handle;
  guint64 fence_val = 0;
  guint64 mouse_fence_val = 0;

  guint64 mouse_token = 0;

  std::mutex lock;
};
/* *INDENT-ON* */

struct _GstD3D12DxgiCapture
{
  GstD3D12ScreenCapture parent;

  GstD3D12Device *device;

  GstD3D12DxgiCapturePrivate *priv;
};

static void gst_d3d12_dxgi_capture_dispose (GObject * object);
static void gst_d3d12_dxgi_capture_finalize (GObject * object);
static void gst_d3d12_dxgi_capture_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static GstFlowReturn
gst_d3d12_dxgi_capture_prepare (GstD3D12ScreenCapture * capture);
static gboolean
gst_d3d12_dxgi_capture_get_size (GstD3D12ScreenCapture * capture,
    guint * width, guint * height);
static GstFlowReturn
gst_d3d12_dxgi_capture_do_capture (GstD3D12ScreenCapture * capture,
    GstBuffer * buffer, const D3D12_BOX * crop_box, gboolean draw_mouse);

#define gst_d3d12_dxgi_capture_parent_class parent_class
G_DEFINE_TYPE (GstD3D12DxgiCapture, gst_d3d12_dxgi_capture,
    GST_TYPE_D3D12_SCREEN_CAPTURE);

static void
gst_d3d12_dxgi_capture_class_init (GstD3D12DxgiCaptureClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto capture_class = GST_D3D12_SCREEN_CAPTURE_CLASS (klass);

  object_class->finalize = gst_d3d12_dxgi_capture_finalize;

  capture_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d12_dxgi_capture_prepare);
  capture_class->get_size = GST_DEBUG_FUNCPTR (gst_d3d12_dxgi_capture_get_size);
  capture_class->do_capture =
      GST_DEBUG_FUNCPTR (gst_d3d12_dxgi_capture_do_capture);
}

static void
gst_d3d12_dxgi_capture_init (GstD3D12DxgiCapture * self)
{
  self->priv = new GstD3D12DxgiCapturePrivate ();
}

static void
gst_d3d12_dxgi_capture_finalize (GObject * object)
{
  auto self = GST_D3D12_DXGI_CAPTURE (object);

  delete self->priv;
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_dxgi_capture_weak_ref_notify (gpointer data,
    GstD3D12DxgiCapture * dupl)
{
  std::lock_guard < std::mutex > lk (g_g_dupl_list_lock);
  g_dupl_list = g_list_remove (g_dupl_list, dupl);
}

static gboolean
gst_d3d12_dxgi_capture_open (GstD3D12DxgiCapture * self,
    HMONITOR monitor_handle)
{
  const D3D12_ROOT_SIGNATURE_FLAGS rs_flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
  const D3D12_STATIC_SAMPLER_DESC static_sampler_desc = {
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    0,
    1,
    D3D12_COMPARISON_FUNC_ALWAYS,
    D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
    0,
    D3D12_FLOAT32_MAX,
    0,
    0,
    D3D12_SHADER_VISIBILITY_PIXEL
  };

  auto priv = self->priv;
  priv->monitor_handle = monitor_handle;

  ComPtr < IDXGIOutput > output;
  auto hr = gst_d3d12_screen_capture_find_output_for_monitor (monitor_handle,
      nullptr, &output);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_WARNING_OBJECT (self,
        "Failed to find associated adapter for monitor %p", monitor_handle);
    return FALSE;
  }

  hr = output.As (&priv->output);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_WARNING_OBJECT (self, "IDXGIOutput1 interface unavailable");
    return FALSE;
  }

  DXGI_OUTPUT_DESC output_desc;
  hr = output->GetDesc (&output_desc);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_WARNING_OBJECT (self, "Couldn't get output desc");
    return FALSE;
  }

  /* DesktopCoordinates will not report actual texture size in case that
   * application is running without dpi-awareness. To get actual monitor size,
   * we need to use Win32 API... */
  MONITORINFOEXW monitor_info;
  DEVMODEW dev_mode;

  monitor_info.cbSize = sizeof (MONITORINFOEXW);
  if (!GetMonitorInfoW (output_desc.Monitor, (LPMONITORINFO) & monitor_info)) {
    GST_WARNING_OBJECT (self, "Couldn't get monitor info");
    return FALSE;
  }

  dev_mode.dmSize = sizeof (DEVMODEW);
  dev_mode.dmDriverExtra = sizeof (POINTL);
  dev_mode.dmFields = DM_POSITION;
  if (!EnumDisplaySettingsW
      (monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode)) {
    GST_WARNING_OBJECT (self, "Couldn't enumerate display settings");
    return FALSE;
  }

  priv->desktop_coordinates.left = dev_mode.dmPosition.x;
  priv->desktop_coordinates.top = dev_mode.dmPosition.y;
  priv->desktop_coordinates.right =
      dev_mode.dmPosition.x + dev_mode.dmPelsWidth;
  priv->desktop_coordinates.bottom =
      dev_mode.dmPosition.y + dev_mode.dmPelsHeight;

  priv->cached_width =
      priv->desktop_coordinates.right - priv->desktop_coordinates.left;
  priv->cached_height =
      priv->desktop_coordinates.bottom - priv->desktop_coordinates.top;

  GST_DEBUG_OBJECT (self,
      "Desktop coordinates left:top:right:bottom = %ld:%ld:%ld:%ld (%dx%d)",
      priv->desktop_coordinates.left, priv->desktop_coordinates.top,
      priv->desktop_coordinates.right, priv->desktop_coordinates.bottom,
      priv->cached_width, priv->cached_height);

  auto device = gst_d3d12_device_get_device_handle (self->device);

  CD3DX12_ROOT_PARAMETER param;
  D3D12_DESCRIPTOR_RANGE range;
  std::vector < D3D12_ROOT_PARAMETER > param_list;

  range = CD3DX12_DESCRIPTOR_RANGE (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  param.InitAsDescriptorTable (1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
  param_list.push_back (param);

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC rs_desc = { };
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC::Init_1_0 (rs_desc,
      param_list.size (), param_list.data (),
      1, &static_sampler_desc, rs_flags);
  ComPtr < ID3DBlob > rs_blob;
  ComPtr < ID3DBlob > error_blob;
  hr = D3DX12SerializeVersionedRootSignature (&rs_desc,
      D3D_ROOT_SIGNATURE_VERSION_1_1, &rs_blob, &error_blob);
  if (!gst_d3d12_result (hr, self->device)) {
    const gchar *error_msg = nullptr;
    if (error_blob)
      error_msg = (const gchar *) error_blob->GetBufferPointer ();

    GST_ERROR_OBJECT (self, "Couldn't serialize root signature, error: %s",
        GST_STR_NULL (error_msg));
    return FALSE;
  }

  hr = device->CreateRootSignature (0, rs_blob->GetBufferPointer (),
      rs_blob->GetBufferSize (), IID_PPV_ARGS (&priv->rs));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create root signature");
    return FALSE;
  }

  D3D12_INPUT_ELEMENT_DESC input_desc[2];
  input_desc[0].SemanticName = "POSITION";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { };
  pso_desc.pRootSignature = priv->rs.Get ();
  pso_desc.VS.BytecodeLength = sizeof (g_VSMain_coord);
  pso_desc.VS.pShaderBytecode = g_VSMain_coord;
  pso_desc.PS.BytecodeLength = sizeof (g_PSMain_sample);
  pso_desc.PS.pShaderBytecode = g_PSMain_sample;
  pso_desc.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
  pso_desc.SampleMask = UINT_MAX;
  pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.DepthStencilState.DepthEnable = FALSE;
  pso_desc.DepthStencilState.StencilEnable = FALSE;
  pso_desc.InputLayout.pInputElementDescs = input_desc;
  pso_desc.InputLayout.NumElements = G_N_ELEMENTS (input_desc);
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
  pso_desc.SampleDesc.Count = 1;

  hr = device->CreateGraphicsPipelineState (&pso_desc,
      IID_PPV_ARGS (&priv->pso));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create pso");
    return FALSE;
  }

  /* size will be updated later */
  GstVideoInfo info;
  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_BGRA,
      priv->cached_width, priv->cached_height);
  D3D12_BLEND_DESC blend_desc = CD3DX12_BLEND_DESC (D3D12_DEFAULT);

  blend_desc.RenderTarget[0].BlendEnable = TRUE;
  blend_desc.RenderTarget[0].LogicOpEnable = FALSE;
  blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  blend_desc.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;

  priv->mouse_blend = gst_d3d12_converter_new (self->device, &info, &info,
      &blend_desc, nullptr, nullptr);

  blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
  blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR;
  priv->mouse_xor_blend = gst_d3d12_converter_new (self->device, &info, &info,
      &blend_desc, nullptr, nullptr);

  D3D12_DESCRIPTOR_HEAP_DESC heap_desc = { };
  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap_desc.NumDescriptors = 1;
  heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = device->CreateDescriptorHeap (&heap_desc,
      IID_PPV_ARGS (&priv->srv_heap));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create descriptor heap");
    return FALSE;
  }

  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = device->CreateDescriptorHeap (&heap_desc,
      IID_PPV_ARGS (&priv->rtv_heap));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create descriptor heap");
    return FALSE;
  }

  priv->ca_pool = gst_d3d12_command_allocator_pool_new (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);

  priv->device = (GstD3D12Device *) gst_object_ref (self->device);

  return TRUE;
}

GstD3D12ScreenCapture *
gst_d3d12_dxgi_capture_new (GstD3D12Device * device, HMONITOR monitor_handle)
{
  GList *iter;

  g_return_val_if_fail (GST_IS_D3D12_DEVICE (device), nullptr);

  /* Check if we have dup object corresponding to monitor_handle,
   * and if there is already configured capture object, reuse it.
   * This is because of the limitation of desktop duplication API
   * (i.e., in a process, only one duplication object can exist).
   * See also
   * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput#remarks
   */
  std::lock_guard < std::mutex > lk (g_g_dupl_list_lock);
  for (iter = g_dupl_list; iter; iter = g_list_next (iter)) {
    auto capture = (GstD3D12DxgiCapture *) iter->data;
    auto priv = capture->priv;

    if (priv->monitor_handle == monitor_handle) {
      GST_DEBUG ("Found configured desktop dup object for monitor handle %p",
          monitor_handle);
      gst_object_ref (capture);
      return GST_D3D12_SCREEN_CAPTURE_CAST (capture);
    }
  }

  auto self = (GstD3D12DxgiCapture *) g_object_new (GST_TYPE_D3D12_DXGI_CAPTURE,
      nullptr);
  self->device = (GstD3D12Device *) gst_object_ref (device);
  gst_object_ref_sink (self);

  if (!gst_d3d12_dxgi_capture_open (self, monitor_handle)) {
    gst_object_unref (self);
    return nullptr;
  }

  g_object_weak_ref (G_OBJECT (self),
      (GWeakNotify) gst_d3d12_dxgi_capture_weak_ref_notify, nullptr);
  g_dupl_list = g_list_append (g_dupl_list, self);

  return GST_D3D12_SCREEN_CAPTURE_CAST (self);
}

static GstFlowReturn
gst_d3d12_dxgi_capture_prepare_unlocked (GstD3D12DxgiCapture * self)
{
  auto priv = self->priv;

  if (priv->ctx) {
    GST_DEBUG_OBJECT (self, "Already prepared");
    return GST_FLOW_OK;
  }

  auto ctx = std::make_unique < DesktopDupCtx > ();
  auto ret = ctx->Init (priv->monitor_handle);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self,
        "Couldn't prepare capturing, %sexpected failure",
        ret == GST_D3D12_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR ? "" : "un");

    return ret;
  }

  ctx->GetSize (&priv->cached_width, &priv->cached_height);
  priv->viewport.TopLeftX = 0;
  priv->viewport.TopLeftY = 0;
  priv->viewport.Width = priv->cached_width;
  priv->viewport.Height = priv->cached_height;
  priv->viewport.MinDepth = 0;
  priv->viewport.MaxDepth = 1;

  priv->scissor_rect.left = 0;
  priv->scissor_rect.top = 0;
  priv->scissor_rect.right = priv->cached_width;
  priv->scissor_rect.bottom = priv->cached_height;

  ComPtr < ID3D12Resource > processed_frame;
  D3D12_CLEAR_VALUE clear_value = { };
  clear_value.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  clear_value.Color[0] = 0.0f;
  clear_value.Color[1] = 0.0f;
  clear_value.Color[2] = 0.0f;
  clear_value.Color[3] = 1.0f;

  D3D12_HEAP_PROPERTIES heap_prop =
      CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC resource_desc =
      CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_B8G8R8A8_UNORM,
      priv->cached_width, priv->cached_height, 1, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
      D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);

  auto device = gst_d3d12_device_get_device_handle (self->device);
  auto hr = device->CreateCommittedResource (&heap_prop, D3D12_HEAP_FLAG_NONE,
      &resource_desc, D3D12_RESOURCE_STATE_COMMON, &clear_value,
      IID_PPV_ARGS (&processed_frame));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create texture");
    return GST_FLOW_ERROR;
  }

  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = { };
  rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  rtv_desc.Texture2D.PlaneSlice = 0;

  device->CreateRenderTargetView (processed_frame.Get (), &rtv_desc,
      priv->rtv_heap->GetCPUDescriptorHandleForHeapStart ());

  priv->ctx = std::move (ctx);
  priv->processed_frame = processed_frame;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_d3d12_dxgi_capture_prepare (GstD3D12ScreenCapture * capture)
{
  auto self = GST_D3D12_DXGI_CAPTURE (capture);
  auto priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);
  return gst_d3d12_dxgi_capture_prepare_unlocked (self);
}

static gboolean
gst_d3d12_dxgi_capture_get_size_unlocked (GstD3D12DxgiCapture * self,
    guint * width, guint * height)
{
  auto priv = self->priv;

  *width = 0;
  *height = 0;

  if (priv->ctx)
    priv->ctx->GetSize (&priv->cached_width, &priv->cached_height);

  *width = priv->cached_width;
  *height = priv->cached_height;

  return TRUE;
}

static gboolean
gst_d3d12_dxgi_capture_get_size (GstD3D12ScreenCapture * capture,
    guint * width, guint * height)
{
  auto self = GST_D3D12_DXGI_CAPTURE (capture);
  auto priv = self->priv;

  std::lock_guard < std::mutex > lk (priv->lock);

  return gst_d3d12_dxgi_capture_get_size_unlocked (self, width, height);
}

static gboolean
gst_d3d12_dxgi_capture_copy_move_rects (GstD3D12DxgiCapture * self,
    ID3D12GraphicsCommandList * cl)
{
  auto priv = self->priv;
  HRESULT hr;

  auto device = gst_d3d12_device_get_device_handle (self->device);

  GST_LOG_OBJECT (self, "Rendering move rects");

  std::vector < D3D12_RESOURCE_BARRIER > barriers;
  if (!priv->move_frame) {
    D3D12_HEAP_PROPERTIES heap_prop =
        CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resource_desc = priv->processed_frame->GetDesc ();
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource (&heap_prop,
        D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS (&priv->move_frame));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create move texture");
      return FALSE;
    }
  }

  D3D12_TEXTURE_COPY_LOCATION move_frame =
      CD3DX12_TEXTURE_COPY_LOCATION (priv->move_frame.Get ());
  D3D12_TEXTURE_COPY_LOCATION processed_frame =
      CD3DX12_TEXTURE_COPY_LOCATION (priv->processed_frame.Get ());

  const auto & data = priv->ctx->GetMoveRects ();
  for (size_t i = 0; i < data.size (); i++) {
    const auto & rect = data[i];
    cl->CopyTextureRegion (&move_frame, rect.src_rect.left, rect.src_rect.top,
        0, &processed_frame, &rect.box);
  }

  priv->resource_state = D3D12_RESOURCE_STATE_COPY_DEST;

  barriers.clear ();
  barriers.
      push_back (CD3DX12_RESOURCE_BARRIER::Transition (priv->processed_frame.
          Get (), D3D12_RESOURCE_STATE_COPY_SOURCE,
          D3D12_RESOURCE_STATE_COPY_DEST));
  barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (priv->
          move_frame.Get (), D3D12_RESOURCE_STATE_COPY_DEST,
          D3D12_RESOURCE_STATE_COPY_SOURCE));
  cl->ResourceBarrier (barriers.size (), barriers.data ());
  for (size_t i = 0; i < data.size (); i++) {
    const auto & rect = data[i];
    cl->CopyTextureRegion (&processed_frame, rect.dst_rect.left,
        rect.dst_rect.top, 0, &move_frame, &rect.box);
  }

  barriers.clear ();
  barriers.push_back (CD3DX12_RESOURCE_BARRIER::Transition (priv->
          move_frame.Get (), D3D12_RESOURCE_STATE_COPY_SOURCE,
          D3D12_RESOURCE_STATE_COPY_DEST));
  cl->ResourceBarrier (barriers.size (), barriers.data ());

  return TRUE;
}

static gboolean
gst_d3d12_dxgi_capture_copy_dirty_rects (GstD3D12DxgiCapture * self,
    IDXGIResource * resource, ID3D12GraphicsCommandList * cl)
{
  auto priv = self->priv;
  HRESULT hr;

  auto device = gst_d3d12_device_get_device_handle (self->device);

  GST_LOG_OBJECT (self, "Rendering dirty rects");

  if (!priv->shared_resource) {
    ComPtr < IDXGIResource1 > resource1;
    hr = resource->QueryInterface (IID_PPV_ARGS (&resource1));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "IDXGIResource1 interface unavailable");
      return FALSE;
    }

    HANDLE shared_handle;
    hr = resource1->CreateSharedHandle (nullptr, DXGI_SHARED_RESOURCE_READ,
        nullptr, &shared_handle);
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create shared handle");
      return FALSE;
    }

    hr = device->OpenSharedHandle (shared_handle,
        IID_PPV_ARGS (&priv->shared_resource));
    CloseHandle (shared_handle);
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't open shared resource");
      return FALSE;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srv_desc.Texture2D.PlaneSlice = 0;
    srv_desc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView (priv->shared_resource.Get (), &srv_desc,
        priv->srv_heap->GetCPUDescriptorHandleForHeapStart ());
  }

  auto desc = priv->ctx->GetDesc ();
  if (desc.Rotation == DXGI_MODE_ROTATION_UNSPECIFIED ||
      desc.Rotation == DXGI_MODE_ROTATION_IDENTITY) {
    const auto & rects = priv->ctx->GetDirtyRects ();
    D3D12_TEXTURE_COPY_LOCATION src =
        CD3DX12_TEXTURE_COPY_LOCATION (priv->shared_resource.Get ());
    D3D12_TEXTURE_COPY_LOCATION dst =
        CD3DX12_TEXTURE_COPY_LOCATION (priv->processed_frame.Get ());
    D3D12_BOX box;
    box.front = 0;
    box.back = 1;

    GST_LOG_OBJECT (self, "Perform copy");

    for (size_t i = 0; i < rects.size (); i++) {
      const auto & rect = rects[i];
      box.left = rect.left;
      box.right = rect.right;
      box.top = rect.top;
      box.bottom = rect.bottom;

      cl->CopyTextureRegion (&dst, box.left, box.top, 0, &src, &box);
    }

    if (priv->resource_state == D3D12_RESOURCE_STATE_COMMON)
      priv->resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
  } else {
    GST_LOG_OBJECT (self, "Perform draw");

    if (priv->resource_state != D3D12_RESOURCE_STATE_COMMON) {
      D3D12_RESOURCE_BARRIER barrier =
          CD3DX12_RESOURCE_BARRIER::Transition (priv->processed_frame.Get (),
          priv->resource_state, D3D12_RESOURCE_STATE_RENDER_TARGET);
      priv->cl->ResourceBarrier (1, &barrier);
    }

    cl->SetGraphicsRootSignature (priv->rs.Get ());
    cl->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->RSSetViewports (1, &priv->viewport);
    cl->RSSetScissorRects (1, &priv->scissor_rect);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_heaps[] = {
      priv->rtv_heap->GetCPUDescriptorHandleForHeapStart ()
    };

    cl->OMSetRenderTargets (1, rtv_heaps, FALSE, nullptr);

    const auto & vertex = priv->ctx->GetDirtyVertex ();
    UINT buf_size = vertex.size () * sizeof (VERTEX);
    if (priv->dirty_vertex_size < buf_size)
      priv->dirty_vertex_buf = nullptr;

    if (!priv->dirty_vertex_buf) {
      D3D12_HEAP_PROPERTIES heap_prop =
          CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD);
      D3D12_RESOURCE_DESC buffer_desc =
          CD3DX12_RESOURCE_DESC::Buffer (buf_size);
      hr = device->CreateCommittedResource (&heap_prop,
          D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &buffer_desc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS (&priv->dirty_vertex_buf));
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create vertex buffer");
        return FALSE;
      }

      priv->dirty_vertex_size = buf_size;
    }

    CD3DX12_RANGE range (0, 0);
    void *data;
    hr = priv->dirty_vertex_buf->Map (0, &range, &data);
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't map buffer");
      return FALSE;
    }

    memcpy (data, vertex.data (), buf_size);
    priv->dirty_vertex_buf->Unmap (0, nullptr);
    D3D12_VERTEX_BUFFER_VIEW vbv = { };
    vbv.BufferLocation = priv->dirty_vertex_buf->GetGPUVirtualAddress ();
    vbv.SizeInBytes = buf_size;
    vbv.StrideInBytes = sizeof (VERTEX);

    ID3D12DescriptorHeap *heaps[] = { priv->srv_heap.Get () };
    cl->SetDescriptorHeaps (1, heaps);
    cl->SetGraphicsRootDescriptorTable (0,
        priv->srv_heap->GetGPUDescriptorHandleForHeapStart ());
    cl->IASetVertexBuffers (0, 1, &vbv);
    cl->DrawInstanced (vertex.size (), 1, 0, 0);

    if (priv->resource_state == D3D12_RESOURCE_STATE_COMMON)
      priv->resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
  }

  return TRUE;
}

static gboolean
gst_d3d12_dxgi_capture_draw_mouse (GstD3D12DxgiCapture * self,
    GstBuffer * buffer, const D3D12_BOX * crop_box)
{
  auto priv = self->priv;
  const auto & info = priv->ctx->GetPointerInfo ();
  HRESULT hr;

  if (!info.position_info.Visible)
    return TRUE;

  if (!info.width_ || !info.height_)
    return TRUE;

  if (info.position_info.Position.x + info.width_ < crop_box->left ||
      info.position_info.Position.x > crop_box->right ||
      info.position_info.Position.y + info.height_ < crop_box->top ||
      info.position_info.Position.y > crop_box->bottom) {
    return TRUE;
  }

  if (info.token_ != priv->mouse_token) {
    gst_clear_buffer (&priv->mouse_buf);
    gst_clear_buffer (&priv->mouse_xor_buf);
    priv->mouse_token = info.token_;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  if (!priv->mouse_buf) {
    ComPtr < ID3D12Resource > mouse_texture;
    ComPtr < ID3D12Resource > mouse_xor_texture;

    D3D12_HEAP_PROPERTIES heap_prop =
        CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resource_desc =
        CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_B8G8R8A8_UNORM,
        info.width_, info.height_, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);

    hr = device->CreateCommittedResource (&heap_prop,
        D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS (&mouse_texture));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create mouse texture");
      return FALSE;
    }

    if (!info.xor_texture_.empty ()) {
      hr = device->CreateCommittedResource (&heap_prop,
          D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COMMON,
          nullptr, IID_PPV_ARGS (&mouse_xor_texture));
      if (!gst_d3d12_result (hr, self->device)) {
        GST_ERROR_OBJECT (self, "Couldn't create mouse texture");
        return FALSE;
      }
    }

    priv->mouse_buf = gst_buffer_new ();
    auto mem = gst_d3d12_allocator_alloc_wrapped (nullptr, self->device,
        mouse_texture.Get (), 0);
    gst_buffer_append_memory (priv->mouse_buf, mem);

    if (mouse_xor_texture) {
      priv->mouse_xor_buf = gst_buffer_new ();
      auto mem = gst_d3d12_allocator_alloc_wrapped (nullptr, self->device,
          mouse_xor_texture.Get (), 0);
      gst_buffer_append_memory (priv->mouse_xor_buf, mem);
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT64 buffer_size;

    device->GetCopyableFootprints (&resource_desc,
        0, 1, 0, &layout, nullptr, nullptr, &buffer_size);

    GstMapInfo map_info;
    gst_buffer_map (priv->mouse_buf, &map_info, GST_MAP_WRITE);
    auto src = info.texture_.data ();
    auto dst = (guint8 *) map_info.data;
    for (UINT i = 0; i < info.height_; i++) {
      memcpy (dst, src, info.width_ * 4);
      src += info.stride_;
      dst += layout.Footprint.RowPitch;
    }
    gst_buffer_unmap (priv->mouse_buf, &map_info);

    if (priv->mouse_xor_buf) {
      gst_buffer_map (priv->mouse_xor_buf, &map_info, GST_MAP_WRITE);
      auto src = info.xor_texture_.data ();
      auto dst = (guint8 *) map_info.data;
      for (UINT i = 0; i < info.height_; i++) {
        memcpy (dst, src, info.width_ * 4);
        src += info.stride_;
        dst += layout.Footprint.RowPitch;
      }
      gst_buffer_unmap (priv->mouse_xor_buf, &map_info);
    }
  }

  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool,
      &priv->mouse_fence_data);
  auto fence_data = priv->mouse_fence_data;

  GstD3D12CommandAllocator *gst_ca = nullptr;
  ComPtr < ID3D12CommandAllocator > ca;

  if (!gst_d3d12_command_allocator_pool_acquire (priv->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    return FALSE;
  }

  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);
  hr = ca->Reset ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command allocator");
    return FALSE;
  }

  if (!priv->mouse_cl) {
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), nullptr, IID_PPV_ARGS (&priv->mouse_cl));
  } else {
    hr = priv->mouse_cl->Reset (ca.Get (), nullptr);
  }

  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command list");
    return FALSE;
  }

  auto cl = priv->mouse_cl;
  gint ptr_x = info.position_info.Position.x - crop_box->left;
  gint ptr_y = info.position_info.Position.y - crop_box->top;
  gint ptr_w = info.width_;
  gint ptr_h = info.height_;

  g_object_set (priv->mouse_blend, "src-x", 0, "src-y", 0, "src-width",
      ptr_w, "src-height", ptr_h, "dest-x", ptr_x, "dest-y", ptr_y,
      "dest-width", ptr_w, "dest-height", ptr_h, nullptr);

  if (!gst_d3d12_converter_convert_buffer (priv->mouse_blend,
          priv->mouse_buf, buffer, fence_data, cl.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't build mouse blend command");
    return FALSE;
  }

  if (priv->mouse_xor_buf) {
    g_object_set (priv->mouse_xor_blend, "src-x", 0, "src-y", 0, "src-width",
        ptr_w, "src-height", ptr_h, "dest-x", ptr_x, "dest-y", ptr_y,
        "dest-width", ptr_w, "dest-height", ptr_h, nullptr);

    if (!gst_d3d12_converter_convert_buffer (priv->mouse_xor_blend,
            priv->mouse_xor_buf, buffer, fence_data, cl.Get ())) {
      GST_ERROR_OBJECT (self, "Couldn't build mouse blend command");
      return FALSE;
    }
  }

  hr = cl->Close ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_d3d12_dxgi_capture_do_capture (GstD3D12ScreenCapture * capture,
    GstBuffer * buffer, const D3D12_BOX * crop_box, gboolean draw_mouse)
{
  auto self = GST_D3D12_DXGI_CAPTURE (capture);
  auto priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  guint width, height;
  GstD3D12Memory *dmem;
  ID3D12Resource *out_resource;
  D3D12_TEXTURE_COPY_LOCATION src, dst;
  ID3D12CommandList *cmd_list[1];
  GstD3D12FenceData *fence_data = nullptr;
  GstD3D12CommandAllocator *gst_ca = nullptr;
  ComPtr < ID3D12CommandAllocator > ca;
  HRESULT hr;

  std::lock_guard < std::mutex > lk (priv->lock);
  if (!priv->ctx) {
    ret = gst_d3d12_dxgi_capture_prepare_unlocked (self);
    if (ret != GST_FLOW_OK) {
      GST_WARNING_OBJECT (self, "We are not prepared");
      return ret;
    }
  }

  gst_d3d12_dxgi_capture_get_size_unlocked (self, &width, &height);
  if (crop_box->left > width || crop_box->right > width ||
      crop_box->top > height || crop_box->bottom > height) {
    GST_INFO_OBJECT (self,
        "Capture area (%u, %u, %u, %u) doesn't fit into screen size %ux%u",
        crop_box->left, crop_box->right, crop_box->top,
        crop_box->bottom, width, height);

    return GST_D3D12_SCREEN_CAPTURE_FLOW_SIZE_CHANGED;
  }

  ComPtr < IDXGIResource > resource;
  ret = priv->ctx->AcquireNextFrame (&resource);
  if (ret != GST_FLOW_OK) {
    priv->ctx = nullptr;
    priv->processed_frame = nullptr;
    priv->move_frame = nullptr;
    if (ret == GST_D3D12_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR) {
      GST_WARNING_OBJECT (self, "Couldn't capture frame, but expected failure");
    } else {
      GST_ERROR_OBJECT (self, "Unexpected failure during capture");
    }

    return ret;
  }

  if (resource) {
    if (resource != priv->last_resource)
      priv->shared_resource = nullptr;

    priv->last_resource = resource;
  }

  GST_LOG_OBJECT (self, "Capture done");

  bool have_move_rect = false;
  if (priv->ctx->GetMoveCount () > 0)
    have_move_rect = true;
  bool have_dirty_rect = false;
  if ((priv->ctx->GetDirtyCount () > 0) && resource)
    have_dirty_rect = true;

  std::future < gboolean > mouse_blend_ret;
  if (draw_mouse) {
    /* Build mouse draw command from other thread */
    mouse_blend_ret = std::async (std::launch::async,
        gst_d3d12_dxgi_capture_draw_mouse, self, buffer, crop_box);
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  if (!gst_d3d12_command_allocator_pool_acquire (priv->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    goto error;
  }

  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);

  hr = ca->Reset ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command allocator");
    goto error;
  }

  if (!priv->cl) {
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        ca.Get (), priv->pso.Get (), IID_PPV_ARGS (&priv->cl));
  } else {
    hr = priv->cl->Reset (ca.Get (), priv->pso.Get ());
  }

  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command list");
    goto error;
  }

  priv->resource_state = D3D12_RESOURCE_STATE_COMMON;
  if (have_move_rect &&
      !gst_d3d12_dxgi_capture_copy_move_rects (self, priv->cl.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't copy move rects");
    goto error;
  }

  if (have_dirty_rect &&
      !gst_d3d12_dxgi_capture_copy_dirty_rects (self, resource.Get (),
          priv->cl.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't copy dirty rects");
    goto error;
  }

  dmem = (GstD3D12Memory *) gst_buffer_peek_memory (buffer, 0);
  out_resource = gst_d3d12_memory_get_resource_handle (dmem);

  src = CD3DX12_TEXTURE_COPY_LOCATION (priv->processed_frame.Get ());
  dst = CD3DX12_TEXTURE_COPY_LOCATION (out_resource);

  if (priv->resource_state != D3D12_RESOURCE_STATE_COMMON) {
    D3D12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition (priv->processed_frame.Get (),
        priv->resource_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    priv->cl->ResourceBarrier (1, &barrier);
  }

  priv->cl->CopyTextureRegion (&dst, 0, 0, 0, &src, crop_box);

  hr = priv->cl->Close ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    goto error;
  }

  cmd_list[0] = priv->cl.Get ();

  if (!gst_d3d12_device_execute_command_lists (self->device,
          D3D12_COMMAND_LIST_TYPE_DIRECT, 1, cmd_list, &priv->fence_val)) {
    GST_ERROR_OBJECT (self, "Couldn't execute command list");
    goto error;
  }

  gst_d3d12_device_set_fence_notify (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT, priv->fence_val, fence_data);

  gst_d3d12_buffer_after_write (buffer, priv->fence_val);

  if (draw_mouse) {
    auto blend_ret = mouse_blend_ret.get ();
    if (!blend_ret) {
      GST_ERROR_OBJECT (self, "Couldn't build mouse draw command");
      goto error;
    }

    if (priv->mouse_fence_data && priv->mouse_cl) {
      cmd_list[0] = priv->mouse_cl.Get ();

      if (!gst_d3d12_device_execute_command_lists (self->device,
              D3D12_COMMAND_LIST_TYPE_DIRECT, 1, cmd_list,
              &priv->mouse_fence_val)) {
        GST_ERROR_OBJECT (self, "Couldn't execute command list");
        goto error;
      }

      gst_d3d12_device_set_fence_notify (self->device,
          D3D12_COMMAND_LIST_TYPE_DIRECT, priv->mouse_fence_val,
          priv->mouse_fence_data);
      priv->mouse_fence_data = nullptr;

      gst_d3d12_buffer_after_write (buffer, priv->mouse_fence_val);
    }
  }

  if (have_dirty_rect) {
    gst_d3d12_device_fence_wait (self->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        priv->fence_val, priv->event_handle);
  }

  priv->ctx->ReleaseFrame ();

  return GST_FLOW_OK;

error:
  if (mouse_blend_ret.valid ())
    mouse_blend_ret.get ();

  gst_clear_d3d12_fence_data (&priv->mouse_fence_data);
  gst_clear_d3d12_fence_data (&fence_data);
  gst_clear_buffer (&priv->mouse_buf);
  gst_clear_buffer (&priv->mouse_xor_buf);
  resource = nullptr;
  priv->last_resource = nullptr;
  priv->shared_resource = nullptr;
  priv->processed_frame = nullptr;
  priv->move_frame = nullptr;
  priv->ctx = nullptr;

  return GST_FLOW_ERROR;
}
