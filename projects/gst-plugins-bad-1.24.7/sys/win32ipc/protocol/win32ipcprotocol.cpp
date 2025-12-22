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

#include "win32ipcprotocol.h"
#include <string.h>

const char *
win32_ipc_pkt_type_to_string (Win32IpcPktType type)
{
  switch (type) {
    case WIN32_IPC_PKT_NEED_DATA:
      return "NEED-DATA";
    case WIN32_IPC_PKT_HAVE_DATA:
      return "HAVE-DATA";
    case WIN32_IPC_PKT_READ_DONE:
      return "READ-DONE";
    case WIN32_IPC_PKT_RELEASE_DATA:
      return "RELEASE-DATA";
    default:
      break;
  }

  return "Unknown";
}

Win32IpcPktType
win32_ipc_pkt_type_from_raw (UINT8 type)
{
  return (Win32IpcPktType) type;
}

UINT8
win32_ipc_pkt_type_to_raw (Win32IpcPktType type)
{
  return (UINT8) type;
}

#define READ_UINT32(d,v) do { \
  (*((UINT32 *) v)) = *((UINT32 *) d); \
  (d) += sizeof (UINT32); \
} while (0)

#define WRITE_UINT32(d,v) do { \
  *((UINT32 *) d) = v; \
  (d) += sizeof (UINT32); \
} while (0)

#define READ_UINT64(d,v) do { \
  (*((UINT64 *) v)) = *((UINT64 *) d); \
  (d) += sizeof (UINT64); \
} while (0)

#define WRITE_UINT64(d,v) do { \
  *((UINT64 *) d) = v; \
  (d) += sizeof (UINT64); \
} while (0)

UINT32
win32_ipc_pkt_build_need_data (UINT8 * pkt, UINT32 pkt_len, UINT64 seq_num)
{
  UINT8 *data = pkt;

  if (!pkt || pkt_len < WIN32_IPC_PKT_NEED_DATA_SIZE)
    return 0;

  data[0] = win32_ipc_pkt_type_to_raw (WIN32_IPC_PKT_NEED_DATA);
  data++;

  WRITE_UINT64 (data, seq_num);

  return WIN32_IPC_PKT_NEED_DATA_SIZE;
}

BOOL
win32_ipc_pkt_parse_need_data (UINT8 * pkt, UINT32 pkt_len, UINT64 * seq_num)
{
  UINT8 *data = pkt;

  if (!pkt || pkt_len < WIN32_IPC_PKT_NEED_DATA_SIZE)
    return FALSE;

  if (win32_ipc_pkt_type_from_raw (data[0]) != WIN32_IPC_PKT_NEED_DATA)
    return FALSE;

  data++;

  READ_UINT64 (data, seq_num);

  return TRUE;
}

UINT32
win32_ipc_pkt_build_have_data (UINT8 * pkt, UINT32 pkt_size, UINT64 seq_num,
    const char * mmf_name, const Win32IpcVideoInfo * info)
{
  UINT8 *data = pkt;
  size_t len;

  if (!pkt || !mmf_name || !info)
    return 0;

  len = strlen (mmf_name);
  if (len == 0)
    return 0;

  len++;
  if (pkt_size < WIN32_IPC_PKT_HAVE_DATA_SIZE + len)
    return 0;

  data[0] = win32_ipc_pkt_type_to_raw (WIN32_IPC_PKT_HAVE_DATA);
  data++;

  WRITE_UINT64 (data, seq_num);

  strcpy ((char *) data, mmf_name);
  data += len;

  WRITE_UINT32 (data, info->format);
  WRITE_UINT32 (data, info->width);
  WRITE_UINT32 (data, info->height);
  WRITE_UINT32 (data, info->fps_n);
  WRITE_UINT32 (data, info->fps_d);
  WRITE_UINT32 (data, info->par_n);
  WRITE_UINT32 (data, info->par_d);
  WRITE_UINT64 (data, info->size);

  for (UINT i = 0; i < 4; i++)
    WRITE_UINT64 (data, info->offset[i]);

  for (UINT i = 0; i < 4; i++)
    WRITE_UINT32 (data, info->stride[i]);

  WRITE_UINT64 (data, info->qpc);

  return data - pkt;
}

BOOL
win32_ipc_pkt_parse_have_data (UINT8 * pkt, UINT32 pkt_size, UINT64 * seq_num,
    char * mmf_name, Win32IpcVideoInfo * info)
{
  UINT8 *data = pkt;
  size_t len;

  if (!pkt || pkt_size < WIN32_IPC_PKT_HAVE_DATA_SIZE)
    return FALSE;

  if (win32_ipc_pkt_type_from_raw (pkt[0]) != WIN32_IPC_PKT_HAVE_DATA)
    return FALSE;

  data++;

  READ_UINT64 (data, seq_num);

  len = strnlen ((const char *) data, pkt_size - (data - pkt));
  if (len == 0)
    return FALSE;

  len++;
  if (pkt_size < WIN32_IPC_PKT_HAVE_DATA_SIZE + len)
    return FALSE;

  strcpy (mmf_name, (const char *) data);
  data += len;

  READ_UINT32 (data, &info->format);
  READ_UINT32 (data, &info->width);
  READ_UINT32 (data, &info->height);
  READ_UINT32 (data, &info->fps_n);
  READ_UINT32 (data, &info->fps_d);
  READ_UINT32 (data, &info->par_n);
  READ_UINT32 (data, &info->par_d);
  READ_UINT64 (data, &info->size);

  for (UINT i = 0; i < 4; i++)
    READ_UINT64 (data, &info->offset[i]);

  for (UINT i = 0; i < 4; i++)
    READ_UINT32 (data, &info->stride[i]);

  READ_UINT64 (data, &info->qpc);

  return TRUE;
}

UINT32
win32_ipc_pkt_build_read_done (UINT8 * pkt, UINT32 pkt_len, UINT64 seq_num)
{
  UINT8 *data = pkt;

  if (!pkt || pkt_len < WIN32_IPC_PKT_READ_DONE_SIZE)
    return 0;

  data[0] = win32_ipc_pkt_type_to_raw (WIN32_IPC_PKT_READ_DONE);
  data++;

  WRITE_UINT64 (data, seq_num);

  return WIN32_IPC_PKT_READ_DONE_SIZE;
}

BOOL
win32_ipc_pkt_parse_read_done (UINT8 * pkt, UINT32 pkt_len, UINT64 * seq_num)
{
  UINT8 *data = pkt;

  if (!pkt || pkt_len < WIN32_IPC_PKT_READ_DONE_SIZE)
    return FALSE;

  if (win32_ipc_pkt_type_from_raw (data[0]) != WIN32_IPC_PKT_READ_DONE)
    return FALSE;

  data++;

  READ_UINT64 (data, seq_num);

  return TRUE;
}

UINT32
win32_ipc_pkt_build_release_data (UINT8 * pkt, UINT32 pkt_size, UINT64 seq_num,
    const char * mmf_name)
{
  UINT8 *data = pkt;
  size_t len;

  if (!pkt || !mmf_name)
    return 0;

  len = strlen (mmf_name);
  if (len == 0)
    return 0;

  len++;

  data[0] = win32_ipc_pkt_type_to_raw (WIN32_IPC_PKT_RELEASE_DATA);
  data++;

  WRITE_UINT64 (data, seq_num);

  strcpy ((char *) data, mmf_name);
  data += len;

  return data - pkt;
}

BOOL
win32_ipc_pkt_parse_release_data (UINT8 * pkt, UINT32 pkt_size,
    UINT64 * seq_num, char * mmf_name)
{
  UINT8 *data = pkt;
  size_t len;

  if (win32_ipc_pkt_type_from_raw (pkt[0]) != WIN32_IPC_PKT_RELEASE_DATA)
    return FALSE;

  data++;

  READ_UINT64 (data, seq_num);

  len = strnlen ((const char *) data, pkt_size - (data - pkt));
  if (len == 0)
    return FALSE;

  len++;

  strcpy (mmf_name, (const char *) data);
  data += len;

  return TRUE;
}
