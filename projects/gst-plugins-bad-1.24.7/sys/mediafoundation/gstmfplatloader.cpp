/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstmfplatloader.h"
#include "gstmfconfig.h"
#include <gmodule.h>

GST_DEBUG_CATEGORY_EXTERN (gst_mf_debug);
#define GST_CAT_DEFAULT gst_mf_debug

/* *INDENT-OFF* */
#define LOAD_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_WARNING ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    goto out; \
  } \
} G_STMT_END;

typedef struct _GstMFPlatVTable
{
  gboolean loaded;

  HRESULT (__stdcall * GstMFTEnum2) (GUID guidCategory,
                                     UINT32 Flags,
                                     const MFT_REGISTER_TYPE_INFO * pInputType,
                                     const MFT_REGISTER_TYPE_INFO * pOutputType,
                                     IMFAttributes * pAttributes,
                                     IMFActivate *** pppMFTActivate,
                                     UINT32 * pnumMFTActivate);

  HRESULT (__stdcall * GstMFCreateDXGIDeviceManager) (UINT * resetToken,
                                                      IMFDXGIDeviceManager ** ppDeviceManager);

  HRESULT (__stdcall * GstMFCreateVideoSampleAllocatorEx) (REFIID riid,
                                                           void** ppSampleAllocator);
} GstMFPlatVTable;
/* *INDENT-ON* */

static GstMFPlatVTable gst_mf_plat_vtable = { 0, };

static gboolean
load_library_once (void)
{
  static gsize load_once = 0;
  if (g_once_init_enter (&load_once)) {
#if GST_MF_HAVE_D3D11
    GModule *module;
    GstMFPlatVTable *vtable = &gst_mf_plat_vtable;

    module = g_module_open ("mfplat.dll", G_MODULE_BIND_LAZY);
    if (!module)
      goto out;

    LOAD_SYMBOL (MFTEnum2, GstMFTEnum2);
    LOAD_SYMBOL (MFCreateDXGIDeviceManager, GstMFCreateDXGIDeviceManager);
    LOAD_SYMBOL (MFCreateVideoSampleAllocatorEx,
        GstMFCreateVideoSampleAllocatorEx);

    vtable->loaded = TRUE;
#endif

  out:
    g_once_init_leave (&load_once, 1);
  }

  return gst_mf_plat_vtable.loaded;
}

gboolean
gst_mf_plat_load_library (void)
{
  return load_library_once ();
}

HRESULT __stdcall
GstMFTEnum2 (GUID guidCategory, UINT32 Flags,
    const MFT_REGISTER_TYPE_INFO * pInputType,
    const MFT_REGISTER_TYPE_INFO * pOutputType,
    IMFAttributes * pAttributes, IMFActivate *** pppMFTActivate,
    UINT32 * pnumMFTActivate)
{
  g_assert (gst_mf_plat_vtable.GstMFTEnum2 != nullptr);

  return gst_mf_plat_vtable.GstMFTEnum2 (guidCategory, Flags, pInputType,
      pOutputType, pAttributes, pppMFTActivate, pnumMFTActivate);
}

HRESULT __stdcall
GstMFCreateDXGIDeviceManager (UINT * resetToken,
    IMFDXGIDeviceManager ** ppDeviceManager)
{
  g_assert (gst_mf_plat_vtable.GstMFCreateDXGIDeviceManager != nullptr);

  return gst_mf_plat_vtable.GstMFCreateDXGIDeviceManager (resetToken,
      ppDeviceManager);
}

HRESULT __stdcall
GstMFCreateVideoSampleAllocatorEx (REFIID riid, void **ppSampleAllocator)
{
  g_assert (gst_mf_plat_vtable.GstMFCreateVideoSampleAllocatorEx != nullptr);

  return gst_mf_plat_vtable.GstMFCreateVideoSampleAllocatorEx (riid,
      ppSampleAllocator);
}
