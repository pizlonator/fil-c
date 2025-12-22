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

#include "win32ipcmmf.h"
#include "win32ipcutils.h"
#include <string>

GST_DEBUG_CATEGORY_EXTERN (gst_win32_ipc_debug);
#define GST_CAT_DEFAULT gst_win32_ipc_debug

struct Win32IpcMmf
{
  explicit Win32IpcMmf (HANDLE f, void * b, UINT32 s, const std::string & n)
    : file (f), buffer (b), size (s), name (n), ref_count (1)
  {
  }

  ~Win32IpcMmf ()
  {
    GST_TRACE ("Freeing %p (%s)", this, name.c_str ());
    if (buffer)
      UnmapViewOfFile (buffer);
    if (file)
      CloseHandle (file);
  }

  HANDLE file;
  void *buffer;
  UINT32 size;
  std::string name;
  ULONG ref_count;
};

static Win32IpcMmf *
win32_pic_mmf_new (HANDLE file, UINT32 size, const char * name)
{
  Win32IpcMmf *self;
  void *buffer;
  std::string msg;
  UINT err_code;

  buffer = MapViewOfFile (file, FILE_MAP_ALL_ACCESS, 0, 0, size);
  if (!buffer) {
    err_code = GetLastError ();
    msg = win32_ipc_error_message (err_code);
    GST_ERROR ("MapViewOfFile failed with 0x%x (%s)",
        err_code, msg.c_str ());
    CloseHandle (file);
    return nullptr;
  }

  self = new Win32IpcMmf (file, buffer, size, name);

  return self;
}

/**
 * win32_ipc_mmf_alloc:
 * @size: Size of memory to allocate
 * @name: The name of Memory Mapped File
 *
 * Creates named shared memory
 *
 * Returns: a new Win32IpcMmf object
 */
Win32IpcMmf *
win32_ipc_mmf_alloc (UINT32 size, const char * name)
{
  HANDLE file;
  std::string msg;
  UINT err_code;

  if (!size) {
    GST_ERROR ("Zero size is not allowed");
    return nullptr;
  }

  if (!name) {
    GST_ERROR ("Name must be specified");
    return nullptr;
  }

  file = CreateFileMappingA (INVALID_HANDLE_VALUE, nullptr,
      PAGE_READWRITE | SEC_COMMIT, 0, size, name);
  if (!file) {
    err_code = GetLastError ();
    msg = win32_ipc_error_message (err_code);
    GST_ERROR ("CreateFileMappingA failed with 0x%x (%s)",
        err_code, msg.c_str ());
    return nullptr;
  }

  /* The name is already occupied, it's caller's fault... */
  if (GetLastError () == ERROR_ALREADY_EXISTS) {
    GST_ERROR ("File already exists");
    CloseHandle (file);
    return nullptr;
  }

  return win32_pic_mmf_new (file, size, name);
}

/**
 * win32_ipc_mmf_open:
 * @size: Size of memory to allocate
 * @name: The name of Memory Mapped File
 *
 * Opens named shared memory
 *
 * Returns: a new Win32IpcMmf object
 */
Win32IpcMmf *
win32_ipc_mmf_open (UINT32 size, const char * name)
{
  HANDLE file;
  std::string msg;
  UINT err_code;

  if (!size) {
    GST_ERROR ("Zero size is not allowed");
    return nullptr;
  }

  if (!name) {
    GST_ERROR ("Name must be specified");
    return nullptr;
  }

  file = OpenFileMappingA (FILE_MAP_ALL_ACCESS, FALSE, name);
  if (!file) {
    err_code = GetLastError ();
    msg = win32_ipc_error_message (err_code);
    GST_ERROR ("OpenFileMappingA failed with 0x%x (%s)",
        err_code, msg.c_str ());
    return nullptr;
  }

  return win32_pic_mmf_new (file, size, name);
}

/**
 * win32_ipc_mmf_get_name:
 * @mmf: a Win32IpcMmf object
 *
 * Returns: the name of @mmf
 */
const char *
win32_ipc_mmf_get_name (Win32IpcMmf * mmf)
{
  if (!mmf)
    return nullptr;

  return mmf->name.c_str ();
}

/**
 * win32_ipc_mmf_get_size:
 * @mmf: a Win32IpcMmf object
 *
 * Returns: the size of allocated memory
 */
UINT32
win32_ipc_mmf_get_size (Win32IpcMmf * mmf)
{
  if (!mmf)
    return 0;

  return mmf->size;
}

/**
 * win32_ipc_mmf_get_raw:
 * @mmf: a Win32IpcMmf object
 *
 * Returns: the address of allocated memory
 */
void *
win32_ipc_mmf_get_raw (Win32IpcMmf * mmf)
{
  if (!mmf)
    return nullptr;

  return mmf->buffer;
}

/**
 * win32_ipc_mmf_ref:
 * @mmf: a Win32IpcMmf object
 *
 * Increase ref count
 */
Win32IpcMmf *
win32_ipc_mmf_ref (Win32IpcMmf * mmf)
{
  if (!mmf)
    return nullptr;

  InterlockedIncrement (&mmf->ref_count);

  return mmf;
}

/**
 * win32_ipc_mmf_unref:
 * @mmf: a Win32IpcMmf object
 *
 * Decrease ref count
 */
void
win32_ipc_mmf_unref (Win32IpcMmf * mmf)
{
  ULONG count;

  if (!mmf)
    return;

  count = InterlockedDecrement (&mmf->ref_count);
  if (count == 0)
    delete mmf;
}
