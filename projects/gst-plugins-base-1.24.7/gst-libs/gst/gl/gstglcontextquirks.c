/*
 * GStreamer
 * Copyright (C) 2022 Matthew Waters <matthew@centricular.com>
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
# include "config.h"
#endif

#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext_private.h>
#include "gstglfuncs.h"

#define GST_CAT_DEFAULT gst_gl_context_debug

#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

void
gst_gl_context_apply_quirks (GstGLContext * context)
{
  GstGLFuncs *gl = context->gl_vtable;
  const char *gl_vendor, *gl_renderer;

  gl_vendor = (const char *) gl->GetString (GL_VENDOR);
  gl_renderer = (const char *) gl->GetString (GL_RENDERER);

  /* Does not implement OES_vertex_array_object properly, see
   * https://bugzilla.gnome.org/show_bug.cgi?id=750185 */
  if (g_strcmp0 (gl_vendor, "Imagination Technologies") == 0
      && g_strcmp0 (gl_renderer, "PowerVR SGX 544MP") == 0) {
    gl->GenVertexArrays = NULL;
    gl->DeleteVertexArrays = NULL;
    gl->BindVertexArray = NULL;
    gl->IsVertexArray = NULL;
  }
  /* doesn't support timer queries without a complete framebuffer.
   * If the default framebuffer is backed by a surfaceless context, then the
   * default framebuffer is always incomplete and timer queries to time
   * upload/downloads will fail with GL errors. */
  if (g_strcmp0 (gl_vendor, "ARM") == 0
      && g_strcmp0 (gl_renderer, "Mali-G52") == 0
      && gl->CheckFramebufferStatus) {
    gint current_fbo;
    GLenum fbo_ret;

    gl->GetIntegerv (GL_FRAMEBUFFER_BINDING, &current_fbo);
    if (current_fbo != 0)
      gl->BindFramebuffer (GL_FRAMEBUFFER, 0);

    fbo_ret = gl->CheckFramebufferStatus (GL_FRAMEBUFFER);
    if (fbo_ret != GL_FRAMEBUFFER_COMPLETE) {
      GST_FIXME_OBJECT (context, "default framebuffer is not complete "
          "(is 0x%x) on ARM Mali-G52 which doesn't support timer queries with "
          "an incomplete framebuffer object, disabling timer queries", fbo_ret);
      gl->GenQueries = NULL;
      gl->BeginQuery = NULL;
      gl->EndQuery = NULL;
      gl->QueryCounter = NULL;
      gl->DeleteQueries = NULL;
      gl->IsQuery = NULL;
      gl->GetQueryiv = NULL;
      gl->GetQueryObjectiv = NULL;
      gl->GetQueryObjectuiv = NULL;
      gl->GetQueryObjecti64v = NULL;
      gl->GetQueryObjectui64v = NULL;
    }

    if (current_fbo != 0)
      gl->BindFramebuffer (GL_FRAMEBUFFER, current_fbo);
  }
}
