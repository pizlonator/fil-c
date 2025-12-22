/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <windows.h>
#include <string.h>
#include "win32ipcmmf.h"
#include "win32ipcprotocol.h"
#include <gst/gst.h>

G_BEGIN_DECLS

struct Win32IpcPipeClient;

Win32IpcPipeClient * win32_ipc_pipe_client_new (const char * pipe_name);

Win32IpcPipeClient * win32_ipc_pipe_client_ref (Win32IpcPipeClient * client);

void                 win32_ipc_pipe_client_unref (Win32IpcPipeClient * client);

void                 win32_ipc_pipe_client_set_flushing (Win32IpcPipeClient * client,
                                                         BOOL flushing);

BOOL                 win32_ipc_pipe_client_get_mmf (Win32IpcPipeClient * client,
                                                    Win32IpcMmf ** mmf,
                                                    Win32IpcVideoInfo * info);

void                 win32_ipc_pipe_client_release_mmf (Win32IpcPipeClient * client,
                                                        Win32IpcMmf * mmf);

void                 win32_ipc_pipe_client_stop (Win32IpcPipeClient * client);

G_END_DECLS
