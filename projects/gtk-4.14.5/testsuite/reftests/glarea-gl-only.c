#include <gtk/gtk.h>
#include <epoxy/gl.h>

/* A png for a red/yellow checkerboard */
static const char image_data[] = {
  0211, 0120, 0116, 0107, 0015, 0012, 0032, 0012, 0000, 0000, 0000, 0015, 0111, 0110, 0104, 0122,
  0000, 0000, 0000, 0040, 0000, 0000, 0000, 0040, 0001, 0003, 0000, 0000, 0000, 0111, 0264, 0350,
  0267, 0000, 0000, 0000, 0006, 0120, 0114, 0124, 0105, 0377, 0000, 0000, 0377, 0340, 0000, 0241,
  0105, 0325, 0002, 0000, 0000, 0000, 0025, 0111, 0104, 0101, 0124, 0010, 0327, 0143, 0230, 0011,
  0004, 0014, 0151, 0100, 0000, 0041, 0300, 0334, 0101, 0044, 0006, 0000, 0355, 0275, 0077, 0301,
  0347, 0173, 0153, 0007, 0000, 0000, 0000, 0000, 0111, 0105, 0116, 0104, 0256, 0102, 0140, 0202
};

G_MODULE_EXPORT gboolean
render_orange_glonly (GtkWidget    *glarea,
                      GdkGLContext *context)
{
  GdkTexture *texture;
  GdkTextureDownloader *downloader;
  GBytes *bytes; 
  gsize stride, width, height;
  GLuint tex_id, fb_id;

  gdk_gl_context_make_current (context);

  /* Clear to green, so that errors in the following code cause a problem */
  glClearColor (0.0, 1.0, 0.0, 1.0);
  glClear (GL_COLOR_BUFFER_BIT);

  /* load the checkerboard image */
  bytes = g_bytes_new_static (image_data, G_N_ELEMENTS (image_data));
  texture = gdk_texture_new_from_bytes (bytes, NULL);
  g_bytes_unref (bytes);
  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  downloader = gdk_texture_downloader_new (texture);
  /* Make sure we use a format that GLES does *NOT* support.
   * And that must include extensions.
   * But GL_EXT_texture_norm16 does support RGB16 as a source, which
   * is why we also use mipmaps below. */
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R16G16B16);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  g_object_unref (texture);
  gdk_texture_downloader_free (downloader);

  glGenTextures (1, &tex_id);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, tex_id);
  /* Now load the image in this ideally unsupported format. Maybe
   * things fail already here. Usually they don't. */
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB16,
                width, height,
                0, GL_RGB, GL_UNSIGNED_SHORT, g_bytes_get_data (bytes, NULL));
  g_bytes_unref (bytes);
  /* Generate mipmaps. GLES should give up now.
   * GL should turn the checkerboard into orange mipmap levels though.
   */
  glGenerateMipmap (GL_TEXTURE_2D);

  glGenFramebuffers (1, &fb_id);
  glBindFramebuffer (GL_READ_FRAMEBUFFER, fb_id);
  /* Bind mipmap level 2 for reading, so we rely on properly converted
   * mipmaps. */
  glFramebufferTexture2D (GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 2);

  /* On GLES, this should now fail due to incomplete framebuffer and
   * leave us with the green contents we've drawn above.
   * Or we are on GL, everything works perfectly, and we now get orange. */
  glBlitFramebuffer (0, 0,
                     width / 4, height / 4,
                     0, 0,
                     gtk_widget_get_width (glarea) * gtk_widget_get_scale_factor (glarea),
                     gtk_widget_get_height (glarea) * gtk_widget_get_scale_factor (glarea),
                     GL_COLOR_BUFFER_BIT,
                     GL_LINEAR);

  glDeleteFramebuffers (1, &fb_id);
  glDeleteTextures (1, &tex_id);

  return TRUE;
}

