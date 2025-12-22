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

#include "gstcuvidloader.h"
#include <gmodule.h>

#ifdef G_OS_WIN32
#define NVCUVID_LIBNAME "nvcuvid.dll"
#else
#define NVCUVID_LIBNAME "libnvcuvid.so.1"
#endif

#define LOAD_SYMBOL(name,func,mandatory) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    if (mandatory) { \
      GST_ERROR ("Failed to load '%s' from %s, %s", G_STRINGIFY (name), filename, g_module_error()); \
      goto error; \
    } \
    GST_WARNING ("Failed to load '%s' from %s, %s", G_STRINGIFY (name), filename, g_module_error()); \
  } \
} G_STMT_END;

typedef struct _GstnvdecCuvidVTable
{
  gboolean loaded;

  guint major_version;
  guint minor_version;

    CUresult (CUDAAPI * CuvidCtxLockCreate) (CUvideoctxlock * pLock,
      CUcontext ctx);
    CUresult (CUDAAPI * CuvidCtxLockDestroy) (CUvideoctxlock lck);
    CUresult (CUDAAPI * CuvidCtxLock) (CUvideoctxlock lck,
      unsigned int reserved_flags);
    CUresult (CUDAAPI * CuvidCtxUnlock) (CUvideoctxlock lck,
      unsigned int reserved_flags);
    CUresult (CUDAAPI * CuvidCreateDecoder) (CUvideodecoder * phDecoder,
      CUVIDDECODECREATEINFO * pdci);
    CUresult (CUDAAPI * CuvidReconfigureDecoder) (CUvideodecoder phDecoder,
      CUVIDRECONFIGUREDECODERINFO * pDecReconfigParams);
    CUresult (CUDAAPI * CuvidDestroyDecoder) (CUvideodecoder hDecoder);
    CUresult (CUDAAPI * CuvidDecodePicture) (CUvideodecoder hDecoder,
      CUVIDPICPARAMS * pPicParams);
    CUresult (CUDAAPI * CuvidCreateVideoParser) (CUvideoparser * pObj,
      CUVIDPARSERPARAMS * pParams);
    CUresult (CUDAAPI * CuvidParseVideoData) (CUvideoparser obj,
      CUVIDSOURCEDATAPACKET * pPacket);
    CUresult (CUDAAPI * CuvidDestroyVideoParser) (CUvideoparser obj);
    CUresult (CUDAAPI * CuvidMapVideoFrame) (CUvideodecoder hDecoder,
      int nPicIdx, guintptr * pDevPtr, unsigned int *pPitch,
      CUVIDPROCPARAMS * pVPP);
    CUresult (CUDAAPI * CuvidUnmapVideoFrame) (CUvideodecoder hDecoder,
      guintptr DevPtr);
    CUresult (CUDAAPI * CuvidGetDecoderCaps) (CUVIDDECODECAPS * pdc);
} GstnvdecCuvidVTable;

static GstnvdecCuvidVTable gst_cuvid_vtable = { 0, };

gboolean
gst_cuvid_load_library (guint api_major_ver, guint api_minor_ver)
{
  GModule *module;
  const gchar *filename = NVCUVID_LIBNAME;
  GstnvdecCuvidVTable *vtable;

  if (gst_cuvid_vtable.loaded)
    return TRUE;

  module = g_module_open (filename, G_MODULE_BIND_LAZY);
  if (module == NULL) {
    GST_WARNING ("Could not open library %s, %s", filename, g_module_error ());
    return FALSE;
  }

  vtable = &gst_cuvid_vtable;

  LOAD_SYMBOL (cuvidCtxLockCreate, CuvidCtxLockCreate, TRUE);
  LOAD_SYMBOL (cuvidCtxLockDestroy, CuvidCtxLockDestroy, TRUE);
  LOAD_SYMBOL (cuvidCtxLock, CuvidCtxLock, TRUE);
  LOAD_SYMBOL (cuvidCtxUnlock, CuvidCtxUnlock, TRUE);
  LOAD_SYMBOL (cuvidCreateDecoder, CuvidCreateDecoder, TRUE);
  LOAD_SYMBOL (cuvidReconfigureDecoder, CuvidReconfigureDecoder, FALSE);
  LOAD_SYMBOL (cuvidDestroyDecoder, CuvidDestroyDecoder, TRUE);
  LOAD_SYMBOL (cuvidDecodePicture, CuvidDecodePicture, TRUE);
  LOAD_SYMBOL (cuvidCreateVideoParser, CuvidCreateVideoParser, TRUE);
  LOAD_SYMBOL (cuvidParseVideoData, CuvidParseVideoData, TRUE);
  LOAD_SYMBOL (cuvidDestroyVideoParser, CuvidDestroyVideoParser, TRUE);
  LOAD_SYMBOL (cuvidMapVideoFrame, CuvidMapVideoFrame, TRUE);
  LOAD_SYMBOL (cuvidUnmapVideoFrame, CuvidUnmapVideoFrame, TRUE);
  LOAD_SYMBOL (cuvidGetDecoderCaps, CuvidGetDecoderCaps, FALSE);

  vtable->loaded = TRUE;
  vtable->major_version = api_major_ver;
  vtable->minor_version = api_minor_ver;

  return TRUE;

error:
  g_module_close (module);

  return FALSE;
}

gboolean
gst_cuvid_get_api_version (guint * api_major_ver, guint * api_minor_ver)
{
  if (!gst_cuvid_vtable.loaded)
    return FALSE;

  if (api_major_ver)
    *api_major_ver = gst_cuvid_vtable.major_version;

  if (api_minor_ver)
    *api_minor_ver = gst_cuvid_vtable.minor_version;

  return TRUE;
}

gboolean
gst_cuvid_can_get_decoder_caps (void)
{
  if (gst_cuvid_vtable.CuvidGetDecoderCaps)
    return TRUE;

  return FALSE;
}

gboolean
gst_cuvid_can_reconfigure (void)
{
  if (gst_cuvid_vtable.CuvidReconfigureDecoder)
    return TRUE;

  return FALSE;
}

CUresult CUDAAPI
CuvidCtxLockCreate (CUvideoctxlock * pLock, CUcontext ctx)
{
  g_assert (gst_cuvid_vtable.CuvidCtxLockCreate != NULL);

  return gst_cuvid_vtable.CuvidCtxLockCreate (pLock, ctx);
}

CUresult CUDAAPI
CuvidCtxLockDestroy (CUvideoctxlock lck)
{
  g_assert (gst_cuvid_vtable.CuvidCtxLockDestroy != NULL);

  return gst_cuvid_vtable.CuvidCtxLockDestroy (lck);
}

CUresult CUDAAPI
CuvidCtxLock (CUvideoctxlock lck, unsigned int reserved_flags)
{
  g_assert (gst_cuvid_vtable.CuvidCtxLock != NULL);

  return gst_cuvid_vtable.CuvidCtxLock (lck, reserved_flags);
}

CUresult CUDAAPI
CuvidCtxUnlock (CUvideoctxlock lck, unsigned int reserved_flags)
{
  g_assert (gst_cuvid_vtable.CuvidCtxLockDestroy != NULL);

  return gst_cuvid_vtable.CuvidCtxUnlock (lck, reserved_flags);
}

CUresult CUDAAPI
CuvidCreateDecoder (CUvideodecoder * phDecoder, CUVIDDECODECREATEINFO * pdci)
{
  g_assert (gst_cuvid_vtable.CuvidCreateDecoder != NULL);

  return gst_cuvid_vtable.CuvidCreateDecoder (phDecoder, pdci);
}

CUresult CUDAAPI
CuvidReconfigureDecoder (CUvideodecoder hDecoder,
    CUVIDRECONFIGUREDECODERINFO * pDecReconfigParams)
{
  g_assert (gst_cuvid_vtable.CuvidReconfigureDecoder != NULL);

  return gst_cuvid_vtable.CuvidReconfigureDecoder (hDecoder,
      pDecReconfigParams);
}

CUresult CUDAAPI
CuvidDestroyDecoder (CUvideodecoder hDecoder)
{
  g_assert (gst_cuvid_vtable.CuvidDestroyDecoder != NULL);

  return gst_cuvid_vtable.CuvidDestroyDecoder (hDecoder);
}

CUresult CUDAAPI
CuvidDecodePicture (CUvideodecoder hDecoder, CUVIDPICPARAMS * pPicParams)
{
  g_assert (gst_cuvid_vtable.CuvidDecodePicture != NULL);

  return gst_cuvid_vtable.CuvidDecodePicture (hDecoder, pPicParams);
}

CUresult CUDAAPI
CuvidCreateVideoParser (CUvideoparser * pObj, CUVIDPARSERPARAMS * pParams)
{
  g_assert (gst_cuvid_vtable.CuvidCreateVideoParser != NULL);

  return gst_cuvid_vtable.CuvidCreateVideoParser (pObj, pParams);
}

CUresult CUDAAPI
CuvidParseVideoData (CUvideoparser obj, CUVIDSOURCEDATAPACKET * pPacket)
{
  g_assert (gst_cuvid_vtable.CuvidParseVideoData != NULL);

  return gst_cuvid_vtable.CuvidParseVideoData (obj, pPacket);
}

CUresult CUDAAPI
CuvidDestroyVideoParser (CUvideoparser obj)
{
  g_assert (gst_cuvid_vtable.CuvidDestroyVideoParser != NULL);

  return gst_cuvid_vtable.CuvidDestroyVideoParser (obj);
}

CUresult CUDAAPI
CuvidMapVideoFrame (CUvideodecoder hDecoder, int nPicIdx,
    guintptr * pDevPtr, unsigned int *pPitch, CUVIDPROCPARAMS * pVPP)
{
  g_assert (gst_cuvid_vtable.CuvidMapVideoFrame != NULL);

  return gst_cuvid_vtable.CuvidMapVideoFrame (hDecoder, nPicIdx, pDevPtr,
      pPitch, pVPP);
}

CUresult CUDAAPI
CuvidUnmapVideoFrame (CUvideodecoder hDecoder, guintptr DevPtr)
{
  g_assert (gst_cuvid_vtable.CuvidUnmapVideoFrame != NULL);

  return gst_cuvid_vtable.CuvidUnmapVideoFrame (hDecoder, DevPtr);
}

CUresult CUDAAPI
CuvidGetDecoderCaps (CUVIDDECODECAPS * pdc)
{
  g_assert (gst_cuvid_vtable.CuvidGetDecoderCaps != NULL);

  return gst_cuvid_vtable.CuvidGetDecoderCaps (pdc);
}
