/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel Corporation
 * Copyright (c) 2018, Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGDECE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gstmsdkcontext.h"
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <va/va_drm.h>
#include <gudev/gudev.h>
#include <gst/va/gstvadisplay_drm.h>
#else
#include <gst/d3d11/gstd3d11.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_debug_msdkcontext);
#define GST_CAT_DEFAULT gst_debug_msdkcontext

struct _GstMsdkContextPrivate
{
  MsdkSession session;
  GstBufferPool *alloc_pool;
  GList *cached_alloc_responses;
  gboolean hardware;
  gboolean has_frame_allocator;
  GstMsdkContextJobType job_type;
  gint shared_async_depth;
  GMutex mutex;
  GList *child_session_list;
  GstMsdkContext *parent_context;
#ifndef _WIN32
  GstVaDisplay *display;
#else
  GstD3D11Device *device;
#endif
};

#define gst_msdk_context_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstMsdkContext, gst_msdk_context, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstMsdkContext)
    GST_DEBUG_CATEGORY_INIT (gst_debug_msdkcontext, "msdkcontext", 0,
        "MSDK Context"));

#ifndef _WIN32

static char *
get_device_path (void)
{
  GUdevClient *client = NULL;
  GUdevEnumerator *e = NULL;
  GList *devices, *l;
  GUdevDevice *dev, *parent;
  const gchar *devnode_path;
  const gchar *devnode_files[2] = { "renderD[0-9]*", "card[0-9]*" };
  int fd = -1, i;
  const gchar *user_choice = g_getenv ("GST_MSDK_DRM_DEVICE");
  gchar *ret_path = NULL;

  if (user_choice) {
    if (g_str_has_prefix (user_choice, "/dev/dri/"))
      fd = open (user_choice, O_RDWR | O_CLOEXEC);

    if (fd >= 0) {
      drmVersionPtr drm_version = drmGetVersion (fd);

      if (!drm_version || strncmp (drm_version->name, "i915", 4)) {
        GST_ERROR ("The specified device isn't an Intel device");
        drmFreeVersion (drm_version);
        close (fd);
        fd = -1;
      } else {
        GST_DEBUG ("Opened the specified drm device %s", user_choice);
        drmFreeVersion (drm_version);
      }
    } else {
      GST_ERROR ("The specified device isn't a valid drm device");
    }

    if (fd >= 0) {
      ret_path = g_strdup (user_choice);
      close (fd);
    }

    return ret_path;
  }

  client = g_udev_client_new (NULL);
  if (!client)
    goto done;

  e = g_udev_enumerator_new (client);
  if (!e)
    goto done;

  g_udev_enumerator_add_match_subsystem (e, "drm");
  for (i = 0; i < 2; i++) {
    g_udev_enumerator_add_match_name (e, devnode_files[i]);
    devices = g_udev_enumerator_execute (e);

    for (l = devices; l != NULL; l = l->next) {
      dev = (GUdevDevice *) l->data;

      parent = g_udev_device_get_parent (dev);
      if (g_strcmp0 (g_udev_device_get_subsystem (parent), "pci") != 0 ||
          g_strcmp0 (g_udev_device_get_driver (parent), "i915") != 0) {
        g_object_unref (parent);
        continue;
      }
      g_object_unref (parent);

      devnode_path = g_udev_device_get_device_file (dev);
      fd = open (devnode_path, O_RDWR | O_CLOEXEC);
      if (fd < 0)
        continue;
      GST_DEBUG ("Opened the drm device node %s", devnode_path);
      ret_path = g_strdup (devnode_path);
      break;
    }

    g_list_foreach (devices, (GFunc) gst_object_unref, NULL);
    g_list_free (devices);
    if (fd >= 0)
      goto done;
  }

done:
  if (fd >= 0)
    close (fd);

  if (e)
    g_object_unref (e);
  if (client)
    g_object_unref (client);

  return ret_path;
}

static gboolean
gst_msdk_context_use_vaapi (GstMsdkContext * context)
{
  char *path;
  GstVaDisplay *display_drm = NULL;
  GstMsdkContextPrivate *priv = context->priv;

  path = get_device_path ();
  if (path == NULL) {
    GST_WARNING ("Couldn't find a drm device node to open");
    return FALSE;
  }

  display_drm = gst_va_display_drm_new_from_path (path);
  g_free (path);

  if (!display_drm) {
    GST_ERROR ("Couldn't create a VA DRM display");
    return FALSE;
  }

  priv->display = display_drm;

  return TRUE;
}
#else
static GstD3D11Device *
get_device_by_index (IDXGIFactory1 * factory, guint idx)
{
  HRESULT hr;
  IDXGIAdapter1 *adapter;
  ID3D11Device *device_handle;
  ID3D10Multithread *multi_thread;
  DXGI_ADAPTER_DESC desc;
  GstD3D11Device *device = NULL;
  gint64 luid;

  hr = IDXGIFactory1_EnumAdapters1 (factory, idx, &adapter);
  if (FAILED (hr)) {
    return NULL;
  }

  hr = IDXGIAdapter1_GetDesc (adapter, &desc);
  if (FAILED (hr)) {
    IDXGIAdapter1_Release (adapter);
    return NULL;
  }

  if (desc.VendorId != 0x8086) {
    IDXGIAdapter1_Release (adapter);
    return NULL;
  }

  luid = gst_d3d11_luid_to_int64 (&desc.AdapterLuid);
  device = gst_d3d11_device_new_for_adapter_luid (luid,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT);
  IDXGIAdapter1_Release (adapter);

  device_handle = gst_d3d11_device_get_device_handle (device);
  hr = ID3D11Device_QueryInterface (device_handle,
      &IID_ID3D10Multithread, (void **) &multi_thread);
  if (FAILED (hr)) {
    gst_object_unref (device);
    return NULL;
  }

  hr = ID3D10Multithread_SetMultithreadProtected (multi_thread, TRUE);
  ID3D10Multithread_Release (multi_thread);

  return device;
}

static gboolean
gst_msdk_context_use_d3d11 (GstMsdkContext * context)
{
  HRESULT hr;
  IDXGIFactory1 *factory = NULL;
  GstD3D11Device *device = NULL;
  ID3D11Device *device_handle;
  GstMsdkContextPrivate *priv = context->priv;
  mfxStatus status;
  guint idx = 0;
  gint user_idx = -1;
  const gchar *user_choice = g_getenv ("GST_MSDK_DEVICE");

  hr = CreateDXGIFactory1 (&IID_IDXGIFactory1, (void **) &factory);
  if (FAILED (hr)) {
    GST_ERROR ("Couldn't create DXGI factory");
    return FALSE;
  }

  if (user_choice) {
    user_idx = atoi (user_choice);
    if (!(device = get_device_by_index (factory, user_idx)))
      GST_WARNING
          ("Failed to get device by user index, try to pick the first available device");
  }

  /* Pick the first available device */
  while (!device) {
    device = get_device_by_index (factory, idx++);
  }

  IDXGIFactory1_Release (factory);
  device_handle = gst_d3d11_device_get_device_handle (device);

  status =
      MFXVideoCORE_SetHandle (priv->session.session, MFX_HANDLE_D3D11_DEVICE,
      gst_d3d11_device_get_device_handle (device));
  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Setting D3D11VA handle failed (%s)",
        msdk_status_to_string (status));
    gst_object_unref (device);
    return FALSE;
  }

  priv->device = device;

  return TRUE;
}
#endif

static gboolean
gst_msdk_context_open (GstMsdkContext * context, gboolean hardware)
{
  mfxU16 codename;
  GstMsdkContextPrivate *priv = context->priv;
  MsdkSession msdk_session;
  mfxIMPL impl;
  mfxHDL handle = NULL;
#ifndef _WIN32
  mfxStatus status;
#endif

  priv->hardware = hardware;

  impl = hardware ? MFX_IMPL_HARDWARE_ANY : MFX_IMPL_SOFTWARE;

#ifdef _WIN32
  impl |= MFX_IMPL_VIA_D3D11;
#endif

#ifndef _WIN32
  if (hardware) {
    if (!gst_msdk_context_use_vaapi (context))
      return FALSE;

    handle = (mfxHDL) gst_va_display_get_va_dpy (priv->display);
  }
#endif

  msdk_session = msdk_open_session (handle, impl);
  if (!msdk_session.session)
    return FALSE;

  priv->session = msdk_session;

  if (hardware) {
#ifndef _WIN32
    status = MFXVideoCORE_SetHandle (priv->session.session,
        MFX_HANDLE_VA_DISPLAY, handle);
    if (status != MFX_ERR_NONE) {
      GST_ERROR ("Setting VAAPI handle failed (%s)",
          msdk_status_to_string (status));
      return FALSE;
    }
#else
    if (!gst_msdk_context_use_d3d11 (context))
      return FALSE;
#endif
  }

  codename = msdk_get_platform_codename (priv->session.session);

  if (codename != MFX_PLATFORM_UNKNOWN)
    GST_INFO ("Detected MFX platform with device code %d", codename);
  else
    GST_WARNING ("Unknown MFX platform");

  return TRUE;
}

static void
gst_msdk_context_init (GstMsdkContext * context)
{
  GstMsdkContextPrivate *priv = gst_msdk_context_get_instance_private (context);

  context->priv = priv;

  g_mutex_init (&priv->mutex);
}

static void
release_child_session (gpointer session)
{
  mfxStatus status;

  mfxSession _session = session;
  status = MFXDisjoinSession (_session);
  if (status != MFX_ERR_NONE)
    GST_WARNING ("failed to disjoin (%s)", msdk_status_to_string (status));
  msdk_close_mfx_session (_session);
}

static void
gst_msdk_context_finalize (GObject * obj)
{
  GstMsdkContext *context = GST_MSDK_CONTEXT_CAST (obj);
  GstMsdkContextPrivate *priv = context->priv;

  /* child sessions will be closed when the parent session is closed */
  if (priv->parent_context) {
    gst_object_unref (priv->parent_context);
    goto done;
  } else
    g_list_free_full (priv->child_session_list, release_child_session);

  msdk_close_session (&priv->session);
  g_mutex_clear (&priv->mutex);

#ifndef _WIN32
  if (priv->display)
    gst_object_unref (priv->display);
#else
  if (priv->device)
    gst_object_unref (priv->device);
#endif

done:
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_msdk_context_class_init (GstMsdkContextClass * klass)
{
  GObjectClass *const g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gst_msdk_context_finalize;
}

GstMsdkContext *
gst_msdk_context_new (gboolean hardware)
{
  GstMsdkContext *obj = g_object_new (GST_TYPE_MSDK_CONTEXT, NULL);
  gst_object_ref_sink (obj);

  if (obj && !gst_msdk_context_open (obj, hardware)) {
    gst_object_unref (obj);
    return NULL;
  }

  return obj;
}

GstMsdkContext *
gst_msdk_context_new_with_job_type (gboolean hardware,
    GstMsdkContextJobType job_type)
{
  GstMsdkContext *obj = gst_msdk_context_new (hardware);

  if (obj)
    obj->priv->job_type = job_type;

  return obj;
}

GstMsdkContext *
gst_msdk_context_new_with_parent (GstMsdkContext * parent)
{
  mfxStatus status;
  GstMsdkContext *obj;
  GstMsdkContextPrivate *priv;
  GstMsdkContextPrivate *parent_priv;
  mfxVersion version;
  mfxIMPL impl;
  MsdkSession child_msdk_session;
  mfxHandleType handle_type = 0;
  mfxHDL handle = NULL, hardware_handle = NULL;

  g_return_val_if_fail (GST_IS_MSDK_CONTEXT (parent), NULL);

  parent_priv = parent->priv;
  status = MFXQueryIMPL (parent_priv->session.session, &impl);

  if (status == MFX_ERR_NONE)
    status = MFXQueryVersion (parent_priv->session.session, &version);

  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Failed to query the session attributes (%s)",
        msdk_status_to_string (status));
    return NULL;
  }

  if (MFX_IMPL_VIA_VAAPI == (0x0f00 & (impl)))
    handle_type = MFX_HANDLE_VA_DISPLAY;
  else if (MFX_IMPL_VIA_D3D11 == (0x0f00 & (impl)))
    handle_type = MFX_HANDLE_D3D11_DEVICE;

  if (handle_type) {
    status =
        MFXVideoCORE_GetHandle (parent_priv->session.session, handle_type,
        &handle);

    if (status != MFX_ERR_NONE || !handle) {
      GST_ERROR ("Failed to get session handle (%s)",
          msdk_status_to_string (status));
      return NULL;
    }
  }

  child_msdk_session.loader = parent_priv->session.loader;
  child_msdk_session.session = NULL;
#ifndef _WIN32
  hardware_handle = (mfxHDL) gst_va_display_get_va_dpy (parent_priv->display);
#endif

  status = msdk_init_msdk_session (hardware_handle, impl, &version,
      &child_msdk_session);

  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Failed to create a child mfx session (%s)",
        msdk_status_to_string (status));
    return NULL;
  }

  if (handle) {
    status =
        MFXVideoCORE_SetHandle (child_msdk_session.session, handle_type,
        handle);

    if (status != MFX_ERR_NONE) {
      GST_ERROR ("Failed to set a HW handle (%s)",
          msdk_status_to_string (status));
      MFXClose (child_msdk_session.session);
      return NULL;
    }
  }
#if (MFX_VERSION >= 1025)
  status =
      MFXJoinSession (parent_priv->session.session, child_msdk_session.session);

  if (status != MFX_ERR_NONE) {
    GST_ERROR ("Failed to join two sessions (%s)",
        msdk_status_to_string (status));
    MFXClose (child_msdk_session.session);
    return NULL;
  }
#endif

  obj = g_object_new (GST_TYPE_MSDK_CONTEXT, NULL);
  gst_object_ref_sink (obj);
  priv = obj->priv;

  /* Set loader to NULL for child session */
  priv->session.loader = NULL;
  priv->session.session = child_msdk_session.session;
  priv->hardware = parent_priv->hardware;
  priv->job_type = parent_priv->job_type;
  parent_priv->child_session_list =
      g_list_prepend (parent_priv->child_session_list, priv->session.session);
#ifndef _WIN32
  priv->display = parent_priv->display;
#else
  priv->device = parent_priv->device;
#endif
  priv->parent_context = gst_object_ref (parent);

  return obj;
}

#ifndef _WIN32
GstMsdkContext *
gst_msdk_context_new_with_va_display (GstObject * display_obj,
    gboolean hardware, GstMsdkContextJobType job_type)
{
  GstMsdkContext *obj = NULL;

  GstMsdkContextPrivate *priv;
  mfxU16 codename;
  mfxStatus status;
  GstVaDisplay *va_display;
  mfxHDL handle;

  va_display = GST_VA_DISPLAY (display_obj);
  if (!va_display)
    return NULL;

  obj = g_object_new (GST_TYPE_MSDK_CONTEXT, NULL);
  gst_object_ref_sink (obj);

  priv = obj->priv;
  priv->display = gst_object_ref (va_display);

  priv->job_type = job_type;
  priv->hardware = hardware;

  handle = (mfxHDL) gst_va_display_get_va_dpy (priv->display);
  priv->session = msdk_open_session (handle,
      hardware ? MFX_IMPL_HARDWARE_ANY : MFX_IMPL_SOFTWARE);
  if (!priv->session.session) {
    gst_object_unref (obj);
    return NULL;
  }

  if (hardware) {
    status =
        MFXVideoCORE_SetHandle (priv->session.session, MFX_HANDLE_VA_DISPLAY,
        handle);
    if (status != MFX_ERR_NONE) {
      GST_ERROR ("Setting VAAPI handle failed (%s)",
          msdk_status_to_string (status));
      gst_object_unref (obj);
      return NULL;
    }
  }

  codename = msdk_get_platform_codename (priv->session.session);

  if (codename != MFX_PLATFORM_UNKNOWN)
    GST_INFO ("Detected MFX platform with device code %d", codename);
  else
    GST_WARNING ("Unknown MFX platform");

  return obj;
}
#else
GstMsdkContext *
gst_msdk_context_new_with_d3d11_device (GstD3D11Device * device,
    gboolean hardware, GstMsdkContextJobType job_type)
{
  GstMsdkContext *obj = NULL;
  GstMsdkContextPrivate *priv;
  mfxU16 codename;
  mfxStatus status;
  ID3D10Multithread *multi_thread;
  ID3D11Device *device_handle;
  HRESULT hr;

  obj = g_object_new (GST_TYPE_MSDK_CONTEXT, NULL);
  gst_object_ref_sink (obj);

  priv = obj->priv;
  priv->device = gst_object_ref (device);

  priv->job_type = job_type;
  priv->hardware = hardware;
  priv->session = msdk_open_session (NULL,
      hardware ? MFX_IMPL_HARDWARE_ANY : MFX_IMPL_SOFTWARE);
  if (!priv->session.session) {
    goto failed;
  }

  device_handle = gst_d3d11_device_get_device_handle (device);
  hr = ID3D11Device_QueryInterface (device_handle,
      &IID_ID3D10Multithread, (void **) &multi_thread);
  if (FAILED (hr)) {
    GST_ERROR ("ID3D10Multithread interface is unavailable");
    goto failed;
  }

  hr = ID3D10Multithread_SetMultithreadProtected (multi_thread, TRUE);
  ID3D10Multithread_Release (multi_thread);

  if (hardware) {
    status =
        MFXVideoCORE_SetHandle (priv->session.session, MFX_HANDLE_D3D11_DEVICE,
        device_handle);
    if (status != MFX_ERR_NONE) {
      GST_ERROR ("Setting D3D11VA handle failed (%s)",
          msdk_status_to_string (status));
      goto failed;
    }
  }

  codename = msdk_get_platform_codename (priv->session.session);

  if (codename != MFX_PLATFORM_UNKNOWN)
    GST_INFO ("Detected MFX platform with device code %d", codename);
  else
    GST_WARNING ("Unknown MFX platform");

  return obj;

failed:
  gst_object_unref (obj);
  gst_object_unref (device);
  return NULL;
}
#endif

mfxSession
gst_msdk_context_get_session (GstMsdkContext * context)
{
  return context->priv->session.session;
}

const mfxLoader *
gst_msdk_context_get_loader (GstMsdkContext * context)
{
  return &context->priv->session.loader;
}

mfxU32
gst_msdk_context_get_impl_idx (GstMsdkContext * context)
{
  return context->priv->session.impl_idx;
}

gpointer
gst_msdk_context_get_handle (GstMsdkContext * context)
{
#ifndef _WIN32
  return gst_va_display_get_va_dpy (context->priv->display);
#else
  return NULL;
#endif
}

#ifndef _WIN32
GstObject *
gst_msdk_context_get_va_display (GstMsdkContext * context)
{
  if (context->priv->display)
    return gst_object_ref (GST_OBJECT_CAST (context->priv->display));
  return NULL;
}
#else
GstD3D11Device *
gst_msdk_context_get_d3d11_device (GstMsdkContext * context)
{
  if (context->priv->device)
    return gst_object_ref (context->priv->device);
  return NULL;
}
#endif

static gint
_find_response (gconstpointer resp, gconstpointer comp_resp)
{
  GstMsdkAllocResponse *cached_resp = (GstMsdkAllocResponse *) resp;
  mfxFrameAllocResponse *_resp = (mfxFrameAllocResponse *) comp_resp;

  return cached_resp ? cached_resp->response.mids != _resp->mids : -1;
}

static inline gboolean
_requested_frame_size_is_equal_or_lower (mfxFrameAllocRequest * _req,
    GstMsdkAllocResponse * cached_resp)
{
  if (((_req->Type & MFX_MEMTYPE_EXPORT_FRAME) &&
          _req->Info.Width == cached_resp->request.Info.Width &&
          _req->Info.Height == cached_resp->request.Info.Height) ||
      (!(_req->Type & MFX_MEMTYPE_EXPORT_FRAME) &&
          _req->Info.Width <= cached_resp->request.Info.Width &&
          _req->Info.Height <= cached_resp->request.Info.Height))
    return TRUE;

  return FALSE;
}

static gint
_find_request (gconstpointer resp, gconstpointer req)
{
  GstMsdkAllocResponse *cached_resp = (GstMsdkAllocResponse *) resp;
  mfxFrameAllocRequest *_req = (mfxFrameAllocRequest *) req;

  /* Confirm if it's under the size of the cached response */
  if (_req->NumFrameSuggested <= cached_resp->request.NumFrameSuggested &&
      _requested_frame_size_is_equal_or_lower (_req, cached_resp))
    return _req->Type & cached_resp->
        request.Type & MFX_MEMTYPE_FROM_DECODE ? 0 : -1;

  return -1;
}

GstMsdkAllocResponse *
gst_msdk_context_get_cached_alloc_responses (GstMsdkContext * context,
    mfxFrameAllocResponse * resp)
{
  GstMsdkContextPrivate *priv = context->priv;
  GList *l =
      g_list_find_custom (priv->cached_alloc_responses, resp, _find_response);

  if (l)
    return l->data;
  else
    return NULL;
}

GstMsdkAllocResponse *
gst_msdk_context_get_cached_alloc_responses_by_request (GstMsdkContext *
    context, mfxFrameAllocRequest * req)
{
  GstMsdkContextPrivate *priv = context->priv;
  GList *l =
      g_list_find_custom (priv->cached_alloc_responses, req, _find_request);

  if (l)
    return l->data;
  else
    return NULL;
}

void
gst_msdk_context_add_alloc_response (GstMsdkContext * context,
    GstMsdkAllocResponse * resp)
{
  context->priv->cached_alloc_responses =
      g_list_prepend (context->priv->cached_alloc_responses, resp);
}

gboolean
gst_msdk_context_remove_alloc_response (GstMsdkContext * context,
    mfxFrameAllocResponse * resp)
{
  GstMsdkAllocResponse *msdk_resp;
  GstMsdkContextPrivate *priv = context->priv;
  GList *l =
      g_list_find_custom (priv->cached_alloc_responses, resp, _find_response);

  if (!l)
    return FALSE;

  msdk_resp = l->data;

  g_slice_free1 (sizeof (GstMsdkAllocResponse), msdk_resp);
  priv->cached_alloc_responses =
      g_list_delete_link (priv->cached_alloc_responses, l);

  return TRUE;
}

void
gst_msdk_context_set_alloc_pool (GstMsdkContext * context, GstBufferPool * pool)
{
  context->priv->alloc_pool = gst_object_ref (pool);
}

GstBufferPool *
gst_msdk_context_get_alloc_pool (GstMsdkContext * context)
{
  return context->priv->alloc_pool;
}

GstMsdkContextJobType
gst_msdk_context_get_job_type (GstMsdkContext * context)
{
  return context->priv->job_type;
}

void
gst_msdk_context_set_job_type (GstMsdkContext * context,
    GstMsdkContextJobType job_type)
{
  context->priv->job_type = job_type;
}

void
gst_msdk_context_add_job_type (GstMsdkContext * context,
    GstMsdkContextJobType job_type)
{
  context->priv->job_type |= job_type;
}

gint
gst_msdk_context_get_shared_async_depth (GstMsdkContext * context)
{
  return context->priv->shared_async_depth;
}

void
gst_msdk_context_add_shared_async_depth (GstMsdkContext * context,
    gint async_depth)
{
  context->priv->shared_async_depth += async_depth;
}

void
gst_msdk_context_set_frame_allocator (GstMsdkContext * context,
    mfxFrameAllocator * allocator)
{
  GstMsdkContextPrivate *priv = context->priv;

  g_mutex_lock (&priv->mutex);

  if (!priv->has_frame_allocator) {
    mfxStatus status;

    status = MFXVideoCORE_SetFrameAllocator (priv->session.session, allocator);

    if (status != MFX_ERR_NONE)
      GST_ERROR ("Failed to set frame allocator");
    else
      priv->has_frame_allocator = 1;
  }

  g_mutex_unlock (&priv->mutex);
}
