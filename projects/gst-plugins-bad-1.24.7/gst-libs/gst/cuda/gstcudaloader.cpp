/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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

#include "cuda-gst.h"
#include "gstcudaloader.h"
#include <gmodule.h>
#include "gstcuda-private.h"

GST_DEBUG_CATEGORY (gst_cudaloader_debug);
#define GST_CAT_DEFAULT gst_cudaloader_debug

#ifndef G_OS_WIN32
#define CUDA_LIBNAME "libcuda.so.1"
#else
#define CUDA_LIBNAME "nvcuda.dll"
#endif

#define LOAD_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_ERROR ("Failed to load '%s' from %s, %s", G_STRINGIFY (name), filename, g_module_error()); \
    g_module_close (module); \
    return; \
  } \
} G_STMT_END;

#define LOAD_OPTIONAL_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_WARNING ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    return; \
  } \
} G_STMT_END;

/* *INDENT-OFF* */
typedef struct _GstNvCodecCudaVTable
{
  gboolean loaded;
  gboolean have_virtual_alloc;

  CUresult (CUDAAPI * CuInit) (unsigned int Flags);
  CUresult (CUDAAPI * CuGetErrorName) (CUresult error, const char **pStr);
  CUresult (CUDAAPI * CuGetErrorString) (CUresult error, const char **pStr);

  CUresult (CUDAAPI * CuCtxCreate) (CUcontext * pctx, unsigned int flags,
      CUdevice dev);
  CUresult (CUDAAPI * CuCtxDestroy) (CUcontext ctx);
  CUresult (CUDAAPI * CuCtxPopCurrent) (CUcontext * pctx);
  CUresult (CUDAAPI * CuCtxPushCurrent) (CUcontext ctx);

  CUresult (CUDAAPI * CuCtxEnablePeerAccess) (CUcontext peerContext,
      unsigned int Flags);
  CUresult (CUDAAPI * CuCtxDisablePeerAccess) (CUcontext peerContext);
  CUresult (CUDAAPI * CuGraphicsMapResources) (unsigned int count,
      CUgraphicsResource * resources, CUstream hStream);
  CUresult (CUDAAPI * CuGraphicsUnmapResources) (unsigned int count,
      CUgraphicsResource * resources, CUstream hStream);
  CUresult (CUDAAPI *
      CuGraphicsResourceSetMapFlags) (CUgraphicsResource resource,
      unsigned int flags);
  CUresult (CUDAAPI * CuGraphicsSubResourceGetMappedArray) (CUarray * pArray,
      CUgraphicsResource resource, unsigned int arrayIndex,
      unsigned int mipLevel);
  CUresult (CUDAAPI * CuGraphicsResourceGetMappedPointer) (CUdeviceptr *
      pDevPtr, size_t * pSize, CUgraphicsResource resource);
  CUresult (CUDAAPI *
      CuGraphicsUnregisterResource) (CUgraphicsResource resource);

  CUresult (CUDAAPI * CuMemAlloc) (CUdeviceptr * dptr, unsigned int bytesize);
  CUresult (CUDAAPI * CuMemAllocPitch) (CUdeviceptr * dptr, size_t * pPitch,
      size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes);
  CUresult (CUDAAPI * CuMemAllocHost) (void **pp, unsigned int bytesize);
  CUresult (CUDAAPI * CuMemcpy2D) (const CUDA_MEMCPY2D * pCopy);
  CUresult (CUDAAPI * CuMemcpy2DAsync) (const CUDA_MEMCPY2D * pCopy,
      CUstream hStream);

  CUresult (CUDAAPI * CuMemFree) (CUdeviceptr dptr);
  CUresult (CUDAAPI * CuMemFreeHost) (void *p);

  CUresult (CUDAAPI * CuStreamCreate) (CUstream * phStream,
      unsigned int Flags);
  CUresult (CUDAAPI * CuStreamDestroy) (CUstream hStream);
  CUresult (CUDAAPI * CuStreamSynchronize) (CUstream hStream);

  CUresult (CUDAAPI * CuDeviceGet) (CUdevice * device, int ordinal);
  CUresult (CUDAAPI * CuDeviceGetCount) (int *count);
  CUresult (CUDAAPI * CuDeviceGetName) (char *name, int len, CUdevice dev);
  CUresult (CUDAAPI * CuDeviceGetAttribute) (int *pi,
      CUdevice_attribute attrib, CUdevice dev);
  CUresult (CUDAAPI * CuDeviceCanAccessPeer) (int *canAccessPeer,
      CUdevice dev, CUdevice peerDev);
  CUresult (CUDAAPI * CuDriverGetVersion) (int *driverVersion);

  CUresult (CUDAAPI * CuModuleLoadData) (CUmodule * module,
      const void *image);
  CUresult (CUDAAPI * CuModuleUnload) (CUmodule module);
  CUresult (CUDAAPI * CuModuleGetFunction) (CUfunction * hfunc,
      CUmodule hmod, const char *name);
  CUresult (CUDAAPI * CuTexObjectCreate) (CUtexObject * pTexObject,
      const CUDA_RESOURCE_DESC * pResDesc, const CUDA_TEXTURE_DESC * pTexDesc,
      const CUDA_RESOURCE_VIEW_DESC * pResViewDesc);
  CUresult (CUDAAPI * CuTexObjectDestroy) (CUtexObject texObject);
  CUresult (CUDAAPI * CuLaunchKernel) (CUfunction f, unsigned int gridDimX,
      unsigned int gridDimY, unsigned int gridDimZ,
      unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
      unsigned int sharedMemBytes, CUstream hStream, void **kernelParams,
      void **extra);

  CUresult (CUDAAPI * CuGraphicsGLRegisterImage) (CUgraphicsResource *
      pCudaResource, unsigned int image, unsigned int target,
      unsigned int Flags);
  CUresult (CUDAAPI * CuGraphicsGLRegisterBuffer) (CUgraphicsResource *
      pCudaResource, unsigned int buffer, unsigned int Flags);
  CUresult (CUDAAPI * CuGLGetDevices) (unsigned int *pCudaDeviceCount,
      CUdevice * pCudaDevices, unsigned int cudaDeviceCount,
      CUGLDeviceList deviceList);

  CUresult (CUDAAPI * CuEventCreate) (CUevent *phEvent, unsigned int Flags);
  CUresult (CUDAAPI * CuEventDestroy) (CUevent hEvent);
  CUresult (CUDAAPI * CuEventRecord) (CUevent hEvent, CUstream hStream);
  CUresult (CUDAAPI * CuEventSynchronize) (CUevent hEvent);

  CUresult (CUDAAPI * CuIpcGetEventHandle) (CUipcEventHandle *pHandle,
      CUevent event);
  CUresult (CUDAAPI * CuIpcOpenEventHandle) (CUevent* phEvent,
      CUipcEventHandle handle);

  CUresult (CUDAAPI * CuIpcGetMemHandle) (CUipcMemHandle *pHandle,
      CUdeviceptr dptr);
  CUresult (CUDAAPI * CuIpcOpenMemHandle) (CUdeviceptr *pdptr,
      CUipcMemHandle handle, unsigned int Flags);
  CUresult (CUDAAPI * CuIpcCloseMemHandle) (CUdeviceptr dptr);

#ifdef G_OS_WIN32
  CUresult (CUDAAPI * CuGraphicsD3D11RegisterResource) (CUgraphicsResource *
      pCudaResource, ID3D11Resource * pD3DResource, unsigned int Flags);
  CUresult (CUDAAPI * CuD3D11GetDevice) (CUdevice * device,
      IDXGIAdapter * pAdapter);
  CUresult (CUDAAPI * CuD3D11GetDevices) (unsigned int *pCudaDeviceCount,
      CUdevice * pCudaDevices, unsigned int cudaDeviceCount,
      ID3D11Device * pD3D11Device, CUd3d11DeviceList deviceList);
#endif

  CUresult (CUDAAPI * CuMemAddressReserve) (CUdeviceptr *ptr, size_t size,
      size_t alignment, CUdeviceptr addr, unsigned long long flags);
  CUresult (CUDAAPI * CuMemAddressFree) (CUdeviceptr ptr, size_t size);
  CUresult (CUDAAPI * CuMemCreate) (CUmemGenericAllocationHandle *handle,
      size_t size, const CUmemAllocationProp *prop, unsigned long long flags);
  CUresult (CUDAAPI * CuMemRelease) (CUmemGenericAllocationHandle handle);
  CUresult (CUDAAPI * CuMemExportToShareableHandle) (void *shareableHandle,
      CUmemGenericAllocationHandle handle, CUmemAllocationHandleType handleType,
      unsigned long long flags);
  CUresult (CUDAAPI * CuMemImportFromShareableHandle)
      (CUmemGenericAllocationHandle *handle, void *osHandle,
       CUmemAllocationHandleType shHandleType);
  CUresult (CUDAAPI * CuMemSetAccess) (CUdeviceptr ptr, size_t size,
      const CUmemAccessDesc *desc, size_t count);
  CUresult (CUDAAPI * CuMemGetAccess) (unsigned long long *flags,
      const CUmemLocation *location, CUdeviceptr ptr);
  CUresult (CUDAAPI * CuMemGetAllocationGranularity) (size_t *granularity,
      const CUmemAllocationProp *prop, CUmemAllocationGranularity_flags option);
  CUresult (CUDAAPI * CuMemGetAllocationPropertiesFromHandle)
      (CUmemAllocationProp *prop, CUmemGenericAllocationHandle handle);
  CUresult (CUDAAPI * CuMemMap) (CUdeviceptr ptr, size_t size, size_t offset,
       CUmemGenericAllocationHandle handle, unsigned long long flags);
  CUresult (CUDAAPI * CuMemUnmap) (CUdeviceptr ptr, size_t size);
  CUresult (CUDAAPI * CuMemRetainAllocationHandle)
      (CUmemGenericAllocationHandle *handle, void *addr);
} GstNvCodecCudaVTable;
/* *INDENT-ON* */

static GstNvCodecCudaVTable gst_cuda_vtable = { 0, };

static void
gst_cuda_load_optional_symbols (GModule * module)
{
  GstNvCodecCudaVTable *vtable = &gst_cuda_vtable;

  LOAD_OPTIONAL_SYMBOL (cuMemAddressReserve, CuMemAddressReserve);
  LOAD_OPTIONAL_SYMBOL (cuMemAddressFree, CuMemAddressFree);
  LOAD_OPTIONAL_SYMBOL (cuMemCreate, CuMemCreate);
  LOAD_OPTIONAL_SYMBOL (cuMemRelease, CuMemRelease);
  LOAD_OPTIONAL_SYMBOL (cuMemExportToShareableHandle,
      CuMemExportToShareableHandle);
  LOAD_OPTIONAL_SYMBOL (cuMemImportFromShareableHandle,
      CuMemImportFromShareableHandle);
  LOAD_OPTIONAL_SYMBOL (cuMemSetAccess, CuMemSetAccess);
  LOAD_OPTIONAL_SYMBOL (cuMemGetAccess, CuMemGetAccess);
  LOAD_OPTIONAL_SYMBOL (cuMemGetAllocationGranularity,
      CuMemGetAllocationGranularity);
  LOAD_OPTIONAL_SYMBOL (cuMemGetAllocationPropertiesFromHandle,
      CuMemGetAllocationPropertiesFromHandle);
  LOAD_OPTIONAL_SYMBOL (cuMemMap, CuMemMap);
  LOAD_OPTIONAL_SYMBOL (cuMemUnmap, CuMemUnmap);
  LOAD_OPTIONAL_SYMBOL (cuMemRetainAllocationHandle,
      CuMemRetainAllocationHandle);

  GST_INFO ("Virtual alloc symbols are loaded");

  vtable->have_virtual_alloc = TRUE;
}

static void
gst_cuda_load_library_once_func (void)
{
  GModule *module;
  const gchar *filename = CUDA_LIBNAME;
  GstNvCodecCudaVTable *vtable;

  GST_DEBUG_CATEGORY_INIT (gst_cudaloader_debug, "cudaloader", 0,
      "CUDA plugin loader");

  module = g_module_open (filename, G_MODULE_BIND_LAZY);
  if (module == nullptr) {
    GST_WARNING ("Could not open library %s, %s", filename, g_module_error ());
    return;
  }

  vtable = &gst_cuda_vtable;

  /* cuda.h */
  LOAD_SYMBOL (cuInit, CuInit);
  LOAD_SYMBOL (cuGetErrorName, CuGetErrorName);
  LOAD_SYMBOL (cuGetErrorString, CuGetErrorString);
  LOAD_SYMBOL (cuCtxCreate, CuCtxCreate);
  LOAD_SYMBOL (cuCtxDestroy, CuCtxDestroy);
  LOAD_SYMBOL (cuCtxPopCurrent, CuCtxPopCurrent);
  LOAD_SYMBOL (cuCtxPushCurrent, CuCtxPushCurrent);
  LOAD_SYMBOL (cuCtxEnablePeerAccess, CuCtxEnablePeerAccess);
  LOAD_SYMBOL (cuCtxDisablePeerAccess, CuCtxDisablePeerAccess);

  LOAD_SYMBOL (cuGraphicsMapResources, CuGraphicsMapResources);
  LOAD_SYMBOL (cuGraphicsUnmapResources, CuGraphicsUnmapResources);
  LOAD_SYMBOL (cuGraphicsResourceSetMapFlags, CuGraphicsResourceSetMapFlags);
  LOAD_SYMBOL (cuGraphicsSubResourceGetMappedArray,
      CuGraphicsSubResourceGetMappedArray);
  LOAD_SYMBOL (cuGraphicsResourceGetMappedPointer,
      CuGraphicsResourceGetMappedPointer);
  LOAD_SYMBOL (cuGraphicsUnregisterResource, CuGraphicsUnregisterResource);

  LOAD_SYMBOL (cuMemAlloc, CuMemAlloc);
  LOAD_SYMBOL (cuMemAllocPitch, CuMemAllocPitch);
  LOAD_SYMBOL (cuMemAllocHost, CuMemAllocHost);
  LOAD_SYMBOL (cuMemcpy2D, CuMemcpy2D);
  LOAD_SYMBOL (cuMemcpy2DAsync, CuMemcpy2DAsync);

  LOAD_SYMBOL (cuMemFree, CuMemFree);
  LOAD_SYMBOL (cuMemFreeHost, CuMemFreeHost);

  LOAD_SYMBOL (cuStreamCreate, CuStreamCreate);
  LOAD_SYMBOL (cuStreamDestroy, CuStreamDestroy);
  LOAD_SYMBOL (cuStreamSynchronize, CuStreamSynchronize);

  LOAD_SYMBOL (cuDeviceGet, CuDeviceGet);
  LOAD_SYMBOL (cuDeviceGetCount, CuDeviceGetCount);
  LOAD_SYMBOL (cuDeviceGetName, CuDeviceGetName);
  LOAD_SYMBOL (cuDeviceGetAttribute, CuDeviceGetAttribute);
  LOAD_SYMBOL (cuDeviceCanAccessPeer, CuDeviceCanAccessPeer);

  LOAD_SYMBOL (cuDriverGetVersion, CuDriverGetVersion);

  LOAD_SYMBOL (cuModuleLoadData, CuModuleLoadData);
  LOAD_SYMBOL (cuModuleUnload, CuModuleUnload);
  LOAD_SYMBOL (cuModuleGetFunction, CuModuleGetFunction);
  LOAD_SYMBOL (cuTexObjectCreate, CuTexObjectCreate);
  LOAD_SYMBOL (cuTexObjectDestroy, CuTexObjectDestroy);
  LOAD_SYMBOL (cuLaunchKernel, CuLaunchKernel);

  LOAD_SYMBOL (cuEventCreate, CuEventCreate);
  LOAD_SYMBOL (cuEventDestroy, CuEventDestroy);
  LOAD_SYMBOL (cuEventRecord, CuEventRecord);
  LOAD_SYMBOL (cuEventSynchronize, CuEventSynchronize);

  LOAD_SYMBOL (cuIpcGetEventHandle, CuIpcGetEventHandle);
  LOAD_SYMBOL (cuIpcOpenEventHandle, CuIpcOpenEventHandle);

  LOAD_SYMBOL (cuIpcGetMemHandle, CuIpcGetMemHandle);
  LOAD_SYMBOL (cuIpcOpenMemHandle, CuIpcOpenMemHandle);
  LOAD_SYMBOL (cuIpcCloseMemHandle, CuIpcCloseMemHandle);

  /* cudaGL.h */
  LOAD_SYMBOL (cuGraphicsGLRegisterImage, CuGraphicsGLRegisterImage);
  LOAD_SYMBOL (cuGraphicsGLRegisterBuffer, CuGraphicsGLRegisterBuffer);
  LOAD_SYMBOL (cuGLGetDevices, CuGLGetDevices);

#ifdef G_OS_WIN32
  /* cudaD3D11.h */
  LOAD_SYMBOL (cuGraphicsD3D11RegisterResource,
      CuGraphicsD3D11RegisterResource);
  LOAD_SYMBOL (cuD3D11GetDevice, CuD3D11GetDevice);
  LOAD_SYMBOL (cuD3D11GetDevices, CuD3D11GetDevices);
#endif

  vtable->loaded = TRUE;

  gst_cuda_load_optional_symbols (module);
}

/**
 * gst_cuda_load_library:
 *
 * Loads the cuda library
 *
 * Returns: %TRUE if the libcuda could be loaded %FALSE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_cuda_load_library (void)
{
  GST_CUDA_CALL_ONCE_BEGIN {
    gst_cuda_load_library_once_func ();
  } GST_CUDA_CALL_ONCE_END;

  return gst_cuda_vtable.loaded;
}

gboolean
gst_cuda_virtual_memory_symbol_loaded (void)
{
  gst_cuda_load_library ();

  return gst_cuda_vtable.have_virtual_alloc;
}

CUresult CUDAAPI
CuInit (unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuInit != nullptr);

  return gst_cuda_vtable.CuInit (Flags);
}

CUresult CUDAAPI
CuGetErrorName (CUresult error, const char **pStr)
{
  g_assert (gst_cuda_vtable.CuGetErrorName != nullptr);

  return gst_cuda_vtable.CuGetErrorName (error, pStr);
}

CUresult CUDAAPI
CuGetErrorString (CUresult error, const char **pStr)
{
  g_assert (gst_cuda_vtable.CuGetErrorString != nullptr);

  return gst_cuda_vtable.CuGetErrorString (error, pStr);
}

CUresult CUDAAPI
CuCtxCreate (CUcontext * pctx, unsigned int flags, CUdevice dev)
{
  g_assert (gst_cuda_vtable.CuCtxCreate != nullptr);

  return gst_cuda_vtable.CuCtxCreate (pctx, flags, dev);
}

CUresult CUDAAPI
CuCtxDestroy (CUcontext ctx)
{
  g_assert (gst_cuda_vtable.CuCtxDestroy != nullptr);

  return gst_cuda_vtable.CuCtxDestroy (ctx);
}

CUresult CUDAAPI
CuCtxPopCurrent (CUcontext * pctx)
{
  g_assert (gst_cuda_vtable.CuCtxPopCurrent != nullptr);

  return gst_cuda_vtable.CuCtxPopCurrent (pctx);
}

CUresult CUDAAPI
CuCtxPushCurrent (CUcontext ctx)
{
  g_assert (gst_cuda_vtable.CuCtxPushCurrent != nullptr);

  return gst_cuda_vtable.CuCtxPushCurrent (ctx);
}

CUresult CUDAAPI
CuCtxEnablePeerAccess (CUcontext peerContext, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuCtxEnablePeerAccess != nullptr);

  return gst_cuda_vtable.CuCtxEnablePeerAccess (peerContext, Flags);
}

CUresult CUDAAPI
CuCtxDisablePeerAccess (CUcontext peerContext)
{
  g_assert (gst_cuda_vtable.CuCtxDisablePeerAccess != nullptr);

  return gst_cuda_vtable.CuCtxDisablePeerAccess (peerContext);
}

CUresult CUDAAPI
CuGraphicsMapResources (unsigned int count, CUgraphicsResource * resources,
    CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuGraphicsMapResources != nullptr);

  return gst_cuda_vtable.CuGraphicsMapResources (count, resources, hStream);
}

CUresult CUDAAPI
CuGraphicsUnmapResources (unsigned int count, CUgraphicsResource * resources,
    CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuGraphicsUnmapResources != nullptr);

  return gst_cuda_vtable.CuGraphicsUnmapResources (count, resources, hStream);
}

CUresult CUDAAPI
CuGraphicsResourceSetMapFlags (CUgraphicsResource resource, unsigned int flags)
{
  g_assert (gst_cuda_vtable.CuGraphicsResourceSetMapFlags != nullptr);

  return gst_cuda_vtable.CuGraphicsResourceSetMapFlags (resource, flags);
}

CUresult CUDAAPI
CuGraphicsSubResourceGetMappedArray (CUarray * pArray,
    CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel)
{
  g_assert (gst_cuda_vtable.CuGraphicsSubResourceGetMappedArray != nullptr);

  return gst_cuda_vtable.CuGraphicsSubResourceGetMappedArray (pArray, resource,
      arrayIndex, mipLevel);
}

/* *INDENT-OFF* */
CUresult CUDAAPI
CuGraphicsResourceGetMappedPointer (CUdeviceptr * pDevPtr, size_t * pSize,
    CUgraphicsResource resource)
{
  g_assert (gst_cuda_vtable.CuGraphicsResourceGetMappedPointer != nullptr);

  return gst_cuda_vtable.CuGraphicsResourceGetMappedPointer (pDevPtr, pSize,
      resource);
}
/* *INDENT-ON* */

CUresult CUDAAPI
CuGraphicsUnregisterResource (CUgraphicsResource resource)
{
  g_assert (gst_cuda_vtable.CuGraphicsUnregisterResource != nullptr);

  return gst_cuda_vtable.CuGraphicsUnregisterResource (resource);
}

CUresult CUDAAPI
CuMemAlloc (CUdeviceptr * dptr, unsigned int bytesize)
{
  g_assert (gst_cuda_vtable.CuMemAlloc != nullptr);

  return gst_cuda_vtable.CuMemAlloc (dptr, bytesize);
}

/* *INDENT-OFF* */
CUresult CUDAAPI
CuMemAllocPitch (CUdeviceptr * dptr, size_t * pPitch, size_t WidthInBytes,
    size_t Height, unsigned int ElementSizeBytes)
{
  g_assert (gst_cuda_vtable.CuMemAllocPitch != nullptr);

  return gst_cuda_vtable.CuMemAllocPitch (dptr, pPitch, WidthInBytes, Height,
      ElementSizeBytes);
}
/* *INDENT-ON* */

CUresult CUDAAPI
CuMemAllocHost (void **pp, unsigned int bytesize)
{
  g_assert (gst_cuda_vtable.CuMemAllocHost != nullptr);

  return gst_cuda_vtable.CuMemAllocHost (pp, bytesize);
}

CUresult CUDAAPI
CuMemcpy2D (const CUDA_MEMCPY2D * pCopy)
{
  g_assert (gst_cuda_vtable.CuMemcpy2D != nullptr);

  return gst_cuda_vtable.CuMemcpy2D (pCopy);
}

CUresult CUDAAPI
CuMemcpy2DAsync (const CUDA_MEMCPY2D * pCopy, CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuMemcpy2DAsync != nullptr);

  return gst_cuda_vtable.CuMemcpy2DAsync (pCopy, hStream);
}

CUresult CUDAAPI
CuMemFree (CUdeviceptr dptr)
{
  g_assert (gst_cuda_vtable.CuMemFree != nullptr);

  return gst_cuda_vtable.CuMemFree (dptr);
}

CUresult CUDAAPI
CuMemFreeHost (void *p)
{
  g_assert (gst_cuda_vtable.CuMemFreeHost != nullptr);

  return gst_cuda_vtable.CuMemFreeHost (p);
}

CUresult CUDAAPI
CuStreamCreate (CUstream * phStream, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuStreamCreate != nullptr);

  return gst_cuda_vtable.CuStreamCreate (phStream, Flags);
}

CUresult CUDAAPI
CuStreamDestroy (CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuStreamDestroy != nullptr);

  return gst_cuda_vtable.CuStreamDestroy (hStream);
}

CUresult CUDAAPI
CuStreamSynchronize (CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuStreamSynchronize != nullptr);

  return gst_cuda_vtable.CuStreamSynchronize (hStream);
}

CUresult CUDAAPI
CuDeviceGet (CUdevice * device, int ordinal)
{
  g_assert (gst_cuda_vtable.CuDeviceGet != nullptr);

  return gst_cuda_vtable.CuDeviceGet (device, ordinal);
}

CUresult CUDAAPI
CuDeviceGetCount (int *count)
{
  g_assert (gst_cuda_vtable.CuDeviceGetCount != nullptr);

  return gst_cuda_vtable.CuDeviceGetCount (count);
}

CUresult CUDAAPI
CuDeviceGetName (char *name, int len, CUdevice dev)
{
  g_assert (gst_cuda_vtable.CuDeviceGetName != nullptr);

  return gst_cuda_vtable.CuDeviceGetName (name, len, dev);
}

CUresult CUDAAPI
CuDeviceGetAttribute (int *pi, CUdevice_attribute attrib, CUdevice dev)
{
  g_assert (gst_cuda_vtable.CuDeviceGetAttribute != nullptr);

  return gst_cuda_vtable.CuDeviceGetAttribute (pi, attrib, dev);
}

CUresult CUDAAPI
CuDeviceCanAccessPeer (int *canAccessPeer, CUdevice dev, CUdevice peerDev)
{
  g_assert (gst_cuda_vtable.CuDeviceCanAccessPeer != nullptr);

  return gst_cuda_vtable.CuDeviceCanAccessPeer (canAccessPeer, dev, peerDev);
}

CUresult CUDAAPI
CuDriverGetVersion (int *driverVersion)
{
  g_assert (gst_cuda_vtable.CuDriverGetVersion != nullptr);

  return gst_cuda_vtable.CuDriverGetVersion (driverVersion);
}

CUresult CUDAAPI
CuModuleLoadData (CUmodule * module, const void *image)
{
  g_assert (gst_cuda_vtable.CuModuleLoadData != nullptr);

  return gst_cuda_vtable.CuModuleLoadData (module, image);
}

CUresult CUDAAPI
CuModuleUnload (CUmodule module)
{
  g_assert (gst_cuda_vtable.CuModuleUnload != nullptr);

  return gst_cuda_vtable.CuModuleUnload (module);
}

CUresult CUDAAPI
CuModuleGetFunction (CUfunction * hfunc, CUmodule hmod, const char *name)
{
  g_assert (gst_cuda_vtable.CuModuleGetFunction != nullptr);

  return gst_cuda_vtable.CuModuleGetFunction (hfunc, hmod, name);
}

CUresult CUDAAPI
CuTexObjectCreate (CUtexObject * pTexObject,
    const CUDA_RESOURCE_DESC * pResDesc, const CUDA_TEXTURE_DESC * pTexDesc,
    const CUDA_RESOURCE_VIEW_DESC * pResViewDesc)
{
  g_assert (gst_cuda_vtable.CuTexObjectCreate != nullptr);

  return gst_cuda_vtable.CuTexObjectCreate (pTexObject, pResDesc, pTexDesc,
      pResViewDesc);
}

CUresult CUDAAPI
CuTexObjectDestroy (CUtexObject texObject)
{
  g_assert (gst_cuda_vtable.CuTexObjectDestroy != nullptr);

  return gst_cuda_vtable.CuTexObjectDestroy (texObject);
}

CUresult CUDAAPI
CuLaunchKernel (CUfunction f, unsigned int gridDimX,
    unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
    unsigned int sharedMemBytes, CUstream hStream, void **kernelParams,
    void **extra)
{
  g_assert (gst_cuda_vtable.CuLaunchKernel != nullptr);

  return gst_cuda_vtable.CuLaunchKernel (f, gridDimX, gridDimY, gridDimZ,
      blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams,
      extra);
}

CUresult CUDAAPI
CuEventCreate (CUevent * phEvent, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuEventCreate);

  return gst_cuda_vtable.CuEventCreate (phEvent, Flags);
}

CUresult CUDAAPI
CuEventDestroy (CUevent hEvent)
{
  g_assert (gst_cuda_vtable.CuEventDestroy);

  return gst_cuda_vtable.CuEventDestroy (hEvent);
}

CUresult CUDAAPI
CuEventRecord (CUevent hEvent, CUstream hStream)
{
  g_assert (gst_cuda_vtable.CuEventRecord);

  return gst_cuda_vtable.CuEventRecord (hEvent, hStream);
}

CUresult CUDAAPI
CuEventSynchronize (CUevent hEvent)
{
  g_assert (gst_cuda_vtable.CuEventSynchronize);

  return gst_cuda_vtable.CuEventSynchronize (hEvent);
}

CUresult CUDAAPI
CuIpcGetEventHandle (CUipcEventHandle * pHandle, CUevent event)
{
  g_assert (gst_cuda_vtable.CuIpcGetEventHandle);

  return gst_cuda_vtable.CuIpcGetEventHandle (pHandle, event);
}

CUresult CUDAAPI
CuIpcOpenEventHandle (CUevent * phEvent, CUipcEventHandle handle)
{
  g_assert (gst_cuda_vtable.CuIpcOpenEventHandle);

  return gst_cuda_vtable.CuIpcOpenEventHandle (phEvent, handle);
}

CUresult CUDAAPI
CuIpcGetMemHandle (CUipcMemHandle * pHandle, CUdeviceptr dptr)
{
  g_assert (gst_cuda_vtable.CuIpcGetMemHandle);

  return gst_cuda_vtable.CuIpcGetMemHandle (pHandle, dptr);
}

CUresult CUDAAPI
CuIpcOpenMemHandle (CUdeviceptr * pdptr, CUipcMemHandle handle,
    unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuIpcOpenMemHandle);

  return gst_cuda_vtable.CuIpcOpenMemHandle (pdptr, handle, Flags);
}

CUresult CUDAAPI
CuIpcCloseMemHandle (CUdeviceptr dptr)
{
  g_assert (gst_cuda_vtable.CuIpcCloseMemHandle);

  return gst_cuda_vtable.CuIpcCloseMemHandle (dptr);
}

CUresult CUDAAPI
CuMemAddressReserve (CUdeviceptr * ptr, size_t size, size_t alignment,
    CUdeviceptr addr, unsigned long long flags)
{
  if (!gst_cuda_vtable.CuMemAddressReserve)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemAddressReserve (ptr,
      size, alignment, addr, flags);
}

CUresult CUDAAPI
CuMemAddressFree (CUdeviceptr ptr, size_t size)
{
  if (!gst_cuda_vtable.CuMemAddressFree)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemAddressFree (ptr, size);
}

CUresult CUDAAPI
CuMemCreate (CUmemGenericAllocationHandle * handle, size_t size,
    const CUmemAllocationProp * prop, unsigned long long flags)
{
  if (!gst_cuda_vtable.CuMemCreate)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemCreate (handle, size, prop, flags);
}

CUresult CUDAAPI
CuMemRelease (CUmemGenericAllocationHandle handle)
{
  if (!gst_cuda_vtable.CuMemRelease)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemRelease (handle);
}

CUresult CUDAAPI
CuMemExportToShareableHandle (void *shareableHandle,
    CUmemGenericAllocationHandle handle, CUmemAllocationHandleType handleType,
    unsigned long long flags)
{
  if (!gst_cuda_vtable.CuMemExportToShareableHandle)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemExportToShareableHandle (shareableHandle,
      handle, handleType, flags);
}

CUresult CUDAAPI
CuMemImportFromShareableHandle (CUmemGenericAllocationHandle * handle,
    void *osHandle, CUmemAllocationHandleType shHandleType)
{
  if (!gst_cuda_vtable.CuMemImportFromShareableHandle)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemImportFromShareableHandle (handle,
      osHandle, shHandleType);
}

CUresult CUDAAPI
CuMemSetAccess (CUdeviceptr ptr, size_t size, const CUmemAccessDesc * desc,
    size_t count)
{
  if (!gst_cuda_vtable.CuMemSetAccess)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemSetAccess (ptr, size, desc, count);
}

CUresult CUDAAPI
CuMemGetAccess (unsigned long long *flags, const CUmemLocation * location,
    CUdeviceptr ptr)
{
  if (!gst_cuda_vtable.CuMemGetAccess)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemGetAccess (flags, location, ptr);
}

CUresult CUDAAPI
CuMemGetAllocationGranularity (size_t *granularity,
    const CUmemAllocationProp * prop, CUmemAllocationGranularity_flags option)
{
  if (!gst_cuda_vtable.CuMemGetAllocationGranularity)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemGetAllocationGranularity (granularity,
      prop, option);
}

CUresult CUDAAPI
CuMemGetAllocationPropertiesFromHandle (CUmemAllocationProp * prop,
    CUmemGenericAllocationHandle handle)
{
  if (!gst_cuda_vtable.CuMemGetAllocationPropertiesFromHandle)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemGetAllocationPropertiesFromHandle (prop, handle);
}


CUresult CUDAAPI
CuMemMap (CUdeviceptr ptr, size_t size, size_t offset,
    CUmemGenericAllocationHandle handle, unsigned long long flags)
{
  if (!gst_cuda_vtable.CuMemMap)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemMap (ptr, size, offset, handle, flags);
}

CUresult CUDAAPI
CuMemUnmap (CUdeviceptr ptr, size_t size)
{
  if (!gst_cuda_vtable.CuMemUnmap)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemUnmap (ptr, size);
}

CUresult CUDAAPI
CuMemRetainAllocationHandle (CUmemGenericAllocationHandle * handle, void *addr)
{
  if (!gst_cuda_vtable.CuMemRetainAllocationHandle)
    return CUDA_ERROR_NOT_SUPPORTED;

  return gst_cuda_vtable.CuMemRetainAllocationHandle (handle, addr);
}

/* cudaGL.h */
CUresult CUDAAPI
CuGraphicsGLRegisterImage (CUgraphicsResource * pCudaResource,
    unsigned int image, unsigned int target, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuGraphicsGLRegisterImage != nullptr);

  return gst_cuda_vtable.CuGraphicsGLRegisterImage (pCudaResource, image,
      target, Flags);
}

CUresult CUDAAPI
CuGraphicsGLRegisterBuffer (CUgraphicsResource * pCudaResource,
    unsigned int buffer, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuGraphicsGLRegisterBuffer != nullptr);

  return gst_cuda_vtable.CuGraphicsGLRegisterBuffer (pCudaResource, buffer,
      Flags);
}

CUresult CUDAAPI
CuGLGetDevices (unsigned int *pCudaDeviceCount, CUdevice * pCudaDevices,
    unsigned int cudaDeviceCount, CUGLDeviceList deviceList)
{
  g_assert (gst_cuda_vtable.CuGLGetDevices != nullptr);

  return gst_cuda_vtable.CuGLGetDevices (pCudaDeviceCount, pCudaDevices,
      cudaDeviceCount, deviceList);
}

/* cudaD3D11.h */
#ifdef G_OS_WIN32
CUresult CUDAAPI
CuGraphicsD3D11RegisterResource (CUgraphicsResource * pCudaResource,
    ID3D11Resource * pD3DResource, unsigned int Flags)
{
  g_assert (gst_cuda_vtable.CuGraphicsD3D11RegisterResource != nullptr);

  return gst_cuda_vtable.CuGraphicsD3D11RegisterResource (pCudaResource,
      pD3DResource, Flags);
}

CUresult CUDAAPI
CuD3D11GetDevice (CUdevice * device, IDXGIAdapter * pAdapter)
{
  g_assert (gst_cuda_vtable.CuD3D11GetDevice != nullptr);

  return gst_cuda_vtable.CuD3D11GetDevice (device, pAdapter);
}

CUresult CUDAAPI
CuD3D11GetDevices (unsigned int *pCudaDeviceCount,
    CUdevice * pCudaDevices, unsigned int cudaDeviceCount,
    ID3D11Device * pD3D11Device, CUd3d11DeviceList deviceList)
{
  g_assert (gst_cuda_vtable.CuD3D11GetDevices != nullptr);

  return gst_cuda_vtable.CuD3D11GetDevices (pCudaDeviceCount, pCudaDevices,
      cudaDeviceCount, pD3D11Device, deviceList);
}
#endif
