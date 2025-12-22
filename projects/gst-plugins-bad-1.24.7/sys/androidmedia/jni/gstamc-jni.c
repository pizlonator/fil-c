/*
 * Copyright (C) 2023, Ratchanan Srirattanamet <peathot@hotmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <android/api-level.h>

#include "../gstamc.h"
#include "gstamc-jni.h"

#define GST_CAT_DEFAULT gst_amc_debug

#ifdef HAVE_NDKMEDIA

#include "../ndk/gstamc-ndk.h"

static gboolean
should_prefer_ndk ()
{
  int device_api_level = android_get_device_api_level ();
  const gchar *impl_env = g_getenv ("GST_AMC_PREFERED_IMPL");

  if (device_api_level < 21) {
    /* NDK media APIs are added in API level 21 (Android 5). Don't bother
     * trying. It'll fail anyway. */
    return FALSE;
  }

  if (impl_env) {
    if (g_str_equal (impl_env, "ndk"))
      return TRUE;
    else if (g_str_equal (impl_env, "jni"))
      return FALSE;
    else
      GST_WARNING ("Unknown value '%s' for GST_AMC_PREFERED_IMPL, ignored.",
          impl_env);
  }

  /* NDK's AMediaCodec_setParameters() is available since API level 26, while
   * Java's MediaCodec.setParameters() is available since API level 19. Prefers
   * JNI version for 21 <= API level < 26 to avoid feature regression.
   */
  return (device_api_level >= 26);
}
#endif /* HAVE_NDKMEDIA */

gboolean
gst_amc_static_init (void)
{
  if (!gst_amc_codeclist_jni_static_init ())
    return FALSE;

  if (!gst_amc_surface_texture_jni_static_init ())
    return FALSE;

#ifdef HAVE_NDKMEDIA

  if (!should_prefer_ndk ())
    goto init_jni;

  if (!gst_amc_codec_ndk_static_init ())
    goto init_jni;

  if (!gst_amc_format_ndk_static_init ())
    goto init_jni;

  GST_INFO ("Use NDK implementation for GstAmc.");

  gst_amc_format_vtable = &gst_amc_format_ndk_vtable;
  gst_amc_codec_vtable = &gst_amc_codec_ndk_vtable;

  return TRUE;

#endif /* HAVE_NDKMEDIA */

init_jni:
  if (!gst_amc_codec_jni_static_init ())
    return FALSE;

  if (!gst_amc_format_jni_static_init ())
    return FALSE;

  GST_INFO ("Use JNI implementation for GstAmc.");

  gst_amc_format_vtable = &gst_amc_format_jni_vtable;
  gst_amc_codec_vtable = &gst_amc_codec_jni_vtable;

  return TRUE;
}
