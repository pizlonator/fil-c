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

struct Win32IpcPipeServer;

typedef void (*Win32IpcMmfDestroy) (void * user_data);

Win32IpcPipeServer * win32_ipc_pipe_server_new (const char * pipe_name);

Win32IpcPipeServer * win32_ipc_pipe_server_ref (Win32IpcPipeServer * server);

void                 win32_ipc_pipe_server_unref (Win32IpcPipeServer * server);

void                 win32_ipc_pipe_server_shutdown (Win32IpcPipeServer * server);

BOOL                 win32_ipc_pipe_server_send_mmf (Win32IpcPipeServer * server,
                                                     Win32IpcMmf * mmf,
                                                     const Win32IpcVideoInfo * info,
                                                     void * user_data,
                                                     Win32IpcMmfDestroy notify);

G_END_DECLS

