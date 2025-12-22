/* GStreamer Wayland Library
 *
 * Copyright (C) 2014 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

/* GstWlBuffer wraps wl_buffer and provides a mechanism for preventing
 * buffers from being re-used while the compositor is using them. This
 * is achieved by adding a reference to the GstBuffer as soon as its
 * associated wl_buffer is sent to the compositor and by removing this
 * reference as soon as the compositor sends a wl_buffer::release message.
 *
 * This mechanism is a bit complicated, though, because it adds cyclic
 * references that can be dangerous. The reference cycles looks like:
 *
 *   ----------------
 *   | GstWlDisplay | ---------------------------->
 *   ----------------                              |
 *                                                 |
 *                                                 V
 *   -----------------     -------------     ---------------
 *   | GstBufferPool | --> | GstBuffer | ==> | GstWlBuffer |
 *   |               | <-- |           | <-- |             |
 *   -----------------     -------------     ---------------
 *
 * A GstBufferPool normally holds references to its GstBuffers and each buffer
 * holds a reference to a GstWlBuffer (saved in the GstMiniObject weak ref data).
 * When a GstBuffer is in use, it holds a reference back to the pool and the
 * pool doesn't hold a reference to the GstBuffer. When the GstBuffer is unrefed
 * externally, it returns back to the pool and the pool holds again a reference
 * to the buffer.
 *
 * Now when the compositor is using a buffer, the GstWlBuffer also holds a ref
 * to the GstBuffer, which prevents it from returning to the pool. When the
 * last GstWlBuffer receives a release event and unrefs the last GstBuffer,
 * the GstBufferPool will be able to stop and if no-one is holding a strong
 * ref to it, it will be destroyed. This will destroy the pool's GstBuffers and
 * also the GstWlBuffers. This will all happen in the same context of the last
 * gst_buffer_unref, which will be called from the buffer_release() callback.
 *
 * The problem here lies in the fact that buffer_release() will be called
 * from the event loop thread of GstWlDisplay, so it's as if the display
 * holds a reference to the GstWlBuffer, but without having an actual reference.
 * When we kill the display, there is no way for the GstWlBuffer, the associated
 * GstBuffer and the GstBufferPool to get destroyed, so we are going to leak a
 * fair amount of memory.
 *
 * Normally, this rarely happens, because the compositor releases buffers
 * almost immediately and when waylandsink stops, they are already released.
 *
 * However, we want to be absolutely certain, so a solution is introduced
 * by registering all the GstWlBuffers with the display and explicitly
 * releasing all the buffer references as soon as the display is destroyed.
 *
 * When the GstWlDisplay is finalized, it takes a reference to all the
 * registered GstWlBuffers and then calls gst_wl_buffer_force_release_and_unref,
 * which releases the potential reference to the GstBuffer, destroys the
 * underlying wl_buffer and removes the reference that GstWlDisplay is holding.
 * At that point, either the GstBuffer is alive somewhere and still holds a ref
 * to the GstWlBuffer, which it will release when it gets destroyed, or the
 * GstBuffer was destroyed in the meantime and the GstWlBuffer gets destroyed
 * as soon as we remove the reference that GstWlDisplay holds.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwlbuffer.h"

#define GST_CAT_DEFAULT gst_wl_buffer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct _GstWlBufferPrivate
{
  struct wl_buffer *wlbuffer;
  GstBuffer *current_gstbuffer;
  GstMemory *gstmem;

  GstWlDisplay *display;

  gboolean used_by_compositor;
} GstWlBufferPrivate;

G_DEFINE_TYPE_WITH_CODE (GstWlBuffer, gst_wl_buffer, G_TYPE_OBJECT,
    G_ADD_PRIVATE (GstWlBuffer)
    GST_DEBUG_CATEGORY_INIT (gst_wl_buffer_debug,
        "wlbuffer", 0, "wlbuffer library");
    );

static void
gst_wl_buffer_dispose (GObject * gobject)
{
  GstWlBuffer *self = GST_WL_BUFFER (gobject);
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  GST_TRACE_OBJECT (self, "dispose");

  /* if the display is shutting down and we are trying to dipose
   * the GstWlBuffer from another thread, unregister_buffer() will
   * block and in the end the display will increase the refcount
   * of this GstWlBuffer, so it will not be finalized */
  if (priv->display) {
    gst_wl_display_unregister_buffer (priv->display, priv->gstmem);
  }

  G_OBJECT_CLASS (gst_wl_buffer_parent_class)->dispose (gobject);
}

static void
gst_wl_buffer_finalize (GObject * gobject)
{
  GstWlBuffer *self = GST_WL_BUFFER (gobject);
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  GST_TRACE_OBJECT (self, "finalize");

  if (priv->wlbuffer)
    wl_buffer_destroy (priv->wlbuffer);

  G_OBJECT_CLASS (gst_wl_buffer_parent_class)->finalize (gobject);
}

static void
gst_wl_buffer_class_init (GstWlBufferClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->dispose = gst_wl_buffer_dispose;
  object_class->finalize = gst_wl_buffer_finalize;
}

static void
gst_wl_buffer_init (GstWlBuffer * self)
{
}

static void
buffer_release (void *data, struct wl_buffer *wl_buffer)
{
  GstWlBuffer *self = data;
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);
  GstBuffer *buf = priv->current_gstbuffer;

  GST_LOG_OBJECT (self, "wl_buffer::release (GstBuffer: %p)", buf);

  priv->used_by_compositor = FALSE;
  priv->current_gstbuffer = NULL;

  /* unref should be last, because it may end up destroying the GstWlBuffer */
  gst_buffer_unref (buf);
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release
};

static void
gstmemory_disposed (GstWlBuffer * self)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  g_assert (!priv->used_by_compositor);

  GST_TRACE_OBJECT (self, "owning GstMemory was finalized");

  /* this will normally destroy the GstWlBuffer, unless the display is
   * finalizing and it has taken an additional reference to it */
  g_object_unref (self);
}

GstWlBuffer *
gst_buffer_add_wl_buffer (GstBuffer * gstbuffer, struct wl_buffer *wlbuffer,
    GstWlDisplay * display)
{
  GstWlBuffer *self;
  GstWlBufferPrivate *priv;

  self = g_object_new (GST_TYPE_WL_BUFFER, NULL);
  priv = gst_wl_buffer_get_instance_private (self);
  priv->current_gstbuffer = gstbuffer;
  priv->wlbuffer = wlbuffer;
  priv->display = display;
  priv->gstmem = gst_buffer_peek_memory (gstbuffer, 0);

  gst_wl_display_register_buffer (priv->display, priv->gstmem, self);

  wl_buffer_add_listener (priv->wlbuffer, &buffer_listener, self);

  gst_mini_object_weak_ref (GST_MINI_OBJECT (priv->gstmem),
      (GstMiniObjectNotify) gstmemory_disposed, self);


  return self;
}

GstWlBuffer *
gst_buffer_get_wl_buffer (GstWlDisplay * display, GstBuffer * gstbuffer)
{
  GstMemory *mem0;
  GstWlBuffer *wlbuf;

  if (!gstbuffer)
    return NULL;

  mem0 = gst_buffer_peek_memory (gstbuffer, 0);

  wlbuf = gst_wl_display_lookup_buffer (display, mem0);
  if (wlbuf) {
    GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (wlbuf);

    priv->current_gstbuffer = gstbuffer;
  }

  return wlbuf;
}

void
gst_wl_buffer_force_release_and_unref (GstBuffer * buf, GstWlBuffer * self)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  /* Force a buffer release.
   * At this point, the GstWlDisplay has killed its event loop,
   * so we don't need to worry about buffer_release() being called
   * at the same time from the event loop thread */
  if (priv->used_by_compositor) {
    GST_DEBUG_OBJECT (self, "forcing wl_buffer::release (GstBuffer: %p)",
        priv->current_gstbuffer);
    priv->used_by_compositor = FALSE;
    gst_buffer_unref (priv->current_gstbuffer);
  }

  /* Finalize this GstWlBuffer early.
   * This method has been called as a result of the display shutting down,
   * so we need to stop using any wayland resources and disconnect from
   * the display. The GstWlBuffer stays alive, though, to avoid race
   * conditions with the GstBuffer being destroyed from another thread.
   * The last reference is either owned by the GstBuffer or by us and
   * it will be released at the end of this function. */
  GST_TRACE_OBJECT (self, "finalizing early");
  wl_buffer_destroy (priv->wlbuffer);
  priv->wlbuffer = NULL;
  priv->display = NULL;
  priv->current_gstbuffer = NULL;

  /* remove the reference that the caller (GstWlDisplay) owns */
  g_object_unref (self);
}

void
gst_wl_buffer_attach (GstWlBuffer * self, struct wl_surface *surface)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  if (priv->used_by_compositor) {
    GST_DEBUG_OBJECT (self, "buffer used by compositor %p",
        priv->current_gstbuffer);
    return;
  }

  wl_surface_attach (surface, priv->wlbuffer, 0, 0);

  /* Add a reference to the buffer. This represents the fact that
   * the compositor is using the buffer and it should not return
   * back to the pool and be re-used until the compositor releases it. */
  gst_buffer_ref (priv->current_gstbuffer);
  priv->used_by_compositor = TRUE;
}

void
gst_wl_buffer_ref_gst_buffer (GstWlBuffer * self)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);
  gst_buffer_ref (priv->current_gstbuffer);
}

void
gst_wl_buffer_unref_buffer (GstWlBuffer * self)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);
  gst_buffer_unref (priv->current_gstbuffer);
}

GstWlDisplay *
gst_wl_buffer_get_display (GstWlBuffer * self)
{
  GstWlBufferPrivate *priv = gst_wl_buffer_get_instance_private (self);

  return priv->display;
}
