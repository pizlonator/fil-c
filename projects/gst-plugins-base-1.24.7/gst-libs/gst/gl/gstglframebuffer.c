/* 
 * GStreamer
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
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
 * SECTION:gstglframebuffer
 * @short_description: OpenGL framebuffer abstraction
 * @title: GstGLFramebuffer
 * @see_also: #GstGLBaseMemory, #GstGLMemory, #GstGLContext
 *
 * A #GstGLFramebuffer represents and holds an OpenGL framebuffer object with
 * it's associated attachments.
 *
 * A #GstGLFramebuffer can be created with gst_gl_framebuffer_new() or
 * gst_gl_framebuffer_new_with_default_depth() and bound with
 * gst_gl_framebuffer_bind().  Other resources can be bound with
 * gst_gl_framebuffer_attach()
 *
 * Note: OpenGL framebuffers are not shareable resources so cannot be used
 * between multiple OpenGL contexts.
 *
 * Since: 1.10
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglframebuffer.h"

#include "gstglcontext.h"
#include "gstglcontext_private.h"
#include "gstglfuncs.h"
#include "gstglmemory.h"
#include "gstglrenderbuffer.h"

#ifndef GL_FRAMEBUFFER_UNDEFINED
#define GL_FRAMEBUFFER_UNDEFINED          0x8219
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED
#define GL_FRAMEBUFFER_UNSUPPORTED        0x8CDD
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif

#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

GST_DEBUG_CATEGORY_STATIC (gst_gl_framebuffer_debug);
#define GST_CAT_DEFAULT gst_gl_framebuffer_debug

static void gst_gl_framebuffer_finalize (GObject * object);

struct _GstGLFramebufferPrivate
{
  guint effective_width;
  guint effective_height;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_framebuffer_debug, "glframebuffer", 0, "GL Framebuffer");

G_DEFINE_TYPE_WITH_CODE (GstGLFramebuffer, gst_gl_framebuffer, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstGLFramebuffer) DEBUG_INIT);

struct fbo_attachment
{
  guint attachment_point;
  GstGLBaseMemory *mem;
};

static void
_fbo_attachment_init (struct fbo_attachment *attach, guint point,
    GstGLBaseMemory * mem)
{
  attach->attachment_point = point;
  attach->mem = (GstGLBaseMemory *) gst_memory_ref (GST_MEMORY_CAST (mem));
}

static void
_fbo_attachment_unset (struct fbo_attachment *attach)
{
  if (!attach)
    return;

  if (attach->mem)
    gst_memory_unref (GST_MEMORY_CAST (attach->mem));
  attach->mem = NULL;
}

static void
gst_gl_framebuffer_class_init (GstGLFramebufferClass * klass)
{
  G_OBJECT_CLASS (klass)->finalize = gst_gl_framebuffer_finalize;
}

static void
gst_gl_framebuffer_init (GstGLFramebuffer * fb)
{
  fb->priv = gst_gl_framebuffer_get_instance_private (fb);

  fb->attachments =
      g_array_new (FALSE, FALSE, (sizeof (struct fbo_attachment)));
  g_array_set_clear_func (fb->attachments,
      (GDestroyNotify) _fbo_attachment_unset);
}

static void
_delete_fbo_gl (GstGLContext * context, GstGLFramebuffer * fb)
{
  const GstGLFuncs *gl = context->gl_vtable;

  if (fb->fbo_id)
    gl->DeleteFramebuffers (1, &fb->fbo_id);
  fb->fbo_id = 0;
}

static void
gst_gl_framebuffer_finalize (GObject * object)
{
  GstGLFramebuffer *fb = GST_GL_FRAMEBUFFER (object);

  if (fb->context) {
    if (fb->fbo_id)
      gst_gl_context_thread_add (fb->context,
          (GstGLContextThreadFunc) _delete_fbo_gl, fb);

    gst_object_unref (fb->context);
    fb->context = NULL;
  }

  if (fb->attachments)
    g_array_free (fb->attachments, TRUE);
  fb->attachments = NULL;

  G_OBJECT_CLASS (gst_gl_framebuffer_parent_class)->finalize (object);
}

/**
 * gst_gl_framebuffer_new:
 * @context: a #GstGLContext
 *
 * This function will internally create an OpenGL framebuffer object and must
 * be called on @context's OpenGL thread.
 *
 * Returns: (transfer full): a new #GstGLFramebuffer
 *
 * Since: 1.10
 */
GstGLFramebuffer *
gst_gl_framebuffer_new (GstGLContext * context)
{
  GstGLFramebuffer *fb;
  const GstGLFuncs *gl;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (gst_gl_context_get_current () == context, NULL);

  gl = context->gl_vtable;

  if (!gl->GenFramebuffers) {
    GST_ERROR_OBJECT (context, "Framebuffers are not supported!");
    return NULL;
  }

  fb = g_object_new (GST_TYPE_GL_FRAMEBUFFER, NULL);
  fb->context = gst_object_ref (context);
  gl->GenFramebuffers (1, &fb->fbo_id);
  gst_object_ref_sink (fb);

  return fb;
}

/**
 * gst_gl_framebuffer_new_with_default_depth:
 * @context: a #GstGLContext
 * @width: width for the depth buffer
 * @height: for the depth buffer
 *
 * This function will internally create an OpenGL framebuffer object and must
 * be called on @context's OpenGL thread.
 *
 * Returns: a new #GstGLFramebuffer with a depth buffer of @width and @height
 *
 * Since: 1.10
 */
GstGLFramebuffer *
gst_gl_framebuffer_new_with_default_depth (GstGLContext * context, guint width,
    guint height)
{
  GstGLFramebuffer *fb = gst_gl_framebuffer_new (context);
  GstGLBaseMemoryAllocator *render_alloc;
  GstGLAllocationParams *params;
  GstGLBaseMemory *renderbuffer;
  guint attach_point, attach_type;

  if (!fb)
    return NULL;

  if (gst_gl_context_get_gl_api (fb->context) & (GST_GL_API_OPENGL |
          GST_GL_API_OPENGL3)) {
    attach_point = GL_DEPTH_STENCIL_ATTACHMENT;
    attach_type = GST_GL_DEPTH24_STENCIL8;
  } else if (gst_gl_context_get_gl_api (fb->context) & GST_GL_API_GLES2) {
    attach_point = GL_DEPTH_ATTACHMENT;
    attach_type = GST_GL_DEPTH_COMPONENT16;
  } else {
    g_assert_not_reached ();
    return NULL;
  }

  render_alloc = (GstGLBaseMemoryAllocator *)
      gst_allocator_find (GST_GL_RENDERBUFFER_ALLOCATOR_NAME);
  params = (GstGLAllocationParams *)
      gst_gl_renderbuffer_allocation_params_new (context, NULL, attach_type,
      width, height);

  renderbuffer = gst_gl_base_memory_alloc (render_alloc, params);
  gst_gl_allocation_params_free (params);
  gst_object_unref (render_alloc);

  gst_gl_framebuffer_bind (fb);
  gst_gl_framebuffer_attach (fb, attach_point, renderbuffer);
  gst_gl_context_clear_framebuffer (fb->context);
  gst_memory_unref (GST_MEMORY_CAST (renderbuffer));

  return fb;
}

/**
 * gst_gl_framebuffer_draw_to_texture:
 * @fb: a #GstGLFramebuffer
 * @mem: the #GstGLMemory to draw to
 * @func: (scope call): the function to run
 * @user_data: data to pass to @func
 *
 * Perform the steps necessary to have the output of a glDraw* command in
 * @func update the contents of @mem.
 *
 * Note: this function does not map @mem for writing with OpenGL and that must
 * be done manually by the caller using any of the mapping functions such as
 * gst_memory_map() with the map flags %GST_MAP_WRITE | %GST_MAP_GL.
 *
 * Must be called with the same OpenGL context current that @fb was created
 * with.
 *
 * Returns: the result of executing @func
 *
 * Since: 1.10
 */
gboolean
gst_gl_framebuffer_draw_to_texture (GstGLFramebuffer * fb, GstGLMemory * mem,
    GstGLFramebufferFunc func, gpointer user_data)
{
  const GstGLFuncs *gl;
  gboolean ret;

  g_return_val_if_fail (GST_IS_GL_FRAMEBUFFER (fb), FALSE);
  g_return_val_if_fail (gst_is_gl_memory (GST_MEMORY_CAST (mem)), FALSE);
  g_return_val_if_fail (gst_gl_context_get_current () == fb->context, FALSE);

  gl = fb->context->gl_vtable;

  GST_TRACE_OBJECT (fb, "drawing to texture %u, dimensions %ix%i", mem->tex_id,
      gst_gl_memory_get_texture_width (mem),
      gst_gl_memory_get_texture_height (mem));

  gst_gl_framebuffer_bind (fb);
  gst_gl_framebuffer_attach (fb, GL_COLOR_ATTACHMENT0, (GstGLBaseMemory *) mem);

  gl->Viewport (0, 0, fb->priv->effective_width, fb->priv->effective_height);
  if (gst_gl_context_get_gl_api (fb->context) & (GST_GL_API_OPENGL |
          GST_GL_API_OPENGL3))
    gl->DrawBuffer (GL_COLOR_ATTACHMENT0);

  ret = func (user_data);

  if (gst_gl_context_get_gl_api (fb->context) & (GST_GL_API_OPENGL |
          GST_GL_API_OPENGL3))
    gl->DrawBuffer (GL_COLOR_ATTACHMENT0);
  gst_gl_context_clear_framebuffer (fb->context);

  return ret;
}

/**
 * gst_gl_framebuffer_bind:
 * @fb: a #GstGLFramebuffer
 *
 * Bind @fb into the current thread
 *
 * Must be called with the same OpenGL context current that @fb was created
 * with.
 *
 * Since: 1.10
 */
void
gst_gl_framebuffer_bind (GstGLFramebuffer * fb)
{
  const GstGLFuncs *gl;

  g_return_if_fail (GST_IS_GL_FRAMEBUFFER (fb));
  g_return_if_fail (gst_gl_context_get_current () == fb->context);
  g_return_if_fail (fb->fbo_id != 0);

  gl = fb->context->gl_vtable;

  gl->BindFramebuffer (GL_FRAMEBUFFER, fb->fbo_id);
}

/**
 * gst_gl_context_clear_framebuffer:
 * @context: a #GstGLContext
 *
 * Unbind the current framebuffer
 *
 * Since: 1.10
 */
void
gst_gl_context_clear_framebuffer (GstGLContext * context)
{
  const GstGLFuncs *gl;

  g_return_if_fail (GST_IS_GL_CONTEXT (context));

  gl = context->gl_vtable;

  gl->BindFramebuffer (GL_FRAMEBUFFER, 0);
}

static void
_update_effective_dimensions (GstGLFramebuffer * fb)
{
  int i;
  guint min_width = -1, min_height = -1;

  /* remove the previous attachment */
  for (i = 0; i < fb->attachments->len; i++) {
    struct fbo_attachment *attach;
    int width, height;

    attach = &g_array_index (fb->attachments, struct fbo_attachment, i);

    if (gst_is_gl_memory (GST_MEMORY_CAST (attach->mem))) {
      GstGLMemory *mem = (GstGLMemory *) attach->mem;

      width = gst_gl_memory_get_texture_width (mem);
      height = gst_gl_memory_get_texture_height (mem);
    } else if (gst_is_gl_renderbuffer (GST_MEMORY_CAST (attach->mem))) {
      GstGLRenderbuffer *mem = (GstGLRenderbuffer *) attach->mem;

      width = mem->width;
      height = mem->height;
    } else {
      g_assert_not_reached ();
    }

    if (width < min_width)
      min_width = width;
    if (height < min_height)
      min_height = height;
  }

  fb->priv->effective_width = min_width;
  fb->priv->effective_height = min_height;
}

static gboolean
_is_valid_attachment_point (guint attachment_point)
{
  /* all 31 possible color attachments */
  if (attachment_point >= 0x8CE0 && attachment_point <= 0x8CFF)
    return TRUE;

  /* depth-stencil attachment */
  if (attachment_point == 0x821A)
    return TRUE;

  /* depth attachment */
  if (attachment_point == 0x8D00)
    return TRUE;

  /* stencil attachment */
  if (attachment_point == 0x8D20)
    return TRUE;

  return FALSE;
}

static void
_attach_gl_memory (GstGLFramebuffer * fb, guint attachment_point,
    GstGLMemory * mem)
{
  struct fbo_attachment attach;
  const GstGLFuncs *gl = fb->context->gl_vtable;
  guint gl_target = gst_gl_texture_target_to_gl (mem->tex_target);

  gst_gl_framebuffer_bind (fb);

  gl->FramebufferTexture2D (GL_FRAMEBUFFER, attachment_point, gl_target,
      mem->tex_id, 0);

  _fbo_attachment_init (&attach, attachment_point, (GstGLBaseMemory *) mem);
  fb->attachments = g_array_append_val (fb->attachments, attach);
}

static void
_attach_renderbuffer (GstGLFramebuffer * fb, guint attachment_point,
    GstGLRenderbuffer * rb)
{
  struct fbo_attachment attach;
  const GstGLFuncs *gl = fb->context->gl_vtable;

  gst_gl_framebuffer_bind (fb);
  gl->BindRenderbuffer (GL_RENDERBUFFER, rb->renderbuffer_id);

  gl->FramebufferRenderbuffer (GL_FRAMEBUFFER, attachment_point,
      GL_RENDERBUFFER, rb->renderbuffer_id);

  _fbo_attachment_init (&attach, attachment_point, (GstGLBaseMemory *) rb);
  fb->attachments = g_array_append_val (fb->attachments, attach);
}

/**
 * gst_gl_framebuffer_attach:
 * @fb: a #GstGLFramebuffer
 * @attachment_point: the OpenGL attachment point to bind @mem to
 * @mem: the memory object to bind to @attachment_point
 *
 * attach @mem to @attachment_point
 *
 * Must be called with the same OpenGL context current that @fb was created
 * with.
 *
 * Since: 1.10
 */
void
gst_gl_framebuffer_attach (GstGLFramebuffer * fb, guint attachment_point,
    GstGLBaseMemory * mem)
{
  int i;

  g_return_if_fail (GST_IS_GL_FRAMEBUFFER (fb));
  g_return_if_fail (gst_gl_context_get_current () == fb->context);
  g_return_if_fail (_is_valid_attachment_point (attachment_point));

  /* remove the previous attachment */
  for (i = 0; i < fb->attachments->len; i++) {
    struct fbo_attachment *attach;

    attach = &g_array_index (fb->attachments, struct fbo_attachment, i);

    if (attach->attachment_point == attachment_point) {
      g_array_remove_index_fast (fb->attachments, i);
      break;
    }
  }

  if (gst_is_gl_memory (GST_MEMORY_CAST (mem))) {
    _attach_gl_memory (fb, attachment_point, (GstGLMemory *) mem);
  } else if (gst_is_gl_renderbuffer (GST_MEMORY_CAST (mem))) {
    _attach_renderbuffer (fb, attachment_point, (GstGLRenderbuffer *) mem);
  } else {
    g_assert_not_reached ();
    return;
  }

  _update_effective_dimensions (fb);
}

/**
 * gst_gl_framebuffer_get_effective_dimensions:
 * @fb: a #GstGLFramebuffer
 * @width: (out) (allow-none): output width
 * @height: (out) (allow-none): output height
 *
 * Retrieve the effective dimensions from the current attachments attached to
 * @fb.
 *
 * Since: 1.10
 */
void
gst_gl_framebuffer_get_effective_dimensions (GstGLFramebuffer * fb,
    guint * width, guint * height)
{
  g_return_if_fail (GST_IS_GL_FRAMEBUFFER (fb));

  if (width)
    *width = fb->priv->effective_width;
  if (height)
    *height = fb->priv->effective_height;
}

/**
 * gst_gl_context_check_framebuffer_status:
 * @context: a #GstGLContext
 * @fbo_target: the GL value of the framebuffer target, GL_FRAMEBUFFER,
 *              GL_READ_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER
 *
 * Must be called with @context current.
 *
 * Returns: whether whether the current framebuffer is complete
 *
 * Since: 1.10
 */
gboolean
gst_gl_context_check_framebuffer_status (GstGLContext * context,
    guint fbo_target)
{
  GLenum ret;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);
  g_return_val_if_fail (gst_gl_context_get_current () == context, FALSE);

  if (fbo_target != GL_FRAMEBUFFER && fbo_target != GL_READ_FRAMEBUFFER
      && fbo_target != GL_DRAW_FRAMEBUFFER) {
    GST_ERROR_OBJECT (context, "fbo target is invalid");
    return FALSE;
  }

  /* Don't do expensive framebuffer checks when debugging is disabled */
  if (!_gst_gl_context_debug_is_enabled (context))
    return TRUE;

  ret = context->gl_vtable->CheckFramebufferStatus (fbo_target);
  switch (ret) {
    case GL_FRAMEBUFFER_COMPLETE:
      return TRUE;
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      GST_WARNING_OBJECT (context, "GL_FRAMEBUFFER_UNSUPPORTED");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      GST_WARNING_OBJECT (context, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      GST_WARNING_OBJECT (context,
          "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      GST_WARNING_OBJECT (context, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS");
      break;
#if GST_GL_HAVE_OPENGL
    case GL_FRAMEBUFFER_UNDEFINED:
      GST_WARNING_OBJECT (context, "GL_FRAMEBUFFER_UNDEFINED");
      break;
#endif
    case 0:
      ret = context->gl_vtable->GetError ();
      GST_ERROR_OBJECT (context,
          "Error calling glCheckFramebufferStatus(0x%X): %d (0x%X)",
          fbo_target, ret, ret);
      break;
    default:
      GST_WARNING_OBJECT (context, "Unknown FBO status: %d (0x%X)", ret, ret);
      break;
  }

  return FALSE;
}

/**
 * gst_gl_framebuffer_get_id:
 * @fb: a #GstGLFramebuffer
 *
 * Returns: the OpenGL id for @fb
 *
 * Since: 1.10
 */
guint
gst_gl_framebuffer_get_id (GstGLFramebuffer * fb)
{
  g_return_val_if_fail (GST_IS_GL_FRAMEBUFFER (fb), 0);

  return fb->fbo_id;
}
