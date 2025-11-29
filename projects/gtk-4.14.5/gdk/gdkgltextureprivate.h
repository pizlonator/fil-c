#pragma once

#include "gdkgltexture.h"

#include "gdkgltexturebuilder.h"
#include "gdktextureprivate.h"

G_BEGIN_DECLS

GdkTexture *            gdk_gl_texture_new_from_builder (GdkGLTextureBuilder    *builder,
                                                         GDestroyNotify          destroy,
                                                         gpointer                data);


GdkGLContext *          gdk_gl_texture_get_context      (GdkGLTexture           *self);
guint                   gdk_gl_texture_get_id           (GdkGLTexture           *self);
gboolean                gdk_gl_texture_has_mipmap       (GdkGLTexture           *self);
gpointer                gdk_gl_texture_get_sync         (GdkGLTexture           *self);

G_END_DECLS

