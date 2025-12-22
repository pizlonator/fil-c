/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#pragma once

#include <gst/gst.h>
#include "protocol/win32ipcmmf.h"

G_BEGIN_DECLS

#define GST_TYPE_WIN32_IPC_ALLOCATOR (gst_win32_ipc_allocator_get_type())
G_DECLARE_FINAL_TYPE (GstWin32IpcAllocator, gst_win32_ipc_allocator,
    GST, WIN32_IPC_ALLOCATOR, GstAllocator);

typedef struct _GstWin32IpcMemory GstWin32IpcMemory;

struct _GstWin32IpcMemory
{
  GstMemory mem;

  Win32IpcMmf *mmf;
};

gboolean gst_is_win32_ipc_memory (GstMemory * mem);

GstWin32IpcAllocator * gst_win32_ipc_allocator_new (guint size);

gboolean               gst_win32_ipc_allocator_set_active (GstWin32IpcAllocator * alloc,
                                                           gboolean active);

GstFlowReturn          gst_win32_ipc_allocator_acquire_memory (GstWin32IpcAllocator * alloc,
                                                               GstMemory ** memory);

G_END_DECLS
