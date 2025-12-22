/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstcudanvmm.h"
#include <gmodule.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_cuda_nvmm_debug);
#define GST_CAT_DEFAULT gst_cuda_nvmm_debug

#define LOAD_SYMBOL(name) G_STMT_START { \
  if (!g_module_symbol (module, G_STRINGIFY (name), (gpointer *) &vtable->name)) { \
    GST_ERROR ("Failed to load symbol '%s', %s", G_STRINGIFY (name), g_module_error()); \
    goto error; \
  } \
} G_STMT_END;

/* *INDENT-OFF* */
typedef struct _GstCudaNvmmVTable
{
  gboolean loaded;

  GstBufferPool * (*gst_nvds_buffer_pool_new) (void);

} GstCudaNvmmVTable;
/* *INDENT-ON* */

static GstCudaNvmmVTable gst_cuda_nvmm_vtable = { 0, };

static gboolean
gst_cuda_nvmm_load_library (void)
{
  GModule *module;
  GstCudaNvmmVTable *vtable;

  if (gst_cuda_nvmm_vtable.loaded)
    return TRUE;

  module = g_module_open ("libnvdsbufferpool.so", G_MODULE_BIND_LAZY);
  if (!module) {
    GST_INFO ("libnvdsbufferpool library is unavailable");
    return FALSE;
  }

  vtable = &gst_cuda_nvmm_vtable;

  LOAD_SYMBOL (gst_nvds_buffer_pool_new);

  vtable->loaded = TRUE;
  return TRUE;

error:
  g_module_close (module);

  return FALSE;
}

gboolean
gst_cuda_nvmm_init_once (void)
{
  static gboolean loaded = FALSE;
  static gsize load_once = 0;

  if (g_once_init_enter (&load_once)) {
    loaded = gst_cuda_nvmm_load_library ();
    g_once_init_leave (&load_once, 1);
  }

  return loaded;
}

GstBufferPool *
gst_cuda_nvmm_buffer_pool_new (void)
{
  if (!gst_cuda_nvmm_init_once ())
    return NULL;

  g_assert (gst_cuda_nvmm_vtable.gst_nvds_buffer_pool_new != NULL);

  return gst_cuda_nvmm_vtable.gst_nvds_buffer_pool_new ();
}
