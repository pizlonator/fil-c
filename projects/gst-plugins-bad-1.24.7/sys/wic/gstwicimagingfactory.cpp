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

#include "gstwicimagingfactory.h"
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_wic_imaging_factory_debug);
#define GST_CAT_DEFAULT gst_wic_imaging_factory_debug

struct _GstWicImagingFactory
{
  GstObject parent;

  IWICImagingFactory *handle;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;
};

static void gst_wic_imaging_factory_constructed (GObject * object);
static void gst_wic_imaging_factory_finalize (GObject * object);
static gpointer gst_wic_imaging_factory_func (GstWicImagingFactory * self);

#define gst_wic_imaging_factory_parent_class parent_class
G_DEFINE_TYPE (GstWicImagingFactory, gst_wic_imaging_factory, GST_TYPE_OBJECT);

static void
gst_wic_imaging_factory_class_init (GstWicImagingFactoryClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gst_wic_imaging_factory_constructed;
  object_class->finalize = gst_wic_imaging_factory_finalize;
}

static void
gst_wic_imaging_factory_init (GstWicImagingFactory * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->context = g_main_context_new ();
  self->loop = g_main_loop_new (self->context, FALSE);
}

static void
gst_wic_imaging_factory_constructed (GObject * object)
{
  GstWicImagingFactory *self = GST_WIC_IMAGING_FACTORY (object);

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstWicImagingFactory",
      (GThreadFunc) gst_wic_imaging_factory_func, self);
  while (!g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gst_wic_imaging_factory_finalize (GObject * object)
{
  GstWicImagingFactory *self = GST_WIC_IMAGING_FACTORY (object);

  if (self->loop) {
    g_main_loop_quit (self->loop);
    g_thread_join (self->thread);

    g_main_loop_unref (self->loop);
    g_main_context_unref (self->context);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
loop_running_cb (GstWicImagingFactory * self)
{
  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static gpointer
gst_wic_imaging_factory_func (GstWicImagingFactory * self)
{
  HRESULT hr;
  ComPtr < IWICImagingFactory > factory;
  GSource *idle_source;

  CoInitializeEx (nullptr, COINIT_MULTITHREADED);

  g_main_context_push_thread_default (self->context);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source,
      (GSourceFunc) loop_running_cb, self, nullptr);
  g_source_attach (idle_source, self->context);
  g_source_unref (idle_source);

  hr = CoCreateInstance (CLSID_WICImagingFactory, nullptr,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&factory));

  if (FAILED (hr)) {
    GST_ERROR_OBJECT (self, "Failed to create factory handle, hr: 0x%x",
        (guint) hr);
  } else {
    self->handle = factory.Detach ();
  }

run_loop:
  g_main_loop_run (self->loop);

  if (self->handle)
    self->handle->Release ();

  g_main_context_pop_thread_default (self->context);
  CoUninitialize ();

  return nullptr;
}

GstWicImagingFactory *
gst_wic_imaging_factory_new (void)
{
  GstWicImagingFactory *self;

  self = (GstWicImagingFactory *) g_object_new (GST_TYPE_WIC_IMAGING_FACTORY,
      nullptr);

  if (!self->handle) {
    gst_object_unref (self);
    return nullptr;
  }

  gst_object_ref_sink (self);

  return self;
}

IWICImagingFactory *
gst_wic_imaging_factory_get_handle (GstWicImagingFactory * factory)
{
  g_return_val_if_fail (GST_IS_WIC_IMAGING_FACTORY (factory), nullptr);

  return factory->handle;
}

HRESULT
gst_wic_imaging_factory_check_codec_support (GstWicImagingFactory * factory,
    gboolean is_decoder, REFGUID codec_id)
{
  HRESULT hr = E_FAIL;

  g_return_val_if_fail (GST_IS_WIC_IMAGING_FACTORY (factory), E_FAIL);

  if (is_decoder) {
    ComPtr < IWICBitmapDecoder > decoder;
    hr = factory->handle->CreateDecoder (codec_id, nullptr, &decoder);
  } else {
    ComPtr < IWICBitmapEncoder > encoder;
    hr = factory->handle->CreateEncoder (codec_id, nullptr, &encoder);
  }

  return hr;
}
