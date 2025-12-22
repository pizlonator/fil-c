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
#include "gstcudanvrtc.h"
#include "gstcudaloader.h"
#include <nvrtc.h>
#include <gmodule.h>
#include "gstcuda-private.h"
#include <string>

GST_DEBUG_CATEGORY_STATIC (gst_cuda_nvrtc_debug);
#define GST_CAT_DEFAULT gst_cuda_nvrtc_debug

#ifndef G_OS_WIN32
#define NVRTC_LIBNAME "libnvrtc.so"
#else
#define NVRTC_LIBNAME "nvrtc64_%d%d_0.dll"
#endif

#define LOAD_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_ERROR ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    goto error; \
  } \
} G_STMT_END;

#define LOAD_OPTIONAL_SYMBOL(name,func) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->func)) { \
    GST_WARNING ("Failed to load '%s', %s", G_STRINGIFY (name), g_module_error()); \
    return TRUE; \
  } \
} G_STMT_END;


/* *INDENT-OFF* */
typedef struct _GstCudaNvrtcVTable
{
  gboolean loaded;
  gboolean have_cubin;

  nvrtcResult (*NvrtcCompileProgram) (nvrtcProgram prog, int numOptions,
      const char **options);
  nvrtcResult (*NvrtcCreateProgram) (nvrtcProgram * prog, const char *src,
      const char *name, int numHeaders, const char **headers,
      const char **includeNames);
  nvrtcResult (*NvrtcDestroyProgram) (nvrtcProgram * prog);
  nvrtcResult (*NvrtcGetPTX) (nvrtcProgram prog, char *ptx);
  nvrtcResult (*NvrtcGetPTXSize) (nvrtcProgram prog, size_t * ptxSizeRet);
  nvrtcResult (*NvrtcGetProgramLog) (nvrtcProgram prog, char *log);
  nvrtcResult (*NvrtcGetProgramLogSize) (nvrtcProgram prog,
      size_t * logSizeRet);
  nvrtcResult (*NvrtcGetCUBINSize) (nvrtcProgram prog, size_t *cubinSizeRet);
  nvrtcResult (*NvrtcGetCUBIN) (nvrtcProgram prog, char *cubin);
} GstCudaNvrtcVTable;
/* *INDENT-ON* */

static GstCudaNvrtcVTable gst_cuda_nvrtc_vtable = { 0, };

#ifdef G_OS_WIN32
static GModule *
gst_cuda_nvrtc_load_library_once_win32 (void)
{
  gchar *dll_name = nullptr;
  GModule *module = nullptr;
  gint cuda_version;
  gint cuda_major_version;
  gint cuda_minor_version;
  gint major, minor;
  CUresult rst;

  rst = CuDriverGetVersion (&cuda_version);
  if (rst != CUDA_SUCCESS) {
    GST_WARNING ("Couldn't get driver version, 0x%x", (guint) rst);
    return nullptr;
  }

  cuda_major_version = cuda_version / 1000;
  cuda_minor_version = (cuda_version % 1000) / 10;

  GST_INFO ("CUDA version %d / %d", cuda_major_version, cuda_minor_version);

  /* First path for searching nvrtc library using system CUDA version */
  for (minor = cuda_minor_version; minor >= 0; minor--) {
    g_clear_pointer (&dll_name, g_free);
    dll_name = g_strdup_printf (NVRTC_LIBNAME, cuda_major_version, minor);
    module = g_module_open (dll_name, G_MODULE_BIND_LAZY);
    if (module) {
      GST_INFO ("%s is available", dll_name);
      g_free (dll_name);
      return module;
    }

    GST_DEBUG ("Couldn't open library %s", dll_name);
  }

  /* CUDA is a part for driever installation, but nvrtc library is a part of
   * CUDA-toolkit. So CUDA-toolkit version may be lower than
   * CUDA version. Do search the dll again */
  for (major = cuda_major_version; major >= 9; major--) {
    for (minor = 5; minor >= 0; minor--) {
      g_clear_pointer (&dll_name, g_free);
      dll_name = g_strdup_printf (NVRTC_LIBNAME, major, minor);
      module = g_module_open (dll_name, G_MODULE_BIND_LAZY);
      if (module) {
        GST_INFO ("%s is available", dll_name);
        g_free (dll_name);
        return module;
      }

      GST_DEBUG ("Couldn't open library %s", dll_name);
    }
  }

  g_free (dll_name);

  return nullptr;
}
#endif

static gboolean
gst_cuda_nvrtc_load_library_once (void)
{
  GModule *module = nullptr;
  const gchar *filename_env;
  GstCudaNvrtcVTable *vtable;

  filename_env = g_getenv ("GST_CUDA_NVRTC_LIBNAME");
  if (filename_env)
    module = g_module_open (filename_env, G_MODULE_BIND_LAZY);

  if (!module) {
#ifndef G_OS_WIN32
    module = g_module_open (NVRTC_LIBNAME, G_MODULE_BIND_LAZY);
#else
    module = gst_cuda_nvrtc_load_library_once_win32 ();
#endif
  }

  if (module == nullptr) {
    GST_WARNING ("Could not open nvrtc library %s", g_module_error ());
    return FALSE;
  }

  vtable = &gst_cuda_nvrtc_vtable;
  vtable->have_cubin = FALSE;

  LOAD_SYMBOL (nvrtcCompileProgram, NvrtcCompileProgram);
  LOAD_SYMBOL (nvrtcCreateProgram, NvrtcCreateProgram);
  LOAD_SYMBOL (nvrtcDestroyProgram, NvrtcDestroyProgram);
  LOAD_SYMBOL (nvrtcGetPTX, NvrtcGetPTX);
  LOAD_SYMBOL (nvrtcGetPTXSize, NvrtcGetPTXSize);
  LOAD_SYMBOL (nvrtcGetProgramLog, NvrtcGetProgramLog);
  LOAD_SYMBOL (nvrtcGetProgramLogSize, NvrtcGetProgramLogSize);
  vtable->loaded = TRUE;

  LOAD_OPTIONAL_SYMBOL (nvrtcGetCUBINSize, NvrtcGetCUBINSize);
  LOAD_OPTIONAL_SYMBOL (nvrtcGetCUBIN, NvrtcGetCUBIN);
  vtable->have_cubin = TRUE;

  return TRUE;

error:
  g_module_close (module);

  return FALSE;
}

/**
 * gst_cuda_nvrtc_load_library:
 *
 * Loads the nvrtc library.
 *
 * Returns: %TRUE if the library could be loaded, %FALSE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_cuda_nvrtc_load_library (void)
{
  GST_CUDA_CALL_ONCE_BEGIN {
    GST_DEBUG_CATEGORY_INIT (gst_cuda_nvrtc_debug, "cudanvrtc", 0,
        "CUDA runtime compiler");
    if (gst_cuda_load_library ())
      gst_cuda_nvrtc_load_library_once ();
  }
  GST_CUDA_CALL_ONCE_END;

  return gst_cuda_nvrtc_vtable.loaded;
}

/* *INDENT-OFF* */
static nvrtcResult
NvrtcCompileProgram (nvrtcProgram prog, int numOptions, const char **options)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcCompileProgram != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcCompileProgram (prog, numOptions, options);
}

static nvrtcResult
NvrtcCreateProgram (nvrtcProgram * prog, const char *src, const char *name,
    int numHeaders, const char **headers, const char **includeNames)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcCreateProgram != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcCreateProgram (prog, src, name, numHeaders,
      headers, includeNames);
}

static nvrtcResult
NvrtcDestroyProgram (nvrtcProgram * prog)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcDestroyProgram != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcDestroyProgram (prog);
}

static nvrtcResult
NvrtcGetPTX (nvrtcProgram prog, char *ptx)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetPTX != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetPTX (prog, ptx);
}

static nvrtcResult
NvrtcGetPTXSize (nvrtcProgram prog, size_t *ptxSizeRet)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetPTXSize != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetPTXSize (prog, ptxSizeRet);
}

static nvrtcResult
NvrtcGetProgramLog (nvrtcProgram prog, char *log)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetProgramLog != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetProgramLog (prog, log);
}

static nvrtcResult
NvrtcGetProgramLogSize (nvrtcProgram prog, size_t *logSizeRet)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetProgramLogSize != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetProgramLogSize (prog, logSizeRet);
}

static nvrtcResult
NvrtcGetCUBINSize (nvrtcProgram prog, size_t *cubinSizeRet)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetCUBINSize != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetCUBINSize (prog, cubinSizeRet);
}

static nvrtcResult
NvrtcGetCUBIN (nvrtcProgram prog, char *cubin)
{
  g_assert (gst_cuda_nvrtc_vtable.NvrtcGetCUBIN != nullptr);

  return gst_cuda_nvrtc_vtable.NvrtcGetCUBIN (prog, cubin);
}
/* *INDENT-ON* */

/**
 * gst_cuda_nvrtc_compile:
 * @source: Source code to compile
 *
 * Since: 1.22
 */
gchar *
gst_cuda_nvrtc_compile (const gchar * source)
{
  nvrtcProgram prog;
  nvrtcResult ret;
  CUresult curet;
  const gchar *opts[] = { "--gpu-architecture=compute_30" };
  gsize ptx_size;
  gchar *ptx = nullptr;
  int driverVersion;

  g_return_val_if_fail (source != nullptr, nullptr);

  if (!gst_cuda_nvrtc_load_library ()) {
    return nullptr;
  }

  GST_TRACE ("CUDA kernel source \n%s", source);

  curet = CuDriverGetVersion (&driverVersion);
  if (curet != CUDA_SUCCESS) {
    GST_ERROR ("Failed to query CUDA Driver version, ret %d", curet);
    return nullptr;
  }

  GST_DEBUG ("CUDA Driver Version %d.%d", driverVersion / 1000,
      (driverVersion % 1000) / 10);

  ret = NvrtcCreateProgram (&prog, source, nullptr, 0, nullptr, nullptr);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("couldn't create nvrtc program, ret %d", ret);
    return nullptr;
  }

  /* Starting from CUDA 11, the lowest supported architecture is 5.2 */
  if (driverVersion >= 11000)
    opts[0] = "--gpu-architecture=compute_52";

  ret = NvrtcCompileProgram (prog, 1, opts);
  if (ret != NVRTC_SUCCESS) {
    gsize log_size;

    GST_ERROR ("couldn't compile nvrtc program, ret %d", ret);
    if (NvrtcGetProgramLogSize (prog, &log_size) == NVRTC_SUCCESS &&
        log_size > 0) {
      gchar *compile_log = (gchar *) g_alloca (log_size);
      if (NvrtcGetProgramLog (prog, compile_log) == NVRTC_SUCCESS) {
        GST_ERROR ("nvrtc compile log %s", compile_log);
      }
    }

    goto error;
  }

  ret = NvrtcGetPTXSize (prog, &ptx_size);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("unknown ptx size, ret %d", ret);

    goto error;
  }

  ptx = (gchar *) g_malloc0 (ptx_size);
  ret = NvrtcGetPTX (prog, ptx);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("couldn't get ptx, ret %d", ret);
    g_free (ptx);

    goto error;
  }

  NvrtcDestroyProgram (&prog);

  GST_TRACE ("compiled CUDA PTX %s\n", ptx);

  return ptx;

error:
  NvrtcDestroyProgram (&prog);

  return nullptr;
}

/**
 * gst_cuda_nvrtc_compile_cubin:
 * @source: Source code to compile
 * @device: CUDA device
 *
 * Returns: (transfer full): Compiled CUDA assembly code if successful,
 * otherwise %NULL
 *
 * Since: 1.24
 */
gchar *
gst_cuda_nvrtc_compile_cubin (const gchar * source, gint device)
{
  nvrtcProgram prog;
  nvrtcResult ret;
  CUresult curet;
  gsize cubin_size;
  gchar *cubin = nullptr;
  gint major, minor;

  g_return_val_if_fail (source != nullptr, nullptr);

  if (!gst_cuda_nvrtc_load_library ())
    return nullptr;

  if (!gst_cuda_nvrtc_vtable.have_cubin) {
    GST_DEBUG ("CUBIN related symbols are unavailable");
    return nullptr;
  }

  GST_TRACE ("CUDA kernel source \n%s", source);

  curet = CuDeviceGetAttribute (&major,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
  if (curet != CUDA_SUCCESS) {
    GST_ERROR ("Unknown major compute caps");
    return nullptr;
  }

  curet = CuDeviceGetAttribute (&minor,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);
  if (curet != CUDA_SUCCESS) {
    GST_ERROR ("Unknown minor compute caps");
    return nullptr;
  }

  std::string opt_str = "--gpu-architecture=sm_" +
      std::to_string (major) + std::to_string (minor);

  ret = NvrtcCreateProgram (&prog, source, nullptr, 0, nullptr, nullptr);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("couldn't create nvrtc program, ret %d", ret);
    return nullptr;
  }

  const char *opts[1] = { opt_str.c_str () };

  ret = NvrtcCompileProgram (prog, 1, opts);
  if (ret != NVRTC_SUCCESS) {
    gsize log_size;

    GST_ERROR ("couldn't compile nvrtc program, ret %d", ret);
    if (NvrtcGetProgramLogSize (prog, &log_size) == NVRTC_SUCCESS &&
        log_size > 0) {
      gchar *compile_log = (gchar *) g_alloca (log_size);
      if (NvrtcGetProgramLog (prog, compile_log) == NVRTC_SUCCESS) {
        GST_ERROR ("nvrtc compile log %s", compile_log);
      }
    }

    goto error;
  }

  ret = NvrtcGetCUBINSize (prog, &cubin_size);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("unknown ptx size, ret %d", ret);
    goto error;
  }

  cubin = (gchar *) g_malloc0 (cubin_size);
  ret = NvrtcGetCUBIN (prog, cubin);
  if (ret != NVRTC_SUCCESS) {
    GST_ERROR ("couldn't get ptx, ret %d", ret);
    g_free (cubin);
    goto error;
  }

  NvrtcDestroyProgram (&prog);

  return cubin;

error:
  NvrtcDestroyProgram (&prog);

  return nullptr;
}
