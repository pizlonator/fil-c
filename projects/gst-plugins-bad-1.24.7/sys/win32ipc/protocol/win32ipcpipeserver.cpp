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

#include "win32ipcpipeserver.h"
#include "win32ipcutils.h"
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <queue>
#include <vector>
#include <string>
#include <algorithm>
#include <assert.h>

GST_DEBUG_CATEGORY_EXTERN (gst_win32_ipc_debug);
#define GST_CAT_DEFAULT gst_win32_ipc_debug

#define CONN_BUFFER_SIZE 1024

struct MmfInfo
{
  explicit MmfInfo (Win32IpcMmf * m, const Win32IpcVideoInfo * i, UINT64 s,
      void * u, Win32IpcMmfDestroy n)
  {
    mmf = m;
    info = *i;
    seq_num = s;
    user_data = u;
    notify = n;
  }

  ~MmfInfo()
  {
    if (mmf)
      win32_ipc_mmf_unref (mmf);

    if (notify)
      notify (user_data);
  }

  Win32IpcMmf *mmf = nullptr;
  Win32IpcVideoInfo info;
  UINT64 seq_num;
  void *user_data;
  Win32IpcMmfDestroy notify;
};

struct ServerConnection : public OVERLAPPED
{
  ServerConnection(Win32IpcPipeServer * server, HANDLE p)
    : self(server), pipe(p)
  {
    OVERLAPPED *parent = dynamic_cast<OVERLAPPED *> (this);
    parent->Internal = 0;
    parent->InternalHigh = 0;
    parent->Offset = 0;
    parent->OffsetHigh = 0;
  }

  Win32IpcPipeServer *self;
  std::shared_ptr<MmfInfo> minfo;
  std::vector<std::shared_ptr<MmfInfo>> used_minfo;
  HANDLE pipe = INVALID_HANDLE_VALUE;
  UINT8 client_msg[CONN_BUFFER_SIZE];
  UINT32 to_read = 0;
  UINT8 server_msg[CONN_BUFFER_SIZE];
  UINT32 to_write = 0;
  UINT64 seq_num = 0;
  BOOL pending_have_data = FALSE;
};

struct Win32IpcPipeServer
{
  explicit Win32IpcPipeServer (const std::string & n)
    : name (n), ref_count (1), last_err (ERROR_SUCCESS), seq_num (0)
  {
    enqueue_event = CreateEventA (nullptr, FALSE, FALSE, nullptr);
    cancellable = CreateEventA (nullptr, TRUE, FALSE, nullptr);
  }

  ~Win32IpcPipeServer ()
  {
    win32_ipc_pipe_server_shutdown (this);
    CloseHandle (cancellable);
    CloseHandle (enqueue_event);
  }

  std::mutex lock;
  std::condition_variable cond;
  std::unique_ptr<std::thread> thread;
  std::shared_ptr<MmfInfo> minfo;
  std::string name;
  std::vector<ServerConnection *> conn;

  ULONG ref_count;
  HANDLE enqueue_event;
  HANDLE cancellable;
  UINT last_err;
  UINT64 seq_num;
};

static void
win32_ipc_pipe_server_wait_client_msg_async (ServerConnection * conn);

static void
win32_ipc_pipe_server_close_connection (ServerConnection * conn,
    BOOL remove_from_list)
{
  Win32IpcPipeServer *self = conn->self;

  GST_DEBUG ("Closing connection %p", conn);

  if (remove_from_list) {
    self->conn.erase (std::remove (self->conn.begin (), self->conn.end (),
        conn), self->conn.end ());
  }

  if (!DisconnectNamedPipe (conn->pipe)) {
    UINT last_err = GetLastError ();
    std::string msg = win32_ipc_error_message (last_err);
    GST_WARNING ("DisconnectNamedPipe failed with 0x%x (%s)",
        last_err, msg.c_str ());
  }

  CloseHandle (conn->pipe);
  delete conn;
}

static void WINAPI
win32_ipc_pipe_server_send_have_data_finish (DWORD error_code, DWORD n_bytes,
    LPOVERLAPPED overlapped)
{
  ServerConnection *conn = (ServerConnection *) overlapped;

  if (error_code != ERROR_SUCCESS) {
    std::string msg = win32_ipc_error_message (error_code);
    GST_WARNING ("HAVE-DATA failed with 0x%x (%s)",
        (UINT) error_code, msg.c_str ());
    win32_ipc_pipe_server_close_connection (conn, TRUE);
    return;
  }

  GST_TRACE ("HAVE-DATA done with %s",
      win32_ipc_mmf_get_name (conn->minfo->mmf));

  win32_ipc_pipe_server_wait_client_msg_async (conn);
}

static void
win32_ipc_pipe_server_send_have_data_async (ServerConnection * conn)
{
  assert (conn->minfo != nullptr);

  conn->pending_have_data = FALSE;
  conn->seq_num = conn->minfo->seq_num;

  conn->to_write = win32_ipc_pkt_build_have_data (conn->server_msg,
      CONN_BUFFER_SIZE, conn->seq_num,
      win32_ipc_mmf_get_name (conn->minfo->mmf), &conn->minfo->info);
  if (conn->to_write == 0) {
    GST_ERROR ("Couldn't build HAVE-DATA pkt");
    win32_ipc_pipe_server_close_connection (conn, TRUE);
    return;
  }

  conn->seq_num++;

  GST_TRACE ("Sending HAVE-DATA");

  if (!WriteFileEx (conn->pipe, conn->server_msg, conn->to_write,
      (OVERLAPPED *) conn, win32_ipc_pipe_server_send_have_data_finish)) {
    UINT last_err = GetLastError ();
    std::string msg = win32_ipc_error_message (last_err);
    GST_WARNING ("WriteFileEx failed with 0x%x (%s)", last_err, msg.c_str ());
    win32_ipc_pipe_server_close_connection (conn, TRUE);
  }
}

static void WINAPI
win32_ipc_pipe_server_wait_client_msg_finish (DWORD error_code, DWORD n_bytes,
    LPOVERLAPPED overlapped)
{
  ServerConnection *conn = (ServerConnection *) overlapped;
  UINT64 seq_num;
  Win32IpcPktType type;
  char mmf_name[1024];

  if (error_code != ERROR_SUCCESS) {
    std::string msg = win32_ipc_error_message (error_code);
    GST_WARNING ("NEED-DATA failed with 0x%x (%s)",
        (UINT) error_code, msg.c_str ());
    win32_ipc_pipe_server_close_connection (conn, TRUE);
    return;
  }

  type = win32_ipc_pkt_type_from_raw (conn->client_msg[0]);
  switch (type) {
    case WIN32_IPC_PKT_NEED_DATA:
      GST_TRACE ("Got NEED-DATA %p", conn);

      if (!win32_ipc_pkt_parse_need_data (conn->client_msg, CONN_BUFFER_SIZE,
          &seq_num)) {
        GST_ERROR ("Couldn't parse NEED-DATA message");
        win32_ipc_pipe_server_close_connection (conn, TRUE);
        return;
      }

      /* Will response later once data is available */
      if (!conn->minfo) {
        GST_LOG ("No data available, waiting");
        conn->pending_have_data = TRUE;
        return;
      }

      win32_ipc_pipe_server_send_have_data_async (conn);
      break;
    case WIN32_IPC_PKT_READ_DONE:
      GST_TRACE ("Got READ-DONE %p", conn);

      conn->used_minfo.push_back (conn->minfo);
      conn->minfo = nullptr;

      /* All done, wait for need-data again */
      win32_ipc_pipe_server_wait_client_msg_async (conn);
      break;
    case WIN32_IPC_PKT_RELEASE_DATA:
    {
      GST_TRACE ("Got RELEASE-DATA %p", conn);

      if (!win32_ipc_pkt_parse_release_data (conn->client_msg, CONN_BUFFER_SIZE,
          &seq_num, mmf_name)) {
        GST_WARNING ("Couldn't parse RELEASE-DATA mssage");
        return;
      }

      auto it = std::find_if (conn->used_minfo.begin (),
          conn->used_minfo.end (), [&](const std::shared_ptr<MmfInfo> info) -> bool {
            return strcmp (mmf_name, win32_ipc_mmf_get_name (info->mmf)) == 0;
          });

      if (it != conn->used_minfo.end ()) {
        conn->used_minfo.erase (it);
      } else {
        GST_WARNING ("Unknown memory name %s", mmf_name);
      }

      win32_ipc_pipe_server_wait_client_msg_async (conn);
      break;
    }
    default:
      GST_WARNING ("Unexpected packet type");
      win32_ipc_pipe_server_close_connection (conn, TRUE);
      break;
  }
}

static void
win32_ipc_pipe_server_wait_client_msg_async (ServerConnection * conn)
{
  GST_TRACE ("Waiting client message");

  if (!ReadFileEx (conn->pipe, conn->client_msg, CONN_BUFFER_SIZE,
      (OVERLAPPED *) conn, win32_ipc_pipe_server_wait_client_msg_finish)) {
    UINT last_err = GetLastError ();
    std::string msg = win32_ipc_error_message (last_err);

    GST_WARNING ("ReadFileEx failed with 0x%x (%s)", last_err, msg.c_str ());
    win32_ipc_pipe_server_close_connection (conn, TRUE);
  }
}

static HANDLE
win32_ipc_pipe_server_create_pipe (Win32IpcPipeServer * self,
    OVERLAPPED * overlap, BOOL * io_pending)
{
  HANDLE pipe = CreateNamedPipeA (self->name.c_str (),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES,
      CONN_BUFFER_SIZE, CONN_BUFFER_SIZE, 5000, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    self->last_err = GetLastError ();
    std::string msg = win32_ipc_error_message (self->last_err);
    GST_WARNING ("CreateNamedPipeA failed with 0x%x (%s)",
        self->last_err, msg.c_str ());
    return INVALID_HANDLE_VALUE;
  }

  /* Async pipe should return FALSE */
  if (ConnectNamedPipe (pipe, overlap)) {
    self->last_err = GetLastError ();
    std::string msg = win32_ipc_error_message (self->last_err);
    GST_WARNING ("ConnectNamedPipe failed with 0x%x (%s)",
        self->last_err, msg.c_str ());
    CloseHandle (pipe);
    return INVALID_HANDLE_VALUE;
  }

  *io_pending = FALSE;
  self->last_err = GetLastError ();
  switch (self->last_err) {
    case ERROR_IO_PENDING:
      *io_pending = TRUE;
      break;
    case ERROR_PIPE_CONNECTED:
      SetEvent (overlap->hEvent);
      break;
    default:
    {
      std::string msg = win32_ipc_error_message (self->last_err);
      GST_WARNING ("ConnectNamedPipe failed with 0x%x (%s)",
          self->last_err, msg.c_str ());
      CloseHandle (pipe);
      return INVALID_HANDLE_VALUE;
    }
  }

  self->last_err = ERROR_SUCCESS;

  return pipe;
}

static void
win32_ipc_pipe_server_loop (Win32IpcPipeServer * self)
{
  BOOL io_pending = FALSE;
  DWORD n_bytes;
  DWORD wait_ret;
  HANDLE waitables[3];
  HANDLE pipe;
  OVERLAPPED overlap;
  std::unique_lock<std::mutex> lk (self->lock);

  overlap.hEvent = CreateEvent (nullptr, TRUE, TRUE, nullptr);
  pipe = win32_ipc_pipe_server_create_pipe (self, &overlap, &io_pending);
  if (pipe == INVALID_HANDLE_VALUE) {
    CloseHandle (overlap.hEvent);
    self->cond.notify_all ();
    return;
  }

  self->last_err = ERROR_SUCCESS;
  self->cond.notify_all ();
  lk.unlock ();

  waitables[0] = overlap.hEvent;
  waitables[1] = self->enqueue_event;
  waitables[2] = self->cancellable;

  do {
    ServerConnection *conn;

    /* Enters alertable state and wait for
     * 1) Client's connection request
     *    (similar to socket listen/accept in async manner)
     * 2) Or, performs completion routines (finish APC)
     * 3) Or, terminates if cancellable event was signalled
     */
    wait_ret = WaitForMultipleObjectsEx (3, waitables, FALSE, INFINITE, TRUE);
    if (wait_ret == WAIT_OBJECT_0 + 2) {
      GST_DEBUG ("Operation cancelled");
      goto out;
    }

    switch (wait_ret) {
      case WAIT_OBJECT_0:
        if (io_pending) {
          BOOL ret = GetOverlappedResult (pipe, &overlap, &n_bytes, FALSE);
          if (!ret) {
            UINT last_err = GetLastError ();
            std::string msg = win32_ipc_error_message (last_err);
            GST_WARNING ("ConnectNamedPipe failed with 0x%x (%s)",
                last_err, msg.c_str ());
            CloseHandle (pipe);
            break;
          }
        }

        conn = new ServerConnection (self, pipe);
        GST_DEBUG ("New connection is established %p", conn);

        /* Stores current buffer if available */
        lk.lock();
        conn->minfo = self->minfo;
        lk.unlock ();

        pipe = INVALID_HANDLE_VALUE;
        self->conn.push_back (conn);
        win32_ipc_pipe_server_wait_client_msg_async (conn);
        pipe = win32_ipc_pipe_server_create_pipe (self, &overlap, &io_pending);
        if (pipe == INVALID_HANDLE_VALUE)
          goto out;
        break;
      case WAIT_OBJECT_0 + 1:
      case WAIT_IO_COMPLETION:
      {
        std::vector<ServerConnection *> pending_conns;
        std::shared_ptr<MmfInfo> minfo;

        lk.lock();
        minfo = self->minfo;
        lk.unlock();

        if (minfo) {
          for (auto iter: self->conn) {
            if (iter->pending_have_data && iter->seq_num <= minfo->seq_num) {
              iter->minfo = minfo;
              pending_conns.push_back (iter);
            }
          }
        }

        for (auto iter: pending_conns) {
          GST_LOG ("Sending pending have data to %p", iter);
          win32_ipc_pipe_server_send_have_data_async (iter);
        }

        break;
      }
      default:
        GST_WARNING ("Unexpected WaitForMultipleObjectsEx return 0x%x",
            (UINT) wait_ret);
        goto out;
    }
  } while (true);

out:
  /* Cancels all I/O event issued from this thread */
  {
    std::vector<HANDLE> pipes;
    for (auto iter: self->conn) {
      if (iter->pipe != INVALID_HANDLE_VALUE)
        pipes.push_back (iter->pipe);
    }

    for (auto iter: pipes)
      CancelIo (iter);
  }

  for (auto iter: self->conn)
    win32_ipc_pipe_server_close_connection (iter, FALSE);

  self->conn.clear ();

  if (pipe != INVALID_HANDLE_VALUE)
    CloseHandle (pipe);

  lk.lock ();
  CloseHandle (overlap.hEvent);
  self->last_err = ERROR_OPERATION_ABORTED;
  self->cond.notify_all ();
}

static BOOL
win32_ipc_pipe_server_run (Win32IpcPipeServer * self)
{
  std::unique_lock<std::mutex> lk (self->lock);

  self->thread = std::make_unique<std::thread>
      (std::thread (win32_ipc_pipe_server_loop, self));
  self->cond.wait (lk);

  if (self->last_err != ERROR_SUCCESS) {
    self->thread->join ();
    self->thread = nullptr;
    return FALSE;
  }

  return TRUE;
}

Win32IpcPipeServer *
win32_ipc_pipe_server_new (const char * pipe_name)
{
  Win32IpcPipeServer *self;

  if (!pipe_name)
    return nullptr;

  self = new Win32IpcPipeServer (pipe_name);

  if (!win32_ipc_pipe_server_run (self)) {
    win32_ipc_pipe_server_unref (self);
    return nullptr;
  }

  return self;
}

Win32IpcPipeServer *
win32_ipc_pipe_server_ref (Win32IpcPipeServer * server)
{
  if (!server)
    return nullptr;

  InterlockedIncrement (&server->ref_count);

  return server;
}

void
win32_ipc_pipe_server_unref (Win32IpcPipeServer * server)
{
  ULONG ref_count;

  if (!server)
    return;

  ref_count = InterlockedDecrement (&server->ref_count);
  if (ref_count == 0)
    delete server;
}

void
win32_ipc_pipe_server_shutdown (Win32IpcPipeServer * server)
{
  GST_DEBUG ("Shutting down");

  SetEvent (server->cancellable);
  if (server->thread) {
    server->thread->join ();
    server->thread = nullptr;
  }

  std::lock_guard<std::mutex> lk (server->lock);
  server->last_err = ERROR_OPERATION_ABORTED;
  server->minfo = nullptr;
  server->cond.notify_all ();
}

BOOL
win32_ipc_pipe_server_send_mmf (Win32IpcPipeServer * server, Win32IpcMmf * mmf,
    const Win32IpcVideoInfo * info, void * user_data, Win32IpcMmfDestroy notify)
{
  std::lock_guard<std::mutex> lk (server->lock);
  server->minfo = std::make_shared<MmfInfo> (mmf, info, server->seq_num,
      user_data, notify);

  GST_LOG ("Enqueue mmf %s", win32_ipc_mmf_get_name (mmf));

  server->seq_num++;

  /* Wakeup event loop */
  SetEvent (server->enqueue_event);

  return TRUE;
}
