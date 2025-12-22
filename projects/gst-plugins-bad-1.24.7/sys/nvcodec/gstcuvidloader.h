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

#ifndef __GST_CUVID_LOADER_H__
#define __GST_CUVID_LOADER_H__

#include <gst/gst.h>
#include <gst/cuda/gstcuda.h>
#include "nvcuvid.h"

G_BEGIN_DECLS

/* cuvid.h */
gboolean gst_cuvid_load_library     (guint api_major_ver,
                                     guint api_minor_ver);

gboolean gst_cuvid_get_api_version  (guint * api_major_ver,
                                     guint * api_minor_ver);

gboolean gst_cuvid_can_get_decoder_caps (void);

gboolean gst_cuvid_can_reconfigure (void);

CUresult CUDAAPI CuvidCtxLockCreate         (CUvideoctxlock * pLock,
                                             CUcontext ctx);

CUresult CUDAAPI CuvidCtxLockDestroy        (CUvideoctxlock lck);

CUresult CUDAAPI CuvidCtxLock               (CUvideoctxlock lck,
                                             unsigned int reserved_flags);

CUresult CUDAAPI CuvidCtxUnlock             (CUvideoctxlock lck,
                                             unsigned int reserved_flags);

CUresult CUDAAPI CuvidCreateDecoder         (CUvideodecoder * phDecoder,
                                             CUVIDDECODECREATEINFO * pdci);

CUresult CUDAAPI CuvidReconfigureDecoder    (CUvideodecoder hDecoder,
                                             CUVIDRECONFIGUREDECODERINFO * pDecReconfigParams);

CUresult CUDAAPI CuvidDestroyDecoder        (CUvideodecoder hDecoder);

CUresult CUDAAPI CuvidDecodePicture         (CUvideodecoder hDecoder,
                                             CUVIDPICPARAMS * pPicParams);

CUresult CUDAAPI CuvidCreateVideoParser     (CUvideoparser * pObj,
                                             CUVIDPARSERPARAMS * pParams);

CUresult CUDAAPI CuvidParseVideoData        (CUvideoparser obj,
                                             CUVIDSOURCEDATAPACKET * pPacket);

CUresult CUDAAPI CuvidDestroyVideoParser    (CUvideoparser obj);

CUresult CUDAAPI CuvidMapVideoFrame         (CUvideodecoder hDecoder,
                                             int nPicIdx,
                                             guintptr * pDevPtr,
                                             unsigned int *pPitch,
                                             CUVIDPROCPARAMS * pVPP);

CUresult CUDAAPI CuvidUnmapVideoFrame       (CUvideodecoder hDecoder,
                                             guintptr DevPtr);

CUresult CUDAAPI CuvidGetDecoderCaps        (CUVIDDECODECAPS * pdc);

G_END_DECLS
#endif /* __GST_CUVID_LOADER_H__ */
