/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <matthew@centricular.com>
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

/**
 * SECTION:gstglsyncmeta
 * @title: GstGLSyncMeta
 * @short_description: synchronization primitives
 * @see_also: #GstGLBaseMemory, #GstGLContext
 *
 * #GstGLSyncMeta provides the ability to synchronize the OpenGL command stream
 * with the CPU or with other OpenGL contexts.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglsyncmeta.h"

#include "gstglcontext.h"
#include "gstglfuncs.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_sync_meta_debug);
#define GST_CAT_DEFAULT gst_gl_sync_meta_debug

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif
#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED 0x911B
#endif
#ifndef GL_TIMEOUT_IGNORED
#define GL_TIMEOUT_IGNORED G_GUINT64_CONSTANT(0xFFFFFFFFFFFFFFFF)
#endif

static void
_default_set_sync_gl (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (gl->FenceSync) {
    if (sync_meta->data) {
      GST_LOG ("deleting sync object %p", sync_meta->data);
      gl->DeleteSync ((GLsync) sync_meta->data);
    }
    sync_meta->data =
        (gpointer) gl->FenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GST_LOG ("setting sync object %p", sync_meta->data);
  }

  if (gst_gl_context_is_shared (context))
    gl->Flush ();
}

static void
_default_wait_gl (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (sync_meta->data && gl->WaitSync) {
    GST_LOG ("waiting on sync object %p", sync_meta->data);
    gl->WaitSync ((GLsync) sync_meta->data, 0, GL_TIMEOUT_IGNORED);
  }
}

static void
_default_wait_cpu_gl (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;
  GLenum res;

  if (sync_meta->data && gl->ClientWaitSync) {
    do {
      GST_LOG ("waiting on sync object %p", sync_meta->data);
      res =
          gl->ClientWaitSync ((GLsync) sync_meta->data,
          GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000 /* 1s */ );
    } while (res == GL_TIMEOUT_EXPIRED);
  } else {
    gl->Finish ();
  }
}

static void
_default_copy (GstGLSyncMeta * src, GstBuffer * sbuffer, GstGLSyncMeta * dest,
    GstBuffer * dbuffer)
{
  GST_LOG ("copy sync object %p from meta %p to %p", src->data, src, dest);

  /* Setting a sync point here relies on GstBuffer copying
   * metas after data */
  gst_gl_sync_meta_set_sync_point (src, src->context);
}

static void
_default_free_gl (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (sync_meta->data) {
    GST_LOG ("deleting sync object %p", sync_meta->data);
    gl->DeleteSync ((GLsync) sync_meta->data);
    sync_meta->data = NULL;
  }
}

/**
 * gst_buffer_add_gl_sync_meta_full:
 * @context: a #GstGLContext
 * @buffer: a #GstBuffer
 * @data: sync data to hold
 *
 * Returns: (transfer none): the #GstGLSyncMeta added to #GstBuffer
 *
 * Since: 1.8
 */
GstGLSyncMeta *
gst_buffer_add_gl_sync_meta_full (GstGLContext * context, GstBuffer * buffer,
    gpointer data)
{
  GstGLSyncMeta *meta;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);

  meta =
      (GstGLSyncMeta *) gst_buffer_add_meta ((buffer), GST_GL_SYNC_META_INFO,
      NULL);

  if (!meta)
    return NULL;

  meta->context = gst_object_ref (context);
  meta->data = data;

  return meta;
}

/**
 * gst_buffer_add_gl_sync_meta:
 * @context: a #GstGLContext
 * @buffer: a #GstBuffer
 *
 * Returns: (transfer none): the #GstGLSyncMeta added to #GstBuffer
 *
 * Since: 1.6
 */
GstGLSyncMeta *
gst_buffer_add_gl_sync_meta (GstGLContext * context, GstBuffer * buffer)
{
  GstGLSyncMeta *ret = gst_buffer_add_gl_sync_meta_full (context, buffer, NULL);
  if (!ret)
    return NULL;

  ret->set_sync_gl = _default_set_sync_gl;
  ret->wait_gl = _default_wait_gl;
  ret->wait_cpu_gl = _default_wait_cpu_gl;
  ret->copy = _default_copy;
  ret->free_gl = _default_free_gl;

  return ret;
}

static void
_set_sync_point (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->set_sync_gl != NULL);

  GST_LOG ("setting sync point %p", sync_meta);
  sync_meta->set_sync_gl (sync_meta, context);
}

/**
 * gst_gl_sync_meta_set_sync_point:
 * @sync_meta: a #GstGLSyncMeta
 * @context: a #GstGLContext
 *
 * Set a sync point to possibly wait on at a later time.
 *
 * Since: 1.6
 */
void
gst_gl_sync_meta_set_sync_point (GstGLSyncMeta * sync_meta,
    GstGLContext * context)
{
  if (sync_meta->set_sync)
    sync_meta->set_sync (sync_meta, context);
  else
    gst_gl_context_thread_add (context,
        (GstGLContextThreadFunc) _set_sync_point, sync_meta);
}

static void
_wait (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->wait_gl != NULL);

  GST_LOG ("waiting %p", sync_meta);
  sync_meta->wait_gl (sync_meta, context);
}

/**
 * gst_gl_sync_meta_wait:
 * @sync_meta: a #GstGLSyncMeta
 * @context: a #GstGLContext
 *
 * Insert a wait into @context's command stream ensuring all previous OpenGL
 * commands before @sync_meta have completed.
 *
 * Since: 1.6
 */
void
gst_gl_sync_meta_wait (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  if (sync_meta->wait)
    sync_meta->wait (sync_meta, context);
  else
    gst_gl_context_thread_add (context,
        (GstGLContextThreadFunc) _wait, sync_meta);
}

static void
_wait_cpu (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->wait_cpu_gl != NULL);

  GST_LOG ("waiting %p", sync_meta);
  sync_meta->wait_cpu_gl (sync_meta, context);
}

/**
 * gst_gl_sync_meta_wait_cpu:
 * @sync_meta: a #GstGLSyncMeta
 * @context: a #GstGLContext
 *
 * Perform a wait so that the sync point has passed from the CPU's perspective
 * What that means, is that all GL operations changing CPU-visible data before
 * the sync point are now visible.
 *
 * Since: 1.8
 */
void
gst_gl_sync_meta_wait_cpu (GstGLSyncMeta * sync_meta, GstGLContext * context)
{
  if (sync_meta->wait_cpu)
    sync_meta->wait_cpu (sync_meta, context);
  else
    gst_gl_context_thread_add (context,
        (GstGLContextThreadFunc) _wait_cpu, sync_meta);
}

static gboolean
_gst_gl_sync_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstGLSyncMeta *dmeta, *smeta;

  smeta = (GstGLSyncMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    g_assert (smeta->copy != NULL);

    if (!copy->region) {
      /* only copy if the complete data is copied as well */
      dmeta = gst_buffer_add_gl_sync_meta_full (smeta->context, dest, NULL);
      if (!dmeta)
        return FALSE;

      dmeta->set_sync = smeta->set_sync;
      dmeta->set_sync_gl = smeta->set_sync_gl;
      dmeta->wait = smeta->wait;
      dmeta->wait_gl = smeta->wait_gl;
      dmeta->wait_cpu = smeta->wait_cpu;
      dmeta->wait_cpu_gl = smeta->wait_cpu_gl;
      dmeta->copy = smeta->copy;
      dmeta->free = smeta->free;
      dmeta->free_gl = smeta->free_gl;

      GST_LOG ("copying sync meta %p into %p", smeta, dmeta);
      smeta->copy (smeta, buffer, dmeta, dest);
    }
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

static void
_free_gl_sync_meta (GstGLContext * context, GstGLSyncMeta * sync_meta)
{
  g_assert (sync_meta->free_gl != NULL);

  GST_LOG ("free sync meta %p", sync_meta);
  sync_meta->free_gl (sync_meta, context);
}

static void
_gst_gl_sync_meta_free (GstGLSyncMeta * sync_meta, GstBuffer * buffer)
{
  if (sync_meta->free)
    sync_meta->free (sync_meta, sync_meta->context);
  else
    gst_gl_context_thread_add (sync_meta->context,
        (GstGLContextThreadFunc) _free_gl_sync_meta, sync_meta);

  gst_object_unref (sync_meta->context);
}

static gboolean
_gst_gl_sync_meta_init (GstGLSyncMeta * sync_meta, gpointer params,
    GstBuffer * buffer)
{
  static gsize _init;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_sync_meta_debug, "glsyncmeta", 0,
        "glsyncmeta");
    g_once_init_leave (&_init, 1);
  }

  sync_meta->context = NULL;
  sync_meta->data = NULL;
  sync_meta->set_sync = NULL;
  sync_meta->set_sync_gl = NULL;
  sync_meta->wait = NULL;
  sync_meta->wait_gl = NULL;
  sync_meta->wait_cpu = NULL;
  sync_meta->wait_cpu_gl = NULL;
  sync_meta->copy = NULL;
  sync_meta->free = NULL;
  sync_meta->free_gl = NULL;

  return TRUE;
}

GType
gst_gl_sync_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstGLSyncMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }

  return type;
}

const GstMetaInfo *
gst_gl_sync_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter (&meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_GL_SYNC_META_API_TYPE, "GstGLSyncMeta",
        sizeof (GstGLSyncMeta), (GstMetaInitFunction) _gst_gl_sync_meta_init,
        (GstMetaFreeFunction) _gst_gl_sync_meta_free,
        _gst_gl_sync_meta_transform);
    g_once_init_leave (&meta_info, meta);
  }

  return meta_info;
}
