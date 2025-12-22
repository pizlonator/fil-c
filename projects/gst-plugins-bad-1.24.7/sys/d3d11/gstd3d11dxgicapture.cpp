/*
 * GStreamer
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

#include "gstd3d11dxgicapture.h"
#include "gstd3d11pluginutils.h"
#include <string.h>
#include <mutex>

#include <wrl.h>

#define _XM_NO_INTRINSICS_
#include <DirectXMath.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3d11_screen_capture_debug);
#define GST_CAT_DEFAULT gst_d3d11_screen_capture_debug

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace DirectX;

/* List of GstD3D11DxgiCapture weakref */
static std::mutex dupl_list_lock;
static GList *dupl_list = nullptr;

/* Below implementation were taken from Microsoft sample
 * https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/DXGIDesktopDuplication
 */
#define NUMVERTICES 6
#define BPP 4

struct VERTEX
{
  XMFLOAT3 Pos;
  XMFLOAT2 TexCoord;
};

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
gst_d3d11_dxgi_capture_return_from_hr (ID3D11Device * device,
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
        return GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR;

      rst++;
    }
  }

  return GST_FLOW_ERROR;
}

class PTR_INFO
{
public:
  PTR_INFO ()
    : PtrShapeBuffer (nullptr)
    , BufferSize (0)
  {
    LastTimeStamp.QuadPart = 0;
  }

  ~PTR_INFO ()
  {
    if (PtrShapeBuffer)
      delete[] PtrShapeBuffer;
  }

  void
  MaybeReallocBuffer (UINT buffer_size)
  {
    if (buffer_size <= BufferSize)
      return;

    if (PtrShapeBuffer)
      delete[] PtrShapeBuffer;

    PtrShapeBuffer = new BYTE[buffer_size];
    BufferSize = buffer_size;
  }

  BYTE* PtrShapeBuffer;
  UINT BufferSize;
  DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info;
  POINT Position;
  bool Visible;
  LARGE_INTEGER LastTimeStamp;
};

class D3D11DesktopDupObject
{
public:
  D3D11DesktopDupObject ()
    : device_(nullptr)
    , metadata_buffer_(nullptr)
    , metadata_buffer_size_(0)
    , vertex_buffer_(nullptr)
    , vertex_buffer_size_(0)
  {
  }

  ~D3D11DesktopDupObject ()
  {
    if (metadata_buffer_)
      delete[] metadata_buffer_;

    if (vertex_buffer_)
      delete[] vertex_buffer_;

    if (keyed_mutex_) {
      keyed_mutex_->ReleaseSync (0);
      keyed_mutex_ = nullptr;
    }

    gst_clear_object (&device_);
  }

  GstFlowReturn
  Init (GstD3D11Device * device, HMONITOR monitor)
  {
    GstFlowReturn ret;
    ID3D11Device *device_handle;
    HRESULT hr;
    D3D11_TEXTURE2D_DESC texture_desc = { 0, };

    if (!InitShader (device))
      return GST_FLOW_ERROR;

    ret = InitDupl (device, monitor);
    if (ret != GST_FLOW_OK)
      return ret;

    GST_INFO ("Init done");

    device_handle = gst_d3d11_device_get_device_handle (device);

    texture_desc.Width = output_desc_.ModeDesc.Width;
    texture_desc.Height = output_desc_.ModeDesc.Height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    /* FIXME: we can support DXGI_FORMAT_R10G10B10A2_UNORM */
    texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    /* source element may hold different d3d11 device object */
    texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    hr = device_handle->CreateTexture2D (&texture_desc,
        nullptr, &shared_texture_);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't create texture, hr 0x%x", (guint) hr);
      return GST_FLOW_ERROR;
    }

    hr = shared_texture_.As (&keyed_mutex_);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (device, "Couldn't get keyed mutex interface");
      shared_texture_ = nullptr;
      return GST_FLOW_ERROR;
    }

    hr = keyed_mutex_->AcquireSync (0, INFINITE);
    if (hr != S_OK) {
      GST_ERROR_OBJECT (device, "Couldn't acquire sync");
      keyed_mutex_ = nullptr;
      shared_texture_ = nullptr;
      return GST_FLOW_ERROR;
    }

    device_ = (GstD3D11Device *) gst_object_ref (device);

    return GST_FLOW_OK;
  }

  GstFlowReturn
  Capture ()
  {
    GstFlowReturn ret;
    bool timeout = false;
    ComPtr<ID3D11Texture2D> texture;
    UINT move_count, dirty_count;
    DXGI_OUTDUPL_FRAME_INFO frame_info;

    GST_TRACE ("Capturing");
    ret = GetFrame (&texture, &move_count, &dirty_count, &frame_info, &timeout);
    if (ret != GST_FLOW_OK)
      return ret;

    /* Nothing updated */
    if (timeout) {
      GST_TRACE ("timeout");
      return GST_FLOW_OK;
    }

    GST_TRACE ("Getting mouse pointer info");
    ret = GetMouse (&ptr_info_, &frame_info);
    if (ret != GST_FLOW_OK) {
      GST_WARNING ("Couldn't get mouse pointer info");
      dupl_->ReleaseFrame ();
      return ret;
    }

    ret = ProcessFrame (texture.Get(), shared_texture_.Get(),
        &output_desc_, move_count, dirty_count, &frame_info);

    if (ret != GST_FLOW_OK) {
      dupl_->ReleaseFrame ();
      GST_WARNING ("Couldn't process frame");
      return ret;
    }

    HRESULT hr = dupl_->ReleaseFrame ();
    if (!gst_d3d11_result (hr, device_)) {
      GST_WARNING ("Couldn't release frame");
      return gst_d3d11_dxgi_capture_return_from_hr (nullptr, hr, FrameInfoExpectedErrors);
    }

    GST_TRACE ("Capture done");

    return GST_FLOW_OK;
  }

  bool
  DrawMouse (GstD3D11Device * device, ID3D11RenderTargetView * rtv,
      ShaderResource * resource, D3D11_BOX * cropBox)
  {
    GST_TRACE ("Drawing mouse");

    if (!ptr_info_.Visible) {
      GST_TRACE ("Mouse is invisiable");
      return true;
    }

    ComPtr<ID3D11Texture2D> MouseTex;
    ComPtr<ID3D11ShaderResourceView> ShaderRes;
    ComPtr<ID3D11Buffer> VertexBufferMouse;
    D3D11_SUBRESOURCE_DATA InitData;
    D3D11_TEXTURE2D_DESC Desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC SDesc;
    ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device);
    ID3D11DeviceContext *context_handle =
        gst_d3d11_device_get_device_context_handle (device);

    VERTEX Vertices[NUMVERTICES] =
    {
      {XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f)},
      {XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
      {XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
      {XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
      {XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
      {XMFLOAT3(1.0f, 1.0f, 0), XMFLOAT2(1.0f, 0.0f)},
    };

    D3D11_TEXTURE2D_DESC FullDesc;
    shared_texture_->GetDesc(&FullDesc);
    INT DesktopWidth = FullDesc.Width;
    INT DesktopHeight = FullDesc.Height;

    INT CenterX = (DesktopWidth / 2);
    INT CenterY = (DesktopHeight / 2);

    INT PtrWidth = 0;
    INT PtrHeight = 0;
    INT PtrLeft = 0;
    INT PtrTop = 0;

    BYTE* InitBuffer = nullptr;

    D3D11_BOX Box;
    Box.front = 0;
    Box.back = 1;

    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    Desc.CPUAccessFlags = 0;
    Desc.MiscFlags = 0;

    // Set shader resource properties
    SDesc.Format = Desc.Format;
    SDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SDesc.Texture2D.MostDetailedMip = Desc.MipLevels - 1;
    SDesc.Texture2D.MipLevels = Desc.MipLevels;

    switch (ptr_info_.shape_info.Type) {
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        PtrLeft = ptr_info_.Position.x;
        PtrTop = ptr_info_.Position.y;

        PtrWidth = static_cast<INT>(ptr_info_.shape_info.Width);
        PtrHeight = static_cast<INT>(ptr_info_.shape_info.Height);
        break;
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        ProcessMonoMask(true, &ptr_info_, &PtrWidth, &PtrHeight, &PtrLeft,
            &PtrTop, &InitBuffer, &Box);
        break;
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        ProcessMonoMask(false, &ptr_info_, &PtrWidth, &PtrHeight, &PtrLeft,
            &PtrTop, &InitBuffer, &Box);
        break;
      default:
        break;
    }

    /* Nothing to draw */
    if (PtrWidth == 0 || PtrHeight == 0 ||
        (PtrLeft + PtrWidth) < static_cast<INT>(cropBox->left) ||
        PtrLeft > static_cast<INT>(cropBox->right) ||
        (PtrTop + PtrHeight) < static_cast<INT>(cropBox->top) ||
        PtrTop > static_cast<INT>(cropBox->bottom)) {
      if (InitBuffer)
        delete[] InitBuffer;

      return true;
    }

    PtrLeft -= cropBox->left;
    PtrTop -= cropBox->top;

    Vertices[0].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
    Vertices[0].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
    Vertices[1].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
    Vertices[1].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;
    Vertices[2].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
    Vertices[2].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
    Vertices[3].Pos.x = Vertices[2].Pos.x;
    Vertices[3].Pos.y = Vertices[2].Pos.y;
    Vertices[4].Pos.x = Vertices[1].Pos.x;
    Vertices[4].Pos.y = Vertices[1].Pos.y;
    Vertices[5].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
    Vertices[5].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;

    Desc.Width = PtrWidth;
    Desc.Height = PtrHeight;

    InitData.pSysMem =
        (ptr_info_.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ?
         ptr_info_.PtrShapeBuffer : InitBuffer;
    InitData.SysMemPitch =
        (ptr_info_.shape_info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ?
        ptr_info_.shape_info.Pitch : PtrWidth * BPP;
    InitData.SysMemSlicePitch = 0;

    // Create mouseshape as texture
    HRESULT hr = device_handle->CreateTexture2D(&Desc, &InitData, &MouseTex);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create texture for rendering mouse");
      return false;
    }

    // Create shader resource from texture
    hr = device_handle->CreateShaderResourceView(MouseTex.Get(), &SDesc,
        &ShaderRes);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create shader resource view for rendering mouse");
      return false;
    }

    D3D11_BUFFER_DESC BDesc;
    memset (&BDesc, 0, sizeof(D3D11_BUFFER_DESC));
    BDesc.Usage = D3D11_USAGE_DEFAULT;
    BDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
    BDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BDesc.CPUAccessFlags = 0;

    memset (&InitData, 0, sizeof(D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem = Vertices;

    // Create vertex buffer
    hr = device_handle->CreateBuffer(&BDesc, &InitData, &VertexBufferMouse);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create vertex buffer for rendering mouse");
      return false;
    }

    FLOAT BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    ID3D11ShaderResourceView *srv = ShaderRes.Get();
    ID3D11Buffer *vert_buf = VertexBufferMouse.Get();

    context_handle->IASetVertexBuffers(0, 1, &vert_buf, &Stride, &Offset);
    context_handle->OMSetBlendState(resource->blend, BlendFactor, 0xFFFFFFFF);
    context_handle->OMSetRenderTargets(1, &rtv, nullptr);
    context_handle->VSSetShader(resource->vs, nullptr, 0);
    context_handle->PSSetShader(resource->ps, nullptr, 0);
    context_handle->PSSetShaderResources(0, 1, &srv);
    context_handle->PSSetSamplers(0, 1, &resource->sampler);
    context_handle->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_handle->IASetInputLayout(resource->layout);

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(FullDesc.Width);
    VP.Height = static_cast<FLOAT>(FullDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    context_handle->RSSetViewports(1, &VP);
    context_handle->RSSetState (resource->rs);

    context_handle->Draw(NUMVERTICES, 0);

    /* Unbind srv and rtv from context */
    srv = nullptr;
    context_handle->PSSetShaderResources (0, 1, &srv);
    context_handle->OMSetRenderTargets (0, nullptr, nullptr);

    if (InitBuffer)
      delete[] InitBuffer;

    return true;
  }

  GstFlowReturn
  CopyToTexture (GstD3D11Device * device, ID3D11Texture2D * texture,
      D3D11_BOX * cropBox)
  {
    ID3D11DeviceContext *context_handle = nullptr;
    ComPtr <ID3D11Texture2D> tex;
    ComPtr < IDXGIKeyedMutex > other_keyed_mutex;
    HRESULT hr;

    context_handle = gst_d3d11_device_get_device_context_handle (device);

    if (device == device_) {
      tex = shared_texture_;
    } else {
      ID3D11Device *device_handle = nullptr;
      ComPtr < IDXGIResource > dxgi_resource;
      HANDLE shared_handle;

      device_handle = gst_d3d11_device_get_device_handle (device);

      hr = shared_texture_.As (&dxgi_resource);
      if (!gst_d3d11_result (hr, device_))
        return GST_FLOW_ERROR;

      hr = dxgi_resource->GetSharedHandle (&shared_handle);
      if (!gst_d3d11_result (hr, device_))
        return GST_FLOW_ERROR;

      hr = device_handle->OpenSharedResource (shared_handle,
          IID_PPV_ARGS (&tex));
      if (!gst_d3d11_result (hr, device))
        return GST_FLOW_ERROR;

      hr = tex.As (&other_keyed_mutex);
      if (!gst_d3d11_result (hr, device))
        return GST_FLOW_ERROR;

      /* release sync from our device, and acquire for other device */
      keyed_mutex_->ReleaseSync (0);
      other_keyed_mutex->AcquireSync (0, INFINITE);
    }

    context_handle->CopySubresourceRegion (texture, 0, 0, 0, 0,
        tex.Get(), 0, cropBox);

    if (other_keyed_mutex) {
      other_keyed_mutex->ReleaseSync (0);
      keyed_mutex_->AcquireSync (0, INFINITE);
    }

    return GST_FLOW_OK;
  }

  void
  GetSize (guint * width, guint * height)
  {
    *width = output_desc_.ModeDesc.Width;
    *height = output_desc_.ModeDesc.Height;
  }

private:
  /* This method is not expected to be failed unless un-recoverable error case */
  bool
  InitShader (GstD3D11Device * device)
  {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11RasterizerState> rs;
    HRESULT hr;

    hr = gst_d3d11_get_vertex_shader_coord (device, &vs, &layout);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create vertex shader");
      return false;
    }

    hr = gst_d3d11_get_pixel_shader_sample (device, &ps);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create pixel shader");
      return false;
    }

    ComPtr<ID3D11SamplerState> sampler;
    hr = gst_d3d11_device_get_sampler (device, D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        &sampler);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Failed to create sampler state, hr 0x%x", (guint) hr);
      return false;
    }

    hr = gst_d3d11_device_get_rasterizer (device, &rs);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Couldn't get rasterizer state");
      return false;
    }

    /* Everything is prepared now */
    vs_ = vs;
    ps_ = ps;
    layout_ = layout;
    sampler_ = sampler;
    rs_ = rs;

    return true;
  }

  /* Maybe returning expected error code depending on desktop status */
  GstFlowReturn
  InitDupl (GstD3D11Device * device, HMONITOR monitor)
  {
    ComPtr<ID3D11Device> d3d11_device;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    ComPtr<IDXGIOutput1> output1;

    d3d11_device = gst_d3d11_device_get_device_handle (device);

    HRESULT hr = gst_d3d11_screen_capture_find_output_for_monitor (monitor,
        &adapter, &output);
    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR ("Couldn't get adapter and output for monitor");
      return GST_FLOW_ERROR;
    }

    hr = output.As (&output1);
    if (!gst_d3d11_result (hr, device)) {
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

    /* FIXME: Use DuplicateOutput1 to avoid potentail color conversion */
    hr = output1->DuplicateOutput(d3d11_device.Get(), &dupl_);
    if (!gst_d3d11_result (hr, device)) {
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
        return GST_D3D11_SCREEN_CAPTURE_FLOW_UNSUPPORTED;
      }

      return gst_d3d11_dxgi_capture_return_from_hr (d3d11_device.Get(), hr,
          CreateDuplicationExpectedErrors);
    }

    dupl_->GetDesc (&output_desc_);

    return GST_FLOW_OK;
  }

  GstFlowReturn
  GetMouse (PTR_INFO * ptr_info, DXGI_OUTDUPL_FRAME_INFO * frame_info)
  {
    /* A non-zero mouse update timestamp indicates that there is a mouse
     * position update and optionally a shape change */
    if (frame_info->LastMouseUpdateTime.QuadPart == 0)
      return GST_FLOW_OK;

    ptr_info->Position.x = frame_info->PointerPosition.Position.x;
    ptr_info->Position.y = frame_info->PointerPosition.Position.y;
    ptr_info->LastTimeStamp = frame_info->LastMouseUpdateTime;
    ptr_info->Visible = frame_info->PointerPosition.Visible != 0;

    /* No new shape */
    if (frame_info->PointerShapeBufferSize == 0)
      return GST_FLOW_OK;

    /* Realloc buffer if needed */
    ptr_info->MaybeReallocBuffer (frame_info->PointerShapeBufferSize);

    /* Must always get shape of cursor, even if not drawn at the moment.
     * Shape of cursor is not repeated by the AcquireNextFrame and can be
     * requested to be drawn any time later */
    UINT dummy;
    HRESULT hr = dupl_->GetFramePointerShape(frame_info->PointerShapeBufferSize,
        (void *) ptr_info->PtrShapeBuffer, &dummy, &ptr_info->shape_info);

    if (!gst_d3d11_result (hr, device_)) {
      ID3D11Device *device_handle =
          gst_d3d11_device_get_device_handle (device_);

      return gst_d3d11_dxgi_capture_return_from_hr(device_handle, hr,
          FrameInfoExpectedErrors);
    }

    return GST_FLOW_OK;
  }

  void
  MaybeReallocMetadataBuffer (UINT buffer_size)
  {
    if (buffer_size <= metadata_buffer_size_)
      return;

    if (metadata_buffer_)
      delete[] metadata_buffer_;

    metadata_buffer_ = new BYTE[buffer_size];
    metadata_buffer_size_ = buffer_size;
  }

  GstFlowReturn
  GetFrame (ID3D11Texture2D ** texture, UINT * move_count, UINT * dirty_count,
      DXGI_OUTDUPL_FRAME_INFO * frame_info, bool* timeout)
  {
    ComPtr<IDXGIResource> resource;
    ComPtr<ID3D11Texture2D> acquired_texture;
    ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device_);

    /* Get new frame */
    HRESULT hr = dupl_->AcquireNextFrame(0, frame_info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
      GST_TRACE ("Timeout");

      *timeout = true;
      return GST_FLOW_OK;
    }

    *timeout = false;
    *move_count = 0;
    *dirty_count = 0;

    if (!gst_d3d11_result (hr, device_)) {
      return gst_d3d11_dxgi_capture_return_from_hr(device_handle, hr,
          FrameInfoExpectedErrors);
    }

    GST_TRACE (
        "LastPresentTime: %" G_GINT64_FORMAT
        ", LastMouseUpdateTime: %" G_GINT64_FORMAT
        ", AccumulatedFrames: %d"
        ", RectsCoalesced: %d"
        ", ProtectedContentMaskedOut: %d"
        ", PointerPosition: (%ldx%ld, visible %d)"
        ", TotalMetadataBufferSize: %d"
        ", PointerShapeBufferSize: %d",
        frame_info->LastPresentTime.QuadPart,
        frame_info->LastMouseUpdateTime.QuadPart,
        frame_info->AccumulatedFrames,
        frame_info->RectsCoalesced,
        frame_info->ProtectedContentMaskedOut,
        frame_info->PointerPosition.Position.x,
        frame_info->PointerPosition.Position.y,
        frame_info->PointerPosition.Visible,
        frame_info->TotalMetadataBufferSize,
        frame_info->PointerShapeBufferSize);

    hr = resource.As (&acquired_texture);
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Failed to get ID3D11Texture2D interface from IDXGIResource "
          "hr 0x%x", (guint) hr);
      return GST_FLOW_ERROR;
    }

    /* Get metadata */
    if (frame_info->TotalMetadataBufferSize) {
      UINT buf_size = frame_info->TotalMetadataBufferSize;

      MaybeReallocMetadataBuffer (buf_size);

      /* Get move rectangles */
      hr = dupl_->GetFrameMoveRects(buf_size,
          (DXGI_OUTDUPL_MOVE_RECT *) metadata_buffer_, &buf_size);
      if (!gst_d3d11_result (hr, device_)) {
        GST_ERROR ("Couldn't get move rect, hr 0x%x", (guint) hr);

        return gst_d3d11_dxgi_capture_return_from_hr(nullptr, hr,
            FrameInfoExpectedErrors);
      }

      *move_count = buf_size / sizeof(DXGI_OUTDUPL_MOVE_RECT);

      GST_TRACE ("MoveRects count %d", *move_count);
#ifndef GST_DISABLE_GST_DEBUG
      {
        DXGI_OUTDUPL_MOVE_RECT *rects =
            (DXGI_OUTDUPL_MOVE_RECT *) metadata_buffer_;
        for (guint i = 0; i < *move_count; i++) {
          GST_TRACE ("MoveRect[%d] SourcePoint: %ldx%ld, "
            "DestinationRect (left:top:right:bottom): %ldx%ldx%ldx%ld",
            i, rects->SourcePoint.x, rects->SourcePoint.y,
            rects->DestinationRect.left, rects->DestinationRect.top,
            rects->DestinationRect.right, rects->DestinationRect.bottom);
        }
      }
#endif

      BYTE* dirty_rects = metadata_buffer_ + buf_size;
      buf_size = frame_info->TotalMetadataBufferSize - buf_size;

      /* Get dirty rectangles */
      hr = dupl_->GetFrameDirtyRects(buf_size, (RECT *) dirty_rects, &buf_size);
      if (!gst_d3d11_result (hr, device_)) {
        GST_ERROR ("Couldn't get dirty rect, hr 0x%x", (guint) hr);
        *move_count = 0;
        *dirty_count = 0;

        return gst_d3d11_dxgi_capture_return_from_hr(nullptr,
            hr, FrameInfoExpectedErrors);
      }

      *dirty_count = buf_size / sizeof(RECT);

      GST_TRACE ("DirtyRects count %d", *dirty_count);
#ifndef GST_DISABLE_GST_DEBUG
      {
        RECT *rects = (RECT *) dirty_rects;
        for (guint i = 0; i < *dirty_count; i++) {
          GST_TRACE ("DirtyRect[%d] left:top:right:bottom: %ldx%ldx%ldx%ld",
            i, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom);
        }
      }
#endif
    }

    *texture = acquired_texture.Detach();

    return GST_FLOW_OK;
  }

  void
  SetMoveRect (RECT* SrcRect, RECT* DestRect, DXGI_OUTDUPL_DESC* DeskDesc,
      DXGI_OUTDUPL_MOVE_RECT* MoveRect, INT TexWidth, INT TexHeight)
  {
    switch (DeskDesc->Rotation)
    {
      case DXGI_MODE_ROTATION_UNSPECIFIED:
      case DXGI_MODE_ROTATION_IDENTITY:
        SrcRect->left = MoveRect->SourcePoint.x;
        SrcRect->top = MoveRect->SourcePoint.y;
        SrcRect->right = MoveRect->SourcePoint.x +
            MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;
        SrcRect->bottom = MoveRect->SourcePoint.y +
            MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;

        *DestRect = MoveRect->DestinationRect;
        break;
      case DXGI_MODE_ROTATION_ROTATE90:
        SrcRect->left = TexHeight - (MoveRect->SourcePoint.y +
            MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
        SrcRect->top = MoveRect->SourcePoint.x;
        SrcRect->right = TexHeight - MoveRect->SourcePoint.y;
        SrcRect->bottom = MoveRect->SourcePoint.x +
            MoveRect->DestinationRect.right - MoveRect->DestinationRect.left;

        DestRect->left = TexHeight - MoveRect->DestinationRect.bottom;
        DestRect->top = MoveRect->DestinationRect.left;
        DestRect->right = TexHeight - MoveRect->DestinationRect.top;
        DestRect->bottom = MoveRect->DestinationRect.right;
        break;
      case DXGI_MODE_ROTATION_ROTATE180:
        SrcRect->left = TexWidth - (MoveRect->SourcePoint.x +
            MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
        SrcRect->top = TexHeight - (MoveRect->SourcePoint.y +
            MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top);
        SrcRect->right = TexWidth - MoveRect->SourcePoint.x;
        SrcRect->bottom = TexHeight - MoveRect->SourcePoint.y;

        DestRect->left = TexWidth - MoveRect->DestinationRect.right;
        DestRect->top = TexHeight - MoveRect->DestinationRect.bottom;
        DestRect->right = TexWidth - MoveRect->DestinationRect.left;
        DestRect->bottom =  TexHeight - MoveRect->DestinationRect.top;
        break;
      case DXGI_MODE_ROTATION_ROTATE270:
        SrcRect->left = MoveRect->SourcePoint.x;
        SrcRect->top = TexWidth - (MoveRect->SourcePoint.x +
            MoveRect->DestinationRect.right - MoveRect->DestinationRect.left);
        SrcRect->right = MoveRect->SourcePoint.y +
            MoveRect->DestinationRect.bottom - MoveRect->DestinationRect.top;
        SrcRect->bottom = TexWidth - MoveRect->SourcePoint.x;

        DestRect->left = MoveRect->DestinationRect.top;
        DestRect->top = TexWidth - MoveRect->DestinationRect.right;
        DestRect->right = MoveRect->DestinationRect.bottom;
        DestRect->bottom =  TexWidth - MoveRect->DestinationRect.left;
        break;
      default:
        memset (DestRect, 0, sizeof (RECT));
        memset (SrcRect, 0, sizeof (RECT));
        break;
    }
  }

  GstFlowReturn
  CopyMove (ID3D11Texture2D* SharedSurf, DXGI_OUTDUPL_MOVE_RECT* MoveBuffer,
      UINT MoveCount, DXGI_OUTDUPL_DESC* DeskDesc)
  {
    ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device_);
    ID3D11DeviceContext *device_context =
       gst_d3d11_device_get_device_context_handle (device_);
    D3D11_TEXTURE2D_DESC FullDesc;
    SharedSurf->GetDesc(&FullDesc);

    GST_TRACE ("Copying MoveRects (count %d)", MoveCount);

    /* Make new intermediate surface to copy into for moving */
    if (!move_texture_) {
      D3D11_TEXTURE2D_DESC MoveDesc;
      MoveDesc = FullDesc;
      MoveDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
      MoveDesc.MiscFlags = 0;
      HRESULT hr = device_handle->CreateTexture2D(&MoveDesc,
          nullptr, &move_texture_);
      if (!gst_d3d11_result (hr, device_)) {
        GST_ERROR ("Couldn't create intermediate texture, hr 0x%x", (guint) hr);
        return GST_FLOW_ERROR;
      }
    }

    for (UINT i = 0; i < MoveCount; i++) {
      RECT SrcRect;
      RECT DestRect;

      SetMoveRect(&SrcRect, &DestRect, DeskDesc, &MoveBuffer[i],
          FullDesc.Width, FullDesc.Height);

      /* Copy rect out of shared surface */
      D3D11_BOX Box;
      Box.left = SrcRect.left;
      Box.top = SrcRect.top;
      Box.front = 0;
      Box.right = SrcRect.right;
      Box.bottom = SrcRect.bottom;
      Box.back = 1;
      device_context->CopySubresourceRegion(move_texture_.Get(),
          0, SrcRect.left, SrcRect.top, 0, SharedSurf, 0, &Box);

      /* Copy back to shared surface */
      device_context->CopySubresourceRegion(SharedSurf,
          0, DestRect.left, DestRect.top, 0, move_texture_.Get(), 0, &Box);
    }

    return GST_FLOW_OK;
  }

  void
  SetDirtyVert (VERTEX* Vertices, RECT* Dirty,
      DXGI_OUTDUPL_DESC* DeskDesc, D3D11_TEXTURE2D_DESC* FullDesc,
      D3D11_TEXTURE2D_DESC* ThisDesc)
  {
    INT CenterX = FullDesc->Width / 2;
    INT CenterY = FullDesc->Height / 2;

    INT Width = FullDesc->Width;
    INT Height = FullDesc->Height;

    /* Rotation compensated destination rect */
    RECT DestDirty = *Dirty;

    /* Set appropriate coordinates compensated for rotation */
    switch (DeskDesc->Rotation)
    {
      case DXGI_MODE_ROTATION_ROTATE90:
        DestDirty.left = Width - Dirty->bottom;
        DestDirty.top = Dirty->left;
        DestDirty.right = Width - Dirty->top;
        DestDirty.bottom = Dirty->right;

        Vertices[0].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[1].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[2].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[5].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        break;
      case DXGI_MODE_ROTATION_ROTATE180:
        DestDirty.left = Width - Dirty->right;
        DestDirty.top = Height - Dirty->bottom;
        DestDirty.right = Width - Dirty->left;
        DestDirty.bottom = Height - Dirty->top;

        Vertices[0].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[1].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[2].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[5].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        break;
      case DXGI_MODE_ROTATION_ROTATE270:
        DestDirty.left = Dirty->top;
        DestDirty.top = Height - Dirty->right;
        DestDirty.right = Dirty->bottom;
        DestDirty.bottom = Height - Dirty->left;

        Vertices[0].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[1].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[2].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[5].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        break;
      case DXGI_MODE_ROTATION_UNSPECIFIED:
      case DXGI_MODE_ROTATION_IDENTITY:
      default:
        Vertices[0].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[1].TexCoord =
            XMFLOAT2(Dirty->left / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[2].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->bottom / static_cast<FLOAT>(ThisDesc->Height));
        Vertices[5].TexCoord =
            XMFLOAT2(Dirty->right / static_cast<FLOAT>(ThisDesc->Width),
                     Dirty->top / static_cast<FLOAT>(ThisDesc->Height));
        break;
    }

    /* Set positions */
    Vertices[0].Pos =
        XMFLOAT3(
          (DestDirty.left - CenterX) / static_cast<FLOAT>(CenterX),
          -1 * (DestDirty.bottom - CenterY) / static_cast<FLOAT>(CenterY),
          0.0f);
    Vertices[1].Pos =
        XMFLOAT3(
          (DestDirty.left - CenterX) / static_cast<FLOAT>(CenterX),
          -1 * (DestDirty.top - CenterY) / static_cast<FLOAT>(CenterY),
          0.0f);
    Vertices[2].Pos =
        XMFLOAT3(
          (DestDirty.right - CenterX) / static_cast<FLOAT>(CenterX),
          -1 * (DestDirty.bottom - CenterY) / static_cast<FLOAT>(CenterY),
          0.0f);
    Vertices[3].Pos = Vertices[2].Pos;
    Vertices[4].Pos = Vertices[1].Pos;
    Vertices[5].Pos =
        XMFLOAT3(
          (DestDirty.right - CenterX) / static_cast<FLOAT>(CenterX),
          -1 * (DestDirty.top - CenterY) / static_cast<FLOAT>(CenterY),
          0.0f);

    Vertices[3].TexCoord = Vertices[2].TexCoord;
    Vertices[4].TexCoord = Vertices[1].TexCoord;
  }

  void
  MaybeReallocVertexBuffer (UINT buffer_size)
  {
    if (buffer_size <= vertex_buffer_size_)
      return;

    if (vertex_buffer_)
      delete[] vertex_buffer_;

    vertex_buffer_ = new BYTE[buffer_size];
    vertex_buffer_size_ = buffer_size;
  }

  GstFlowReturn
  CopyDirty (ID3D11Texture2D* SrcSurface, ID3D11Texture2D* SharedSurf,
      RECT* DirtyBuffer, UINT DirtyCount, DXGI_OUTDUPL_DESC* DeskDesc)
  {
    D3D11_TEXTURE2D_DESC FullDesc;
    D3D11_TEXTURE2D_DESC ThisDesc;
    ComPtr<ID3D11ShaderResourceView> ShaderResource;
    ComPtr<ID3D11Buffer> VertBuf;
    HRESULT hr;
    ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device_);
    ID3D11DeviceContext *device_context =
       gst_d3d11_device_get_device_context_handle (device_);

    GST_TRACE ("Copying DirtyRects (count %d)", DirtyCount);

    SharedSurf->GetDesc(&FullDesc);
    SrcSurface->GetDesc(&ThisDesc);

    if (!rtv_) {
      hr = device_handle->CreateRenderTargetView(SharedSurf, nullptr, &rtv_);
      if (!gst_d3d11_result (hr, device_)) {
        GST_ERROR ("Couldn't create render target view, hr 0x%x", (guint) hr);
        return GST_FLOW_ERROR;
      }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
    ShaderDesc.Format = ThisDesc.Format;
    ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ShaderDesc.Texture2D.MostDetailedMip = ThisDesc.MipLevels - 1;
    ShaderDesc.Texture2D.MipLevels = ThisDesc.MipLevels;

    /* Create new shader resource view */
    hr = device_handle->CreateShaderResourceView(SrcSurface,
        &ShaderDesc, &ShaderResource);
    if (!gst_d3d11_result (hr, device_)) {
      return gst_d3d11_dxgi_capture_return_from_hr(device_handle, hr,
          SystemTransitionsExpectedErrors);
    }

    ID3D11SamplerState *samplers = sampler_.Get();
    ID3D11ShaderResourceView *srv = ShaderResource.Get();
    ID3D11RenderTargetView *rtv = rtv_.Get();
    device_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    device_context->OMSetRenderTargets(1, &rtv, nullptr);
    device_context->VSSetShader(vs_.Get(), nullptr, 0);
    device_context->PSSetShader(ps_.Get(), nullptr, 0);
    device_context->PSSetShaderResources(0, 1, &srv);
    device_context->PSSetSamplers(0, 1, &samplers);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_context->IASetInputLayout(layout_.Get());

    /* Create space for vertices for the dirty rects if the current space isn't
     * large enough */
    UINT byte_needed = sizeof(VERTEX) * NUMVERTICES * DirtyCount;
    MaybeReallocVertexBuffer (byte_needed);

    /* Fill them in */
    VERTEX* DirtyVertex = (VERTEX *) vertex_buffer_;
    for (UINT i = 0; i < DirtyCount; ++i, DirtyVertex += NUMVERTICES) {
      SetDirtyVert(DirtyVertex, &DirtyBuffer[i], DeskDesc,
          &FullDesc, &ThisDesc);
    }

    /* Create vertex buffer */
    D3D11_BUFFER_DESC BufferDesc;
    memset (&BufferDesc, 0, sizeof (D3D11_BUFFER_DESC));
    BufferDesc.Usage = D3D11_USAGE_DEFAULT;
    BufferDesc.ByteWidth = byte_needed;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    memset (&InitData, 0, sizeof (D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem = vertex_buffer_;

    hr = device_handle->CreateBuffer(&BufferDesc, &InitData, &VertBuf);
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Failed to create vertex buffer");
      return GST_FLOW_ERROR;
    }

    UINT Stride = sizeof(VERTEX);
    UINT Offset = 0;
    ID3D11Buffer *vert_buf = VertBuf.Get();
    device_context->IASetVertexBuffers(0, 1, &vert_buf, &Stride, &Offset);

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(FullDesc.Width);
    VP.Height = static_cast<FLOAT>(FullDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    device_context->RSSetViewports(1, &VP);
    device_context->RSSetState (rs_.Get ());

    device_context->Draw(NUMVERTICES * DirtyCount, 0);

    /* Unbind srv and rtv from context */
    srv = nullptr;
    device_context->PSSetShaderResources (0, 1, &srv);
    device_context->OMSetRenderTargets (0, nullptr, nullptr);

    return GST_FLOW_OK;
  }

  GstFlowReturn
  ProcessFrame(ID3D11Texture2D * acquired_texture, ID3D11Texture2D* SharedSurf,
      DXGI_OUTDUPL_DESC* DeskDesc, UINT move_count, UINT dirty_count,
      DXGI_OUTDUPL_FRAME_INFO * frame_info)
  {
    GstFlowReturn ret = GST_FLOW_OK;

    GST_TRACE ("Processing frame");

    /* Process dirties and moves */
    if (frame_info->TotalMetadataBufferSize) {
      if (move_count) {
        ret = CopyMove(SharedSurf, (DXGI_OUTDUPL_MOVE_RECT *) metadata_buffer_,
            move_count, DeskDesc);

        if (ret != GST_FLOW_OK)
          return ret;
      }

      if (dirty_count) {
        ret = CopyDirty(acquired_texture, SharedSurf,
            (RECT *)(metadata_buffer_ +
                (move_count * sizeof(DXGI_OUTDUPL_MOVE_RECT))),
            dirty_count, DeskDesc);
      }
    } else {
      GST_TRACE ("No metadata");
    }

    return ret;
  }

  /* To draw mouse */
  bool
  ProcessMonoMask (bool IsMono, PTR_INFO* PtrInfo, INT* PtrWidth,
      INT* PtrHeight, INT* PtrLeft, INT* PtrTop, BYTE** InitBuffer,
      D3D11_BOX* Box)
  {
    ID3D11Device *device_handle = gst_d3d11_device_get_device_handle (device_);
    ID3D11DeviceContext *context_handle =
        gst_d3d11_device_get_device_context_handle (device_);

    D3D11_TEXTURE2D_DESC FullDesc;
    shared_texture_->GetDesc(&FullDesc);
    INT DesktopWidth = FullDesc.Width;
    INT DesktopHeight = FullDesc.Height;

    // Pointer position
    INT GivenLeft = PtrInfo->Position.x;
    INT GivenTop = PtrInfo->Position.y;

    // Figure out if any adjustment is needed for out of bound positions
    if (GivenLeft < 0) {
      *PtrWidth = GivenLeft + static_cast<INT>(PtrInfo->shape_info.Width);
    } else if ((GivenLeft + static_cast<INT>(PtrInfo->shape_info.Width)) > DesktopWidth) {
      *PtrWidth = DesktopWidth - GivenLeft;
    } else {
      *PtrWidth = static_cast<INT>(PtrInfo->shape_info.Width);
    }

    if (IsMono)
      PtrInfo->shape_info.Height = PtrInfo->shape_info.Height / 2;

    if (GivenTop < 0) {
      *PtrHeight = GivenTop + static_cast<INT>(PtrInfo->shape_info.Height);
    } else if ((GivenTop + static_cast<INT>(PtrInfo->shape_info.Height)) > DesktopHeight) {
      *PtrHeight = DesktopHeight - GivenTop;
    } else {
      *PtrHeight = static_cast<INT>(PtrInfo->shape_info.Height);
    }

    if (IsMono)
      PtrInfo->shape_info.Height = PtrInfo->shape_info.Height * 2;

    *PtrLeft = (GivenLeft < 0) ? 0 : GivenLeft;
    *PtrTop = (GivenTop < 0) ? 0 : GivenTop;

    D3D11_TEXTURE2D_DESC CopyBufferDesc;
    CopyBufferDesc.Width = *PtrWidth;
    CopyBufferDesc.Height = *PtrHeight;
    CopyBufferDesc.MipLevels = 1;
    CopyBufferDesc.ArraySize = 1;
    CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    CopyBufferDesc.SampleDesc.Count = 1;
    CopyBufferDesc.SampleDesc.Quality = 0;
    CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
    CopyBufferDesc.BindFlags = 0;
    CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CopyBufferDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> CopyBuffer;
    HRESULT hr = device_handle->CreateTexture2D(&CopyBufferDesc,
        nullptr, &CopyBuffer);
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Couldn't create texture for mouse pointer");
      return false;
    }

    Box->left = *PtrLeft;
    Box->top = *PtrTop;
    Box->right = *PtrLeft + *PtrWidth;
    Box->bottom = *PtrTop + *PtrHeight;
    context_handle->CopySubresourceRegion(CopyBuffer.Get(),
        0, 0, 0, 0, shared_texture_.Get(), 0, Box);

    ComPtr<IDXGISurface> CopySurface;
    hr = CopyBuffer.As (&CopySurface);
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Couldn't get DXGI resource from mouse texture");
      return false;
    }

    DXGI_MAPPED_RECT MappedSurface;
    hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Couldn't map DXGI surface");
      return false;
    }

    *InitBuffer = new BYTE[*PtrWidth * *PtrHeight * BPP];

    UINT* InitBuffer32 = reinterpret_cast<UINT*>(*InitBuffer);
    UINT* Desktop32 = reinterpret_cast<UINT*>(MappedSurface.pBits);
    UINT  DesktopPitchInPixels = MappedSurface.Pitch / sizeof(UINT);

    // What to skip (pixel offset)
    UINT SkipX = (GivenLeft < 0) ? (-1 * GivenLeft) : (0);
    UINT SkipY = (GivenTop < 0) ? (-1 * GivenTop) : (0);

    if (IsMono) {
      for (INT Row = 0; Row < *PtrHeight; Row++) {
        BYTE Mask = 0x80;
        Mask = Mask >> (SkipX % 8);
        for (INT Col = 0; Col < *PtrWidth; Col++) {
          BYTE AndMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) +
              ((Row + SkipY) * (PtrInfo->shape_info.Pitch))] & Mask;
          BYTE XorMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) +
              ((Row + SkipY + (PtrInfo->shape_info.Height / 2)) *
                  (PtrInfo->shape_info.Pitch))] & Mask;
          UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
          UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

          InitBuffer32[(Row * *PtrWidth) + Col] =
              (Desktop32[(Row * DesktopPitchInPixels) + Col] & AndMask32) ^ XorMask32;

          if (Mask == 0x01) {
              Mask = 0x80;
          } else {
              Mask = Mask >> 1;
          }
        }
      }
    } else {
      UINT* Buffer32 = reinterpret_cast<UINT*>(PtrInfo->PtrShapeBuffer);

      for (INT Row = 0; Row < *PtrHeight; Row++) {
        for (INT Col = 0; Col < *PtrWidth; ++Col) {
          // Set up mask
          UINT MaskVal = 0xFF000000 & Buffer32[(Col + SkipX) + ((Row + SkipY) *
              (PtrInfo->shape_info.Pitch / sizeof(UINT)))];
          if (MaskVal) {
            // Mask was 0xFF
            InitBuffer32[(Row * *PtrWidth) + Col] =
                (Desktop32[(Row * DesktopPitchInPixels) + Col] ^
                    Buffer32[(Col + SkipX) + ((Row + SkipY) *
                    (PtrInfo->shape_info.Pitch / sizeof(UINT)))]) | 0xFF000000;
          } else {
            // Mask was 0x00
            InitBuffer32[(Row * *PtrWidth) + Col] = Buffer32[(Col + SkipX) +
                ((Row + SkipY) * (PtrInfo->shape_info.Pitch / sizeof(UINT)))] | 0xFF000000;
          }
        }
      }
    }

    // Done with resource
    hr = CopySurface->Unmap();
    if (!gst_d3d11_result (hr, device_)) {
      GST_ERROR ("Failed to unmap DXGI surface");
      return false;
    }

    return true;
  }

private:
  PTR_INFO ptr_info_;
  DXGI_OUTDUPL_DESC output_desc_;
  GstD3D11Device * device_;

  ComPtr<ID3D11Texture2D> shared_texture_;
  ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  ComPtr<ID3D11RenderTargetView> rtv_;
  ComPtr<ID3D11Texture2D> move_texture_;
  ComPtr<ID3D11VertexShader> vs_;
  ComPtr<ID3D11PixelShader> ps_;
  ComPtr<ID3D11InputLayout> layout_;
  ComPtr<ID3D11SamplerState> sampler_;
  ComPtr<ID3D11RasterizerState> rs_;
  ComPtr<IDXGIOutputDuplication> dupl_;

  /* frame metadata */
  BYTE *metadata_buffer_;
  UINT metadata_buffer_size_;

  /* vertex buffers */
  BYTE *vertex_buffer_;
  UINT vertex_buffer_size_;
};
/* *INDENT-ON* */

enum
{
  PROP_0,
  PROP_D3D11_DEVICE,
  PROP_MONITOR_HANDLE,
};

struct _GstD3D11DxgiCapture
{
  GstD3D11ScreenCapture parent;

  GstD3D11Device *device;
  guint cached_width;
  guint cached_height;

  D3D11DesktopDupObject *dupl_obj;
  IDXGIOutput *output;

  HMONITOR monitor_handle;
  RECT desktop_coordinates;
  gboolean prepared;
  gint64 adapter_luid;

  CRITICAL_SECTION lock;
};

static void gst_d3d11_dxgi_capture_constructed (GObject * object);
static void gst_d3d11_dxgi_capture_dispose (GObject * object);
static void gst_d3d11_dxgi_capture_finalize (GObject * object);
static void gst_d3d11_dxgi_capture_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static GstFlowReturn
gst_d3d11_dxgi_capture_prepare (GstD3D11ScreenCapture * capture);
static gboolean
gst_d3d11_dxgi_capture_get_size (GstD3D11ScreenCapture * capture,
    guint * width, guint * height);
static GstFlowReturn
gst_d3d11_dxgi_capture_do_capture (GstD3D11ScreenCapture * capture,
    GstD3D11Device * device, ID3D11Texture2D * texture,
    ID3D11RenderTargetView * rtv, ShaderResource * resource,
    D3D11_BOX * crop_box, gboolean draw_mouse);

#define gst_d3d11_dxgi_capture_parent_class parent_class
G_DEFINE_TYPE (GstD3D11DxgiCapture, gst_d3d11_dxgi_capture,
    GST_TYPE_D3D11_SCREEN_CAPTURE);

static void
gst_d3d11_dxgi_capture_class_init (GstD3D11DxgiCaptureClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstD3D11ScreenCaptureClass *capture_class =
      GST_D3D11_SCREEN_CAPTURE_CLASS (klass);

  gobject_class->constructed = gst_d3d11_dxgi_capture_constructed;
  gobject_class->dispose = gst_d3d11_dxgi_capture_dispose;
  gobject_class->finalize = gst_d3d11_dxgi_capture_finalize;
  gobject_class->set_property = gst_d3d11_dxgi_capture_set_property;

  g_object_class_install_property (gobject_class, PROP_D3D11_DEVICE,
      g_param_spec_object ("d3d11device", "D3D11 Device",
          "GstD3D11Device object for operating",
          GST_TYPE_D3D11_DEVICE, (GParamFlags)
          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MONITOR_HANDLE,
      g_param_spec_pointer ("monitor-handle", "Monitor Handle",
          "A HMONITOR handle of monitor to capture", (GParamFlags)
          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
              G_PARAM_STATIC_STRINGS)));

  capture_class->prepare = GST_DEBUG_FUNCPTR (gst_d3d11_dxgi_capture_prepare);
  capture_class->get_size = GST_DEBUG_FUNCPTR (gst_d3d11_dxgi_capture_get_size);
  capture_class->do_capture =
      GST_DEBUG_FUNCPTR (gst_d3d11_dxgi_capture_do_capture);
}

static void
gst_d3d11_dxgi_capture_init (GstD3D11DxgiCapture * self)
{
  InitializeCriticalSection (&self->lock);

  memset (&self->desktop_coordinates, 0, sizeof (RECT));
}

static void
gst_d3d11_dxgi_capture_constructed (GObject * object)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (object);
  /* *INDENT-OFF* */
  ComPtr<IDXGIDevice> dxgi_device;
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<IDXGIOutput> output;
  ComPtr<IDXGIOutput1> output1;
  /* *INDENT-ON* */
  HRESULT hr;
  gboolean ret = FALSE;
  DXGI_OUTPUT_DESC output_desc;
  DXGI_ADAPTER_DESC adapter_desc;
  gint64 luid, device_luid;

  if (!self->device) {
    GST_WARNING_OBJECT (self, "D3D11 device is unavailable");
    goto out;
  }

  if (!self->monitor_handle) {
    GST_WARNING_OBJECT (self, "Null monitor handle");
    goto out;
  }

  hr = gst_d3d11_screen_capture_find_output_for_monitor (self->monitor_handle,
      &adapter, &output);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_WARNING_OBJECT (self,
        "Failed to find associated adapter for monitor %p",
        self->monitor_handle);
    goto out;
  }

  hr = output.As (&output1);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_WARNING_OBJECT (self, "IDXGIOutput1 interface is unavailble");
    goto out;
  }

  hr = adapter->GetDesc (&adapter_desc);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_WARNING_OBJECT (self, "Failed to get adapter desc");
    goto out;
  }

  luid = gst_d3d11_luid_to_int64 (&adapter_desc.AdapterLuid);
  g_object_get (self->device, "adapter-luid", &device_luid, nullptr);
  if (luid != device_luid) {
    GST_WARNING_OBJECT (self, "Incompatible d3d11 device");
    goto out;
  }

  hr = output->GetDesc (&output_desc);
  if (!gst_d3d11_result (hr, self->device)) {
    GST_WARNING_OBJECT (self, "Failed to get output desc");
    goto out;
  }

  /* DesktopCoordinates will not report actual texture size in case that
   * application is running without dpi-awareness. To get actual monitor size,
   * we need to use Win32 API... */
  MONITORINFOEXW monitor_info;
  DEVMODEW dev_mode;

  monitor_info.cbSize = sizeof (MONITORINFOEXW);
  if (!GetMonitorInfoW (output_desc.Monitor, (LPMONITORINFO) & monitor_info)) {
    GST_WARNING_OBJECT (self, "Couldn't get monitor info");
    goto out;
  }

  dev_mode.dmSize = sizeof (DEVMODEW);
  dev_mode.dmDriverExtra = sizeof (POINTL);
  dev_mode.dmFields = DM_POSITION;
  if (!EnumDisplaySettingsW
      (monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode)) {
    GST_WARNING_OBJECT (self, "Couldn't enumerate display settings");
    goto out;
  }

  self->desktop_coordinates.left = dev_mode.dmPosition.x;
  self->desktop_coordinates.top = dev_mode.dmPosition.y;
  self->desktop_coordinates.right =
      dev_mode.dmPosition.x + dev_mode.dmPelsWidth;
  self->desktop_coordinates.bottom =
      dev_mode.dmPosition.y + dev_mode.dmPelsHeight;

  self->cached_width =
      self->desktop_coordinates.right - self->desktop_coordinates.left;
  self->cached_height =
      self->desktop_coordinates.bottom - self->desktop_coordinates.top;

  GST_DEBUG_OBJECT (self,
      "Desktop coordinates left:top:right:bottom = %ld:%ld:%ld:%ld (%dx%d)",
      self->desktop_coordinates.left, self->desktop_coordinates.top,
      self->desktop_coordinates.right, self->desktop_coordinates.bottom,
      self->cached_width, self->cached_height);

  g_object_get (self->device, "adapter-luid", &self->adapter_luid, nullptr);

  self->output = output.Detach ();

  ret = TRUE;

out:
  if (!ret)
    gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_d3d11_dxgi_capture_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (object);

  switch (prop_id) {
    case PROP_D3D11_DEVICE:
      self->device = (GstD3D11Device *) g_value_dup_object (value);
      break;
    case PROP_MONITOR_HANDLE:
      self->monitor_handle = (HMONITOR) g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_dxgi_capture_dispose (GObject * object)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (object);

  GST_D3D11_CLEAR_COM (self->output);

  if (self->dupl_obj) {
    delete self->dupl_obj;
    self->dupl_obj = nullptr;
  }

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_dxgi_capture_finalize (GObject * object)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (object);

  DeleteCriticalSection (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d11_dxgi_capture_weak_ref_notify (gpointer data,
    GstD3D11DxgiCapture * dupl)
{
  std::lock_guard < std::mutex > lk (dupl_list_lock);
  dupl_list = g_list_remove (dupl_list, dupl);
}

GstD3D11ScreenCapture *
gst_d3d11_dxgi_capture_new (GstD3D11Device * device, HMONITOR monitor_handle)
{
  GstD3D11DxgiCapture *self = nullptr;
  GList *iter;

  g_return_val_if_fail (GST_IS_D3D11_DEVICE (device), nullptr);

  /* Check if we have dup object corresponding to monitor_handle,
   * and if there is already configured capture object, reuse it.
   * This is because of the limitation of desktop duplication API
   * (i.e., in a process, only one duplication object can exist).
   * See also
   * https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput#remarks
   */
  std::lock_guard < std::mutex > lk (dupl_list_lock);
  for (iter = dupl_list; iter; iter = g_list_next (iter)) {
    GstD3D11DxgiCapture *dupl = (GstD3D11DxgiCapture *) iter->data;

    if (dupl->monitor_handle == monitor_handle) {
      GST_DEBUG ("Found configured desktop dup object for monitor handle %p",
          monitor_handle);
      gst_object_ref (dupl);
      return GST_D3D11_SCREEN_CAPTURE_CAST (dupl);
    }
  }

  self = (GstD3D11DxgiCapture *) g_object_new (GST_TYPE_D3D11_DXGI_CAPTURE,
      "d3d11device", device, "monitor-handle", monitor_handle, nullptr);

  if (!self->device) {
    GST_WARNING_OBJECT (self, "Couldn't configure desktop dup object");
    gst_object_unref (self);

    return nullptr;
  }

  g_object_weak_ref (G_OBJECT (self),
      (GWeakNotify) gst_d3d11_dxgi_capture_weak_ref_notify, nullptr);
  dupl_list = g_list_append (dupl_list, self);

  return GST_D3D11_SCREEN_CAPTURE_CAST (self);
}

static GstFlowReturn
gst_d3d11_dxgi_capture_prepare (GstD3D11ScreenCapture * capture)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (capture);
  GstFlowReturn ret;

  GstD3D11CSLockGuard lk (&self->lock);

  if (self->prepared) {
    GST_DEBUG_OBJECT (self, "Already prepared");
    return GST_FLOW_OK;
  }

  self->dupl_obj = new D3D11DesktopDupObject ();
  ret = self->dupl_obj->Init (self->device, self->monitor_handle);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (capture,
        "Couldn't prepare capturing, %sexpected failure",
        ret == GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR ? "" : "un");

    delete self->dupl_obj;
    self->dupl_obj = nullptr;

    return ret;
  }

  self->prepared = TRUE;

  return GST_FLOW_OK;
}

static gboolean
gst_d3d11_dxgi_capture_get_size_unlocked (GstD3D11DxgiCapture * self,
    guint * width, guint * height)
{
  *width = 0;
  *height = 0;

  if (self->dupl_obj) {
    self->dupl_obj->GetSize (&self->cached_width, &self->cached_height);
  }

  *width = self->cached_width;
  *height = self->cached_height;

  return TRUE;
}


static gboolean
gst_d3d11_dxgi_capture_get_size (GstD3D11ScreenCapture * capture,
    guint * width, guint * height)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (capture);
  GstD3D11CSLockGuard lk (&self->lock);

  return gst_d3d11_dxgi_capture_get_size_unlocked (self, width, height);
}

static GstFlowReturn
gst_d3d11_dxgi_capture_do_capture (GstD3D11ScreenCapture * capture,
    GstD3D11Device * device, ID3D11Texture2D * texture,
    ID3D11RenderTargetView * rtv, ShaderResource * resource,
    D3D11_BOX * crop_box, gboolean draw_mouse)
{
  GstD3D11DxgiCapture *self = GST_D3D11_DXGI_CAPTURE (capture);
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean shared_device = FALSE;
  guint width, height;

  if (device != self->device) {
    gint64 luid;

    g_object_get (device, "adapter-luid", &luid, nullptr);
    /* source element must hold d3d11 device for the same GPU already
     * by DXGI duplication API design */
    if (luid != self->adapter_luid) {
      GST_ERROR_OBJECT (self, "Trying to capture from different device");
      return GST_FLOW_ERROR;
    }

    shared_device = TRUE;
  }

  GstD3D11CSLockGuard lk (&self->lock);
  if (!self->prepared)
    ret = gst_d3d11_dxgi_capture_prepare (capture);

  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "We are not prepared");
    return ret;
  }

  gst_d3d11_dxgi_capture_get_size_unlocked (self, &width, &height);

  if (crop_box->left > width || crop_box->right > width ||
      crop_box->top > height || crop_box->bottom > height) {
    GST_INFO_OBJECT (self,
        "Capture area (%u, %u, %u, %u) doesn't fit into screen size %ux%u",
        crop_box->left, crop_box->right, crop_box->top,
        crop_box->bottom, width, height);

    return GST_D3D11_SCREEN_CAPTURE_FLOW_SIZE_CHANGED;
  }

  gst_d3d11_device_lock (self->device);
  ret = self->dupl_obj->Capture ();
  if (ret != GST_FLOW_OK) {
    gst_d3d11_device_unlock (self->device);

    delete self->dupl_obj;
    self->dupl_obj = nullptr;
    self->prepared = FALSE;

    if (ret == GST_D3D11_SCREEN_CAPTURE_FLOW_EXPECTED_ERROR) {
      GST_WARNING_OBJECT (self, "Couldn't capture frame, but expected failure");
    } else {
      GST_ERROR_OBJECT (self, "Unexpected failure during capture");
    }

    return ret;
  }

  GST_LOG_OBJECT (self, "Capture done");
  if (shared_device)
    gst_d3d11_device_lock (device);

  ret = self->dupl_obj->CopyToTexture (device, texture, crop_box);
  if (ret != GST_FLOW_OK)
    goto out;

  if (draw_mouse)
    self->dupl_obj->DrawMouse (device, rtv, resource, crop_box);

out:
  if (shared_device)
    gst_d3d11_device_unlock (device);

  gst_d3d11_device_unlock (self->device);

  return ret;
}
