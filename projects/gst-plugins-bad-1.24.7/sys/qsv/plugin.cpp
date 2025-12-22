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

/**
 * SECTION:plugin-qsv
 *
 * Intel Quick Sync plugin.
 *
 * This plugin consists of various video encoder and decoder elements.
 * Depending on the hardware it runs on, some elements might not be registered
 * in case that underlying hardware doesn't support the for feature.
 *
 * To get a list of all available elements, user can run
 * ```sh
 * gst-inspect-1.0 qsv
 * ```
 *
 * Since: 1.22
 */

#include <gst/gst.h>
#include <mfx.h>
#include "gstqsvav1enc.h"
#include "gstqsvh264dec.h"
#include "gstqsvh264enc.h"
#include "gstqsvh265dec.h"
#include "gstqsvh265enc.h"
#include "gstqsvjpegdec.h"
#include "gstqsvjpegenc.h"
#include "gstqsvvp9dec.h"
#include "gstqsvvp9enc.h"
#include "gstqsvutils.h"
#include <string.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <versionhelpers.h>
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

#include <glib/gi18n-lib.h>

GST_DEBUG_CATEGORY (gst_qsv_debug);
GST_DEBUG_CATEGORY (gst_qsv_allocator_debug);

#define GST_CAT_DEFAULT gst_qsv_debug

#ifdef G_OS_WIN32
#define MFX_ACCEL_MODE MFX_ACCEL_MODE_VIA_D3D11
#else
#define MFX_ACCEL_MODE MFX_ACCEL_MODE_VIA_VAAPI
#endif

#ifdef G_OS_WIN32
static mfxSession
create_session_with_platform_device (mfxLoader loader,
    mfxImplDescription * desc, guint impl_index, GstObject ** d3d11_device,
    GList ** devices)
{
  mfxSession session = nullptr;
  mfxStatus status;
  GstD3D11Device *selected = nullptr;
  GList *list = *devices;
  GList *iter;
  mfxU16 device_id = 0;

  *d3d11_device = nullptr;

  status = MFXCreateSession (loader, impl_index, &session);
  if (status != MFX_ERR_NONE) {
    GST_WARNING ("Failed to create session with index %d, %d (%s)",
        impl_index, QSV_STATUS_ARGS (status));
    return nullptr;
  }

  if (desc->ApiVersion.Major >= 2 ||
      (desc->ApiVersion.Major == 1 && desc->ApiVersion.Minor >= 19)) {
    mfxPlatform platform;

    memset (&platform, 0, sizeof (mfxPlatform));

    if (MFXVideoCORE_QueryPlatform (session, &platform) == MFX_ERR_NONE) {
      device_id = platform.DeviceId;

      /* XXX: re-create session, MFXVideoCORE_QueryPlatform() may cause
       * later MFXVideoCORE_SetHandle() call failed with
       * MFX_ERR_UNDEFINED_BEHAVIOR error */
      g_clear_pointer (&session, MFXClose);

      status = MFXCreateSession (loader, impl_index, &session);
      if (status != MFX_ERR_NONE) {
        GST_WARNING ("Failed to re-create session with index %d, %d (%s)",
            impl_index, QSV_STATUS_ARGS (status));
        return nullptr;
      }
    }
  }

  if (device_id) {
    for (iter = list; iter; iter = g_list_next (iter)) {
      GstD3D11Device *dev = GST_D3D11_DEVICE (iter->data);
      guint dev_id;

      g_object_get (dev, "device-id", &dev_id, nullptr);
      if (dev_id == (guint) device_id) {
        selected = dev;
        list = g_list_delete_link (list, iter);
        break;
      }
    }
  }

  if (!selected) {
    /* Unknown device id, pick the first device */
    selected = GST_D3D11_DEVICE (list->data);
    list = g_list_delete_link (list, list);
  }

  *devices = list;

  status = MFXVideoCORE_SetHandle (session, MFX_HANDLE_D3D11_DEVICE,
      gst_d3d11_device_get_device_handle (selected));
  if (status != MFX_ERR_NONE) {
    GST_WARNING ("Failed to set d3d11 device handle, %d (%s)",
        QSV_STATUS_ARGS (status));
    gst_object_unref (selected);
    MFXClose (session);

    return nullptr;
  }

  *d3d11_device = GST_OBJECT (selected);

  return session;
}
#else
static mfxSession
create_session_with_platform_device (mfxLoader loader,
    mfxImplDescription * desc, guint impl_index, GstObject ** va_display,
    GList ** devices)
{
  mfxSession session = nullptr;
  mfxStatus status;
  GstVaDisplay *selected;
  GList *list = *devices;

  *va_display = nullptr;

  status = MFXCreateSession (loader, impl_index, &session);
  if (status != MFX_ERR_NONE) {
    GST_WARNING ("Failed to create session with index %d, %d (%s)",
        impl_index, QSV_STATUS_ARGS (status));
    return nullptr;
  }

  /* XXX: what's the relation between implementation index and VA display ?
   * Pick the first available device for now */
  selected = GST_VA_DISPLAY (list->data);
  list = g_list_delete_link (list, list);
  *devices = list;

  status = MFXVideoCORE_SetHandle (session, MFX_HANDLE_VA_DISPLAY,
      gst_va_display_get_va_dpy (selected));
  if (status != MFX_ERR_NONE) {
    GST_WARNING ("Failed to set display handle, %d (%s)",
        QSV_STATUS_ARGS (status));
    gst_object_unref (selected);
    MFXClose (session);

    return nullptr;
  }

  *va_display = GST_OBJECT (selected);

  return session;
}
#endif

static void
plugin_deinit (gpointer data)
{
  gst_qsv_deinit ();
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  mfxLoader loader;
  guint i = 0;
  GList *platform_devices = nullptr;
  GstRank enc_rank = GST_RANK_NONE;

#ifdef G_OS_WIN32
  /* D3D11 Video API is supported since Windows 8.
   * Do we want to support old OS (Windows 7 for example) with D3D9 ?? */
  if (!IsWindows8OrGreater ()) {
    gst_plugin_add_status_warning (plugin,
        N_("This plugin requires at least Windows 8 or newer."));
    return TRUE;
  }

  enc_rank = GST_RANK_PRIMARY;
#endif

  GST_DEBUG_CATEGORY_INIT (gst_qsv_debug, "qsv", 0, "Intel Quick Sync Video");
  GST_DEBUG_CATEGORY_INIT (gst_qsv_allocator_debug,
      "qsvallocator", 0, "qsvallocator");

  loader = gst_qsv_get_loader ();
  if (!loader) {
    // FIXME: any status/error/warning message we should show here?
    return TRUE;
  }

  platform_devices = gst_qsv_get_platform_devices ();
  if (!platform_devices) {
    gst_plugin_add_status_warning (plugin,
        N_("No Intel graphics cards detected!"));
    gst_qsv_deinit ();
    return TRUE;
  }

  GST_INFO ("Found %d platform devices", g_list_length (platform_devices));

  do {
    mfxStatus status = MFX_ERR_NONE;
    mfxSession session = nullptr;
    mfxImplDescription *desc = nullptr;
    GstObject *device = nullptr;

    status = MFXEnumImplementations (loader,
        i, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *) & desc);

    if (status != MFX_ERR_NONE)
      break;

    if ((desc->Impl & MFX_IMPL_TYPE_HARDWARE) == 0)
      goto next;

    if ((desc->AccelerationMode & MFX_ACCEL_MODE) == 0)
      goto next;

    session = create_session_with_platform_device (loader, desc, i, &device,
        &platform_devices);
    if (!session)
      goto next;

    gst_qsv_h264_dec_register (plugin, GST_RANK_MARGINAL, i, device, session);
    gst_qsv_h265_dec_register (plugin, GST_RANK_MARGINAL, i, device, session);
    gst_qsv_jpeg_dec_register (plugin, GST_RANK_SECONDARY, i, device, session);
    gst_qsv_vp9_dec_register (plugin, GST_RANK_MARGINAL, i, device, session);

    gst_qsv_h264_enc_register (plugin, enc_rank, i, device, session);
    gst_qsv_h265_enc_register (plugin, enc_rank, i, device, session);
    gst_qsv_jpeg_enc_register (plugin, enc_rank, i, device, session);
    gst_qsv_vp9_enc_register (plugin, enc_rank, i, device, session);
    gst_qsv_av1_enc_register (plugin, enc_rank, i, device, session);

  next:
    MFXDispReleaseImplDescription (loader, desc);
    g_clear_pointer (&session, MFXClose);
    gst_clear_object (&device);
    i++;

    /* What's the possible maximum number of impl/device ? */
  } while (i < 16 && platform_devices != nullptr);

  if (platform_devices)
    g_list_free_full (platform_devices, (GDestroyNotify) gst_object_unref);

  g_object_set_data_full (G_OBJECT (plugin), "plugin-qsv-shutdown",
      (gpointer) "shutdown-data", (GDestroyNotify) plugin_deinit);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qsv,
    "Intel Quick Sync Video plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
