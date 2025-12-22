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

#include "gstdwritebitmapmemory.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_dwrite_bitmap_allocator_debug);
#define GST_CAT_DEFAULT gst_dwrite_bitmap_allocator_debug

#define GST_DWRITE_BITMAP_MEMORY_NAME "DWriteBitmapMemory"

struct GstDWriteBitmapAllocatorPrivate
{
  GstDWriteBitmapAllocatorPrivate ()
  {
    com_thread = std::thread (&GstDWriteBitmapAllocatorPrivate::ComThreadFunc,
        this);
    std::unique_lock < std::mutex > lk (init_lock);
    while (!running)
      init_cond.wait (lk);
  }

   ~GstDWriteBitmapAllocatorPrivate ()
  {
    thread_lock.lock ();
    terminate = true;
    thread_cond.notify_one ();
    thread_lock.unlock ();

    com_thread.join ();
  }

  void ComThreadFunc ()
  {
    HRESULT hr;
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    hr = CoCreateInstance (CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&factory));
    if (FAILED (hr))
      GST_ERROR ("Couldn't create image factory");

    init_lock.lock ();
    running = true;
    init_cond.notify_one ();
    init_lock.unlock ();

    std::unique_lock < std::mutex > lk (thread_lock);
    while (!terminate)
      thread_cond.wait (lk);

    if (factory)
      factory->Release ();

    CoUninitialize ();
  }

  IWICImagingFactory *factory = nullptr;

  std::mutex init_lock;
  std::condition_variable init_cond;

  std::mutex thread_lock;
  std::condition_variable thread_cond;

  bool running = false;
  bool terminate = false;

  std::thread com_thread;
};

struct _GstDWriteBitmapAllocator
{
  GstAllocator parent;

  GstDWriteBitmapAllocatorPrivate *priv;
};

static void gst_dwrite_bitmap_allocator_finalize (GObject * object);

static GstMemory *gst_dwrite_bitmap_allocator_dummy_alloc (GstAllocator * alloc,
    gsize size, GstAllocationParams * params);
static void gst_dwrite_bitmap_allocator_free (GstAllocator * alloc,
    GstMemory * mem);

static gpointer gst_dwrite_bitmap_allocator_map (GstMemory * mem,
    GstMapInfo * info, gsize maxsize);
static void gst_dwrite_bitmap_allocator_unmap (GstMemory * mem,
    GstMapInfo * info);
static GstMemory *gst_dwrite_bitmap_allocator_share (GstMemory * mem,
    gssize offset, gssize size);

#define gst_dwrite_bitmap_allocator_parent_class parent_class
G_DEFINE_TYPE (GstDWriteBitmapAllocator, gst_dwrite_bitmap_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_dwrite_bitmap_allocator_class_init (GstDWriteBitmapAllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstAllocatorClass *alloc_class = GST_ALLOCATOR_CLASS (klass);

  object_class->finalize = gst_dwrite_bitmap_allocator_finalize;

  alloc_class->alloc = gst_dwrite_bitmap_allocator_dummy_alloc;
  alloc_class->free = gst_dwrite_bitmap_allocator_free;

  GST_DEBUG_CATEGORY_INIT (gst_dwrite_bitmap_allocator_debug,
      "dwritebitmapallocator", 0, "dwritebitmapallocator");
}

static void
gst_dwrite_bitmap_allocator_init (GstDWriteBitmapAllocator * self)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (self);

  alloc->mem_type = GST_DWRITE_BITMAP_MEMORY_NAME;
  alloc->mem_map_full = gst_dwrite_bitmap_allocator_map;
  alloc->mem_unmap_full = gst_dwrite_bitmap_allocator_unmap;
  alloc->mem_share = gst_dwrite_bitmap_allocator_share;

  GST_OBJECT_FLAG_SET (alloc, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);

  self->priv = new GstDWriteBitmapAllocatorPrivate ();
}

static void
gst_dwrite_bitmap_allocator_finalize (GObject * object)
{
  GstDWriteBitmapAllocator *self = GST_DWRITE_BITMAP_ALLOCATOR (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstMemory *
gst_dwrite_bitmap_allocator_dummy_alloc (GstAllocator * alloc, gsize size,
    GstAllocationParams * params)
{
  g_assert_not_reached ();
  return nullptr;
}

static void
gst_dwrite_bitmap_allocator_free (GstAllocator * alloc, GstMemory * mem)
{
  GstDWriteBitmapMemory *dmem = (GstDWriteBitmapMemory *) mem;

  if (dmem->bitmap)
    dmem->bitmap->Release ();

  g_free (dmem);
}

static GstMemory *
gst_dwrite_bitmap_allocator_share (GstMemory * mem, gssize offset, gssize size)
{
  return nullptr;
}

static gpointer
gst_dwrite_bitmap_allocator_map (GstMemory * mem, GstMapInfo * info,
    gsize maxsize)
{
  GstDWriteBitmapMemory *dmem = (GstDWriteBitmapMemory *) mem;
  ComPtr < IWICBitmapLock > bitmap_lock;
  HRESULT hr;
  WICRect rect = { 0, 0, dmem->info.width, dmem->info.height };
  DWORD map_flags = 0;
  BYTE *ptr = nullptr;
  UINT size;

  info->user_data[0] = nullptr;

  if ((info->flags & GST_MAP_READ) == GST_MAP_READ)
    map_flags |= (gint) WICBitmapLockRead;

  if ((info->flags & GST_MAP_WRITE) == GST_MAP_WRITE)
    map_flags |= (gint) WICBitmapLockWrite;


  hr = dmem->bitmap->Lock (&rect, map_flags, &bitmap_lock);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (mem->allocator,
        "Couldn't map bitmap, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  hr = bitmap_lock->GetDataPointer (&size, &ptr);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (mem->allocator, "Couldn't get data pointer, hr: 0x%x",
        (guint) hr);
    return nullptr;
  }

  info->user_data[0] = (gpointer) bitmap_lock.Detach ();

  return (gpointer) ptr;
}

static void
gst_dwrite_bitmap_allocator_unmap (GstMemory * mem, GstMapInfo * info)
{
  IWICBitmapLock *bitmap_lock;

  bitmap_lock = (IWICBitmapLock *) info->user_data[0];
  if (!bitmap_lock) {
    GST_WARNING_OBJECT (mem->allocator, "No attached bitmap lock");
    return;
  }

  bitmap_lock->Release ();
  info->user_data[0] = nullptr;
}

GstDWriteBitmapAllocator *
gst_dwrite_bitmap_allocator_new (void)
{
  GstDWriteBitmapAllocator *self = (GstDWriteBitmapAllocator *)
      g_object_new (GST_TYPE_DWRITE_BITMAP_ALLOCATOR, nullptr);

  gst_object_ref_sink (self);

  if (!self->priv->factory) {
    gst_object_unref (self);
    return nullptr;
  }

  return self;
}

GstMemory *
gst_dwrite_bitmap_allocator_alloc (GstDWriteBitmapAllocator * alloc,
    guint width, guint height)
{
  HRESULT hr;
  IWICImagingFactory *factory = alloc->priv->factory;
  ComPtr < IWICBitmap > bitmap;
  ComPtr < IWICBitmapLock > bitmap_lock;
  WICRect rect = { 0, 0, (INT) width, (INT) height };
  guint stride = width * 4;
  GstDWriteBitmapMemory *mem;
  gsize size;

  hr = factory->CreateBitmap (width, height, GUID_WICPixelFormat32bppPBGRA,
      WICBitmapCacheOnDemand, &bitmap);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (alloc, "Couldn't create bitmap, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  hr = bitmap->Lock (&rect, WICBitmapLockRead, &bitmap_lock);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (alloc, "Couldn't lock bitmap, hr: 0x%x", (guint) hr);
    return nullptr;
  }

  bitmap_lock = nullptr;
  size = stride * height;

  mem = g_new0 (GstDWriteBitmapMemory, 1);
  gst_memory_init (GST_MEMORY_CAST (mem), (GstMemoryFlags) 0,
      GST_ALLOCATOR_CAST (alloc), nullptr, size, 0, 0, size);

  gst_video_info_set_format (&mem->info, GST_VIDEO_FORMAT_BGRA, width, height);
  mem->info.size = size;
  mem->info.stride[0] = stride;

  mem->bitmap = bitmap.Detach ();

  return GST_MEMORY_CAST (mem);
}
