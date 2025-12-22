/* CUDA stub header
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

#ifndef __GST_CUDA_STUB_H__
#define __GST_CUDA_STUB_H__

#include <glib.h>

G_BEGIN_DECLS

typedef gpointer CUcontext;
typedef gpointer CUgraphicsResource;
typedef gpointer CUstream;
typedef gpointer CUarray;
typedef gpointer CUmodule;
typedef gpointer CUfunction;
typedef gpointer CUmipmappedArray;
typedef gpointer CUevent;

typedef guint64  CUtexObject;
typedef guintptr CUdeviceptr;
typedef gint CUdevice;

typedef enum
{
  CUDA_SUCCESS = 0,
  CUDA_ERROR_NO_DEVICE = 100,
  CUDA_ERROR_ALREADY_MAPPED = 208,
  CUDA_ERROR_NOT_SUPPORTED = 801,
} CUresult;

typedef enum
{
  CU_MEMORYTYPE_HOST = 1,
  CU_MEMORYTYPE_DEVICE = 2,
  CU_MEMORYTYPE_ARRAY = 3,
  CU_MEMORYTYPE_UNIFIED = 4,
} CUmemorytype;

typedef enum
{
  CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,
  CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76,
  CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED = 102,
  CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR_SUPPORTED = 103,
  CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_WIN32_HANDLE_SUPPORTED = 104,
  CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_WIN32_KMT_HANDLE_SUPPORTED = 105,
} CUdevice_attribute;

typedef enum
{
  CU_GRAPHICS_REGISTER_FLAGS_NONE = 0x00,
  CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY = 0x01,
  CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 0x02,
  CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LOAD_STORE = 0x04,
  CU_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER = 0x08,
} CUgraphicsRegisterFlags;

typedef enum
{
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE = 0x00,
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
  CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} CUgraphicsMapResourceFlags;

typedef enum
{
  CU_STREAM_DEFAULT = 0x0,
  CU_STREAM_NON_BLOCKING = 0x1
} CUstream_flags;

typedef enum
{
  CU_TR_FILTER_MODE_POINT = 0,
  CU_TR_FILTER_MODE_LINEAR = 1
} CUfilter_mode;

typedef enum
{
  CU_TR_ADDRESS_MODE_WRAP = 0,
  CU_TR_ADDRESS_MODE_CLAMP = 1,
  CU_TR_ADDRESS_MODE_MIRROR = 2,
  CU_TR_ADDRESS_MODE_BORDER = 3
} CUaddress_mode;

typedef enum
{
  CU_RESOURCE_TYPE_ARRAY = 0,
  CU_RESOURCE_TYPE_MIPMAPPED_ARRAY = 1,
  CU_RESOURCE_TYPE_LINEAR = 2,
  CU_RESOURCE_TYPE_PITCH2D = 3
} CUresourcetype;

typedef enum
{
  CU_AD_FORMAT_UNSIGNED_INT8  = 1,
  CU_AD_FORMAT_UNSIGNED_INT16 = 2,
} CUarray_format;

typedef enum
{
  CU_RES_VIEW_FORMAT_NONE = 0,
} CUresourceViewFormat;

typedef enum
{
  CU_EVENT_DEFAULT = 0x0,
  CU_EVENT_BLOCKING_SYNC = 0x1,
  CU_EVENT_DISABLE_TIMING = 0x2,
  CU_EVENT_INTERPROCESS = 0x4,
} CUevent_flags;

typedef struct
{
  gsize srcXInBytes;
  gsize srcY;
  CUmemorytype srcMemoryType;
  gconstpointer srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  gsize srcPitch;

  gsize dstXInBytes;
  gsize dstY;
  CUmemorytype dstMemoryType;
  gpointer dstHost;
  CUdeviceptr dstDevice;
  CUarray dstArray;
  gsize dstPitch;

  gsize WidthInBytes;
  gsize Height;
} CUDA_MEMCPY2D;

typedef struct
{
  CUaddress_mode addressMode[3];
  CUfilter_mode filterMode;
  guint flags;
  guint maxAnisotropy;
  CUfilter_mode mipmapFilterMode;
  gfloat mipmapLevelBias;
  gfloat minMipmapLevelClamp;
  gfloat maxMipmapLevelClamp;
  gfloat borderColor[4];
  gint reserved[12];
} CUDA_TEXTURE_DESC;

typedef struct
{
  CUresourcetype resType;

  union {
    struct {
      CUarray hArray;
    } array;
    struct {
      CUmipmappedArray hMipmappedArray;
    } mipmap;
    struct {
      CUdeviceptr devPtr;
      CUarray_format format;
      guint numChannels;
      gsize sizeInBytes;
    } linear;
    struct {
      CUdeviceptr devPtr;
      CUarray_format format;
      guint numChannels;
      gsize width;
      gsize height;
      gsize pitchInBytes;
    } pitch2D;
    struct {
        gint reserved[32];
    } reserved;
  } res;

  guint flags;
} CUDA_RESOURCE_DESC;

typedef struct
{
  CUresourceViewFormat format;
  gsize width;
  gsize height;
  gsize depth;
  guint firstMipmapLevel;
  guint lastMipmapLevel;
  guint firstLayer;
  guint lastLayer;
  guint reserved[16];
} CUDA_RESOURCE_VIEW_DESC;

typedef enum
{
  CU_IPC_MEM_LAZY_ENABLE_PEER_ACCESS = 0x1
} CUipcMem_flags;

#define CU_IPC_HANDLE_SIZE 64
typedef struct
{
  char reserved[CU_IPC_HANDLE_SIZE];
} CUipcMemHandle;

typedef struct
{
  char reserved[CU_IPC_HANDLE_SIZE];
} CUipcEventHandle;

typedef unsigned long long CUmemGenericAllocationHandle;

typedef enum
{
  CU_MEM_HANDLE_TYPE_NONE = 0x0,
  CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR = 0x1,
  CU_MEM_HANDLE_TYPE_WIN32 = 0x2,
  CU_MEM_HANDLE_TYPE_WIN32_KMT = 0x4,
  CU_MEM_HANDLE_TYPE_MAX = 0x7FFFFFFF
} CUmemAllocationHandleType;

typedef enum
{
  CU_MEM_ACCESS_FLAGS_PROT_NONE = 0x0,
  CU_MEM_ACCESS_FLAGS_PROT_READ  = 0x1,
  CU_MEM_ACCESS_FLAGS_PROT_READWRITE = 0x3,
  CU_MEM_ACCESS_FLAGS_PROT_MAX = 0x7FFFFFFF
} CUmemAccess_flags;

typedef enum
{
  CU_MEM_LOCATION_TYPE_INVALID = 0x0,
  CU_MEM_LOCATION_TYPE_DEVICE = 0x1,
  CU_MEM_LOCATION_TYPE_MAX = 0x7FFFFFFF
} CUmemLocationType;

typedef enum CUmemAllocationType_enum {
  CU_MEM_ALLOCATION_TYPE_INVALID = 0x0,
  CU_MEM_ALLOCATION_TYPE_PINNED = 0x1,
  CU_MEM_ALLOCATION_TYPE_MAX = 0x7FFFFFFF
} CUmemAllocationType;

typedef enum
{
  CU_MEM_ALLOC_GRANULARITY_MINIMUM = 0x0,
  CU_MEM_ALLOC_GRANULARITY_RECOMMENDED = 0x1
} CUmemAllocationGranularity_flags;

typedef struct
{
  CUmemLocationType type;
  int id;
} CUmemLocation;

typedef struct
{
  unsigned char compressionType;
  unsigned char gpuDirectRDMACapable;
  unsigned short usage;
  unsigned char reserved[4];
} CUmemAllocationPropAllocFlags;

typedef struct
{
  CUmemAllocationType type;
  CUmemAllocationHandleType requestedHandleTypes;
  CUmemLocation location;
  void *win32HandleMetaData;
  CUmemAllocationPropAllocFlags allocFlags;
} CUmemAllocationProp;

typedef struct
{
  CUmemLocation location;
  CUmemAccess_flags flags;
} CUmemAccessDesc;

#define CUDA_VERSION 10000

#ifdef _WIN32
#define CUDAAPI __stdcall
#else
#define CUDAAPI
#endif

#define cuCtxCreate cuCtxCreate_v2
#define cuCtxDestroy cuCtxDestroy_v2
#define cuCtxPopCurrent cuCtxPopCurrent_v2
#define cuCtxPushCurrent cuCtxPushCurrent_v2
#define cuGraphicsResourceGetMappedPointer cuGraphicsResourceGetMappedPointer_v2
#define cuGraphicsResourceSetMapFlags cuGraphicsResourceSetMapFlags_v2

#define cuStreamDestroy cuStreamDestroy_v2

#define cuMemAlloc cuMemAlloc_v2
#define cuMemAllocPitch cuMemAllocPitch_v2
#define cuMemAllocHost  cuMemAllocHost_v2
#define cuMemcpy2D cuMemcpy2D_v2
#define cuMemcpy2DAsync cuMemcpy2DAsync_v2
#define cuMemFree cuMemFree_v2

#define cuEventDestroy cuEventDestroy_v2

#define CU_TRSF_READ_AS_INTEGER 1

G_END_DECLS

#endif /* __GST_CUDA_STUB_H__ */
