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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwin32ipcmemory.h"
#include "gstwin32ipcutils.h"
#include <string>
#include <mutex>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_win32_ipc_allocator_debug);
#define GST_CAT_DEFAULT gst_win32_ipc_allocator_debug

#define GST_WIN32_IPC_MEMORY_NAME "Win32IpcMemory"
#define GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING(alloc) \
    (g_atomic_int_get (&alloc->flushing))

static GstWin32IpcAllocator *gc_allocator = nullptr;

struct _GstWin32IpcAllocator
{
  GstAllocator parent;

  guint size;

  gboolean is_gc;

  GstAtomicQueue *queue;
  GstPoll *poll;
  gchar *prefix;
  LONG64 seq_num;

  CRITICAL_SECTION lock;
  gboolean started;
  gboolean active;

  /* atomic */
  gint outstanding;
  guint max_mems;
  guint cur_mems;
  gboolean flushing;
};

static void gst_win32_ipc_allocator_finalize (GObject * object);
static GstMemory *gst_win32_ipc_allocator_dummy_alloc (GstAllocator * alloc,
    gsize size, GstAllocationParams * params);
static void gst_win32_ipc_allocator_free (GstAllocator * alloc,
    GstMemory * mem);

static gpointer gst_win32_ipc_allocator_map (GstMemory * mem, gsize maxsize,
    GstMapFlags flags);
static void gst_win32_ipc_allocator_unmap (GstMemory * mem);
static GstMemory *gst_win32_ipc_allocator_share (GstMemory * mem,
    gssize offset, gssize size);

static gboolean gst_win32_ipc_allocator_start (GstWin32IpcAllocator * self);
static gboolean gst_win32_ipc_allocator_stop (GstWin32IpcAllocator * self);
static gboolean gst_win32_ipc_memory_release (GstMiniObject * mini_object);

#define gst_win32_ipc_allocator_parent_class parent_class
G_DEFINE_TYPE (GstWin32IpcAllocator, gst_win32_ipc_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_win32_ipc_allocator_class_init (GstWin32IpcAllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstAllocatorClass *alloc_class = GST_ALLOCATOR_CLASS (klass);

  object_class->finalize = gst_win32_ipc_allocator_finalize;

  alloc_class->alloc = gst_win32_ipc_allocator_dummy_alloc;
  alloc_class->free = gst_win32_ipc_allocator_free;

  GST_DEBUG_CATEGORY_INIT (gst_win32_ipc_allocator_debug,
      "win32ipcallocator", 0, "win32ipcallocator");
}

static void
gst_win32_ipc_allocator_init (GstWin32IpcAllocator * self)
{
  GstAllocator *alloc = GST_ALLOCATOR (self);

  alloc->mem_type = GST_WIN32_IPC_MEMORY_NAME;
  alloc->mem_map = gst_win32_ipc_allocator_map;
  alloc->mem_unmap = gst_win32_ipc_allocator_unmap;
  alloc->mem_share = gst_win32_ipc_allocator_share;

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);

  InitializeCriticalSection (&self->lock);

  self->poll = gst_poll_new_timer ();
  self->queue = gst_atomic_queue_new (16);
  self->flushing = 1;
  self->active = FALSE;
  self->started = FALSE;

  /* 1 control write for flushing - the flush token */
  gst_poll_write_control (self->poll);
  /* 1 control write for marking that we are not waiting for poll - the wait token */
  gst_poll_write_control (self->poll);
}

static void
gst_win32_ipc_allocator_finalize (GObject * object)
{
  GstWin32IpcAllocator *self = GST_WIN32_IPC_ALLOCATOR (object);

  GST_DEBUG_OBJECT (self, "Finalize");

  gst_win32_ipc_allocator_stop (self);
  gst_atomic_queue_unref (self->queue);
  gst_poll_free (self->poll);
  DeleteCriticalSection (&self->lock);
  g_free (self->prefix);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstMemory *
gst_win32_ipc_allocator_dummy_alloc (GstAllocator * alloc, gsize size,
    GstAllocationParams * params)
{
  g_return_val_if_reached (nullptr);
}

static void
gst_win32_ipc_allocator_free (GstAllocator * alloc, GstMemory * mem)
{
  GstWin32IpcMemory *imem = (GstWin32IpcMemory *) mem;

  win32_ipc_mmf_unref (imem->mmf);
  g_free (imem);
}

static gpointer
gst_win32_ipc_allocator_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstWin32IpcMemory *imem = (GstWin32IpcMemory *) mem;

  return win32_ipc_mmf_get_raw (imem->mmf);
}

static void
gst_win32_ipc_allocator_unmap (GstMemory * mem)
{
  /* do nothing */
}

static GstMemory *
gst_win32_ipc_allocator_share (GstMemory * mem, gssize offset, gssize size)
{
  /* not supported */
  return nullptr;
}

static gboolean
gst_win32_ipc_allocator_start (GstWin32IpcAllocator * self)
{
  if (self->started)
    return TRUE;

  self->started = TRUE;

  return TRUE;
}

static void
gst_win32_ipc_allocator_do_set_flushing (GstWin32IpcAllocator * self,
    gboolean flushing)
{
  if (GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING (self) == flushing)
    return;

  if (flushing) {
    g_atomic_int_set (&self->flushing, 1);
    /* Write the flush token to wake up any waiters */
    gst_poll_write_control (self->poll);
  } else {
    while (!gst_poll_read_control (self->poll)) {
      if (errno == EWOULDBLOCK) {
        /* This should not really happen unless flushing and unflushing
         * happens on different threads. Let's wait a bit to get back flush
         * token from the thread that was setting it to flushing */
        g_thread_yield ();
        continue;
      } else {
        /* Critical error but GstPoll already complained */
        break;
      }
    }

    g_atomic_int_set (&self->flushing, 0);
  }
}

gboolean
gst_win32_ipc_allocator_set_active (GstWin32IpcAllocator * self,
    gboolean active)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_WIN32_IPC_ALLOCATOR (self), FALSE);

  EnterCriticalSection (&self->lock);
  if (self->active == active)
    goto out;

  if (active) {
    gst_win32_ipc_allocator_start (self);

    /* flush_stop may release memory objects, setting to active to avoid running
     * do_stop while activating the pool */
    self->active = TRUE;

    gst_win32_ipc_allocator_do_set_flushing (self, FALSE);
  } else {
    gint outstanding;

    /* set to flushing first */
    gst_win32_ipc_allocator_do_set_flushing (self, TRUE);

    /* when all memory objects are in the pool, free them. Else they will be
     * freed when they are released */
    outstanding = g_atomic_int_get (&self->outstanding);
    GST_LOG_OBJECT (self, "outstanding memories %d, (in queue %d)",
        outstanding, gst_atomic_queue_length (self->queue));
    if (outstanding == 0) {
      if (!gst_win32_ipc_allocator_stop (self)) {
        GST_ERROR_OBJECT (self, "stop failed");
        ret = FALSE;
        goto out;
      }
    }

    self->active = FALSE;
  }

out:
  LeaveCriticalSection (&self->lock);

  return ret;
}

static void
gst_win32_ipc_allocator_free_memory (GstWin32IpcAllocator * self,
    GstMemory * mem)
{
  g_atomic_int_add (&self->cur_mems, -1);
  GST_LOG_OBJECT (self, "freeing memory %p (%u left)", mem, self->cur_mems);

  GST_MINI_OBJECT_CAST (mem)->dispose = nullptr;
  gst_memory_unref (mem);
}

/* must be called with the lock */
static gboolean
gst_win32_ipc_allocator_clear_queue (GstWin32IpcAllocator * self)
{
  GstMemory *memory;

  GST_LOG_OBJECT (self, "Clearing queue");

  /* clear the pool */
  while ((memory = (GstMemory *) gst_atomic_queue_pop (self->queue))) {
    while (!gst_poll_read_control (self->poll)) {
      if (errno == EWOULDBLOCK) {
        /* We put the memory into the queue but did not finish writing control
         * yet, let's wait a bit and retry */
        g_thread_yield ();
        continue;
      } else {
        /* Critical error but GstPoll already complained */
        break;
      }
    }
    gst_win32_ipc_allocator_free_memory (self, memory);
  }

  GST_LOG_OBJECT (self, "Clear done");

  return self->cur_mems == 0;
}

static gboolean
gst_win32_ipc_allocator_stop (GstWin32IpcAllocator * self)
{
  GST_DEBUG_OBJECT (self, "Stop");

  if (self->started) {
    if (!gst_win32_ipc_allocator_clear_queue (self))
      return FALSE;

    self->started = FALSE;
  }

  return TRUE;
}

static void
dec_outstanding (GstWin32IpcAllocator * self)
{
  if (g_atomic_int_dec_and_test (&self->outstanding)) {
    /* all memory objects are returned to the pool, see if we need to free them */
    if (GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING (self)) {
      /* take the lock so that set_active is not run concurrently */
      EnterCriticalSection (&self->lock);
      /* now that we have the lock, check if we have been de-activated with
       * outstanding buffers */
      if (!self->active)
        gst_win32_ipc_allocator_stop (self);
      LeaveCriticalSection (&self->lock);
    }
  }
}

static void
gst_win32_ipc_allocator_release_memory (GstWin32IpcAllocator * self,
    GstMemory * mem)
{
  GST_MINI_OBJECT_CAST (mem)->dispose = nullptr;
  mem->allocator = (GstAllocator *) gst_object_ref (gc_allocator);

  /* keep it around in our queue */
  gst_atomic_queue_push (self->queue, mem);
  gst_poll_write_control (self->poll);
  dec_outstanding (self);

  gst_object_unref (self);
}

static gboolean
gst_win32_ipc_memory_release (GstMiniObject * mini_object)
{
  GstMemory *mem = GST_MEMORY_CAST (mini_object);
  GstWin32IpcAllocator *self;

  g_assert (mem->allocator != nullptr);

  self = GST_WIN32_IPC_ALLOCATOR (mem->allocator);

  /* Memory belongs to garbage collector, free this */
  if (self->is_gc)
    return TRUE;

  if (GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING (self))
    return TRUE;

  /* return the memory to the allocator */
  gst_memory_ref (mem);
  gst_win32_ipc_allocator_release_memory (self, mem);

  return FALSE;
}

static GstFlowReturn
gst_win32_ipc_allocator_alloc (GstWin32IpcAllocator * self, GstMemory ** mem)
{
  GstWin32IpcMemory *new_mem;
  Win32IpcMmf *mmf;
  std::string mmf_name;

  mmf_name = std::string (self->prefix) +
      std::to_string (InterlockedIncrement64 (&self->seq_num));

  mmf = win32_ipc_mmf_alloc (self->size, mmf_name.c_str ());
  if (!mmf) {
    GST_ERROR_OBJECT (self, "Couldn't allocate memory");
    return GST_FLOW_ERROR;
  }

  memset (win32_ipc_mmf_get_raw (mmf), 0, win32_ipc_mmf_get_size (mmf));

  g_atomic_int_add (&self->cur_mems, 1);
  new_mem = g_new0 (GstWin32IpcMemory, 1);
  gst_memory_init (GST_MEMORY_CAST (new_mem), (GstMemoryFlags) 0,
      GST_ALLOCATOR_CAST (gc_allocator), nullptr, self->size, 0, 0, self->size);
  new_mem->mmf = mmf;

  *mem = GST_MEMORY_CAST (new_mem);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_win32_ipc_allocator_acquire_memory_internal (GstWin32IpcAllocator * self,
    GstMemory ** memory)
{
  GstFlowReturn result;

  while (TRUE) {
    if (GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING (self)) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      return GST_FLOW_FLUSHING;
    }

    /* try to get a memory from the queue */
    *memory = (GstMemory *) gst_atomic_queue_pop (self->queue);
    if (*memory) {
      while (!gst_poll_read_control (self->poll)) {
        if (errno == EWOULDBLOCK) {
          /* We put the memory into the queue but did not finish writing control
           * yet, let's wait a bit and retry */
          g_thread_yield ();
          continue;
        } else {
          /* Critical error but GstPoll already complained */
          break;
        }
      }
      result = GST_FLOW_OK;
      GST_LOG_OBJECT (self, "acquired memory %p", *memory);
      break;
    }

    /* no memory, try to allocate some more */
    GST_LOG_OBJECT (self, "no memory, trying to allocate");
    result = gst_win32_ipc_allocator_alloc (self, memory);
    if (result == GST_FLOW_OK)
      /* we have a memory, return it */
      break;

    if (G_UNLIKELY (result != GST_FLOW_EOS))
      /* something went wrong, return error */
      break;

    /* now we release the control socket, we wait for a memory release or
     * flushing */
    if (!gst_poll_read_control (self->poll)) {
      if (errno == EWOULDBLOCK) {
        /* This means that we have two threads trying to allocate memory
         * already, and the other one already got the wait token. This
         * means that we only have to wait for the poll now and not write the
         * token afterwards: we will be woken up once the other thread is
         * woken up and that one will write the wait token it removed */
        GST_LOG_OBJECT (self, "waiting for free memory or flushing");
        gst_poll_wait (self->poll, GST_CLOCK_TIME_NONE);
      } else {
        /* This is a critical error, GstPoll already gave a warning */
        result = GST_FLOW_ERROR;
        break;
      }
    } else {
      /* We're the first thread waiting, we got the wait token and have to
       * write it again later
       * OR
       * We're a second thread and just consumed the flush token and block all
       * other threads, in which case we must not wait and give it back
       * immediately */
      if (!GST_WIN32_IPC_ALLOCATOR_IS_FLUSHING (self)) {
        GST_LOG_OBJECT (self, "waiting for free memory or flushing");
        gst_poll_wait (self->poll, GST_CLOCK_TIME_NONE);
      }
      gst_poll_write_control (self->poll);
    }
  }

  return result;
}

gboolean
gst_is_win32_ipc_memory (GstMemory * mem)
{
  return mem != nullptr && mem->allocator != nullptr &&
      GST_IS_WIN32_IPC_ALLOCATOR (mem->allocator);
}

static void
gst_win32_ipc_allocator_init_once (void)
{
  static std::once_flag once_flag;
  std::call_once (once_flag,[]() {
        gc_allocator = (GstWin32IpcAllocator *)
        g_object_new (GST_TYPE_WIN32_IPC_ALLOCATOR, nullptr);
        gst_object_ref_sink (gc_allocator);
        GST_OBJECT_FLAG_SET (gc_allocator, GST_OBJECT_FLAG_MAY_BE_LEAKED);
        gc_allocator->is_gc = TRUE;
      });
}

GstWin32IpcAllocator *
gst_win32_ipc_allocator_new (guint size)
{
  GstWin32IpcAllocator *self;

  g_return_val_if_fail (size != 0, nullptr);

  gst_win32_ipc_allocator_init_once ();

  self = (GstWin32IpcAllocator *)
      g_object_new (GST_TYPE_WIN32_IPC_ALLOCATOR, nullptr);
  self->size = size;
  self->prefix = gst_win32_ipc_get_mmf_prefix ();

  gst_object_ref_sink (self);

  return self;
}

GstFlowReturn
gst_win32_ipc_allocator_acquire_memory (GstWin32IpcAllocator * alloc,
    GstMemory ** memory)
{
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_WIN32_IPC_ALLOCATOR (alloc), GST_FLOW_ERROR);
  g_return_val_if_fail (memory != nullptr, GST_FLOW_ERROR);

  *memory = nullptr;

  g_atomic_int_inc (&alloc->outstanding);
  ret = gst_win32_ipc_allocator_acquire_memory_internal (alloc, memory);

  if (ret == GST_FLOW_OK) {
    GstMemory *mem = *memory;
    /* Replace default allocator with ours */
    gst_object_unref (mem->allocator);
    mem->allocator = (GstAllocator *) gst_object_ref (alloc);
    GST_MINI_OBJECT_CAST (mem)->dispose = gst_win32_ipc_memory_release;
  } else {
    dec_outstanding (alloc);
  }

  return ret;
}
