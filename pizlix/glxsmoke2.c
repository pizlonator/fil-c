/* Minimal reproducer: does a DrawElements after the standard DrawArrays
 * render test rasterize?  Same pipeline as glxsmoke.c's render test. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

#ifndef EGL_PLATFORM_GBM
#define EGL_PLATFORM_GBM 0x31D7
#endif

static void fail(const char *what)
{
   fprintf(stderr, "GLXSMOKE2 FAIL: %s (egl error 0x%04x)\n", what,
           eglGetError());
   exit(1);
}

static void probe(const char *name, int indexed, GLuint prog, GLint a_pos,
                  GLuint vbo, int stride)
{
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glDisableVertexAttribArray(a_pos);
   glEnableVertexAttribArray(a_pos);
   glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, stride,
                         (const void *) 0);
   glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   if (indexed) {
      GLuint my_ibo = 0;
      glGenBuffers(1, &my_ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, my_ibo);
      const GLubyte idx[3] = { 0, 1, 2 };
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx,
                   GL_STATIC_DRAW);
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, 0);
      glDeleteBuffers(1, &my_ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   } else {
      glDrawArrays(GL_TRIANGLES, 0, 3);
   }
   glFinish();
   unsigned char px[4];
   glReadPixels(128, 76, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
   printf("%s: err=0x%04x pixel=(%u,%u,%u,%u) %s\n", name, glGetError(),
          px[0], px[1], px[2], px[3],
          (px[0] == 255 && px[1] == 0) ? "DREW" : "MISSING");
   }


int main(int argc, char **argv)
{
   const char *force = argc > 1 ? argv[1] : NULL;
   if (force)
      setenv("GALLIUM_DRIVER", force, 1);

   int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      perror("open card0");
      return 1;
   }
   struct gbm_device *gbm = gbm_create_device(fd);
   if (!gbm)
      fail("gbm_create_device");
   EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM, gbm, NULL);
   if (dpy == EGL_NO_DISPLAY)
      fail("eglGetPlatformDisplay");
   EGLint maj, min;
   if (!eglInitialize(dpy, &maj, &min))
      fail("eglInitialize");
   eglBindAPI(EGL_OPENGL_ES_API);
   const EGLint cfg_attrs[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE
   };
   EGLConfig cfg;
   EGLint ncfg = 0;
   if (!eglChooseConfig(dpy, cfg_attrs, &cfg, 1, &ncfg) || ncfg < 1)
      fail("eglChooseConfig");
   const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
   if (ctx == EGL_NO_CONTEXT)
      fail("eglCreateContext");
   EGLint vid = GBM_FORMAT_XRGB8888;
   eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_ID, &vid);
   struct gbm_surface *gs = gbm_surface_create(gbm, 256, 256,
                                               (uint32_t) vid,
                                               GBM_BO_USE_RENDERING);
   if (!gs)
      fail("gbm_surface_create");
   EGLSurface surf = eglCreatePlatformWindowSurface(dpy, cfg, gs, NULL);
   if (surf == EGL_NO_SURFACE)
      fail("eglCreatePlatformWindowSurface");
   if (!eglMakeCurrent(dpy, surf, surf, ctx))
      fail("eglMakeCurrent");

   printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));

   static const char *vs_src =
      "attribute vec2 a_pos;\n"
      "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
   static const char *fs_src =
      "precision mediump float;\n"
      "uniform vec4 u_color;\n"
      "void main() { gl_FragColor = u_color; }\n";
   GLuint vs = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vs, 1, &vs_src, NULL);
   glCompileShader(vs);
   GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fs, 1, &fs_src, NULL);
   glCompileShader(fs);
   GLuint prog = glCreateProgram();
   glAttachShader(prog, vs);
   glAttachShader(prog, fs);
   glLinkProgram(prog);
   GLint linked = 0;
   glGetProgramiv(prog, GL_LINK_STATUS, &linked);
   if (!linked)
      fail("link");
   glUseProgram(prog);
   glUniform4f(glGetUniformLocation(prog, "u_color"), 1.0f, 0.0f, 0.0f,
               1.0f);

   GLuint vbo = 0, ibo = 0;
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   const float verts[6] = { -0.5f, -0.5f,  0.5f, -0.5f,  0.0f, 0.5f };
   glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
   GLint a_pos = glGetAttribLocation(prog, "a_pos");
   glEnableVertexAttribArray(a_pos);
   glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

   /* replicate glxsmoke.c's extra program creation BEFORE any draw */
   {
      GLuint vs4 = glCreateShader(GL_VERTEX_SHADER);
      static const char *vs4_src =
         "attribute vec2 a_p;\n"
         "attribute vec4 a_c;\n"
         "void main() { gl_Position = vec4(a_p, 0.0, 1.0); }\n";
      GLuint fs4 = glCreateShader(GL_FRAGMENT_SHADER);
      static const char *fs4_src =
         "precision mediump float;\n"
         "void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
      glShaderSource(vs4, 1, &vs4_src, NULL);
      glCompileShader(vs4);
      glShaderSource(fs4, 1, &fs4_src, NULL);
      glCompileShader(fs4);
      GLuint p4 = glCreateProgram();
      glAttachShader(p4, vs4); glAttachShader(p4, fs4);
      glBindAttribLocation(p4, 0, "a_p");
      glBindAttribLocation(p4, 1, "a_c");
      glLinkProgram(p4);
      GLint lk = 0;
      glGetProgramiv(p4, GL_LINK_STATUS, &lk);
      printf("p4 link: %d\n", lk);

      GLuint vs3 = glCreateShader(GL_VERTEX_SHADER);
      static const char *vs3_src =
         "attribute vec2 a_p;\n"
         "varying vec4 v_c;\n"
         "void main() { gl_Position = vec4(a_p, 0.0, 1.0); "
         "v_c = vec4(0.0, 1.0, 0.0, 1.0); }\n";
      GLuint fs3 = glCreateShader(GL_FRAGMENT_SHADER);
      static const char *fs3_src =
         "precision mediump float;\n"
         "varying vec4 v_c;\n"
         "void main() { gl_FragColor = v_c; }\n";
      glShaderSource(vs3, 1, &vs3_src, NULL);
      glCompileShader(vs3);
      glShaderSource(fs3, 1, &fs3_src, NULL);
      glCompileShader(fs3);
      GLuint p3 = glCreateProgram();
      glAttachShader(p3, vs3); glAttachShader(p3, fs3);
      glBindAttribLocation(p3, 0, "a_p");
      glLinkProgram(p3);
      glGetProgramiv(p3, GL_LINK_STATUS, &lk);
      printf("p3 link: %d\n", lk);

      GLuint vs2 = glCreateShader(GL_VERTEX_SHADER);
      static const char *vs2_src =
         "attribute vec2 a_p;\n"
         "attribute vec4 a_c;\n"
         "varying vec4 v_c;\n"
         "void main() { gl_Position = vec4(a_p, 0.0, 1.0); v_c = a_c; }\n";
      GLuint fs2 = glCreateShader(GL_FRAGMENT_SHADER);
      static const char *fs2_src =
         "precision mediump float;\n"
         "varying vec4 v_c;\n"
         "void main() { gl_FragColor = v_c; }\n";
      glShaderSource(vs2, 1, &vs2_src, NULL);
      glCompileShader(vs2);
      glShaderSource(fs2, 1, &fs2_src, NULL);
      glCompileShader(fs2);
      GLuint p2 = glCreateProgram();
      glAttachShader(p2, vs2); glAttachShader(p2, fs2);
      glBindAttribLocation(p2, 0, "a_p");
      glBindAttribLocation(p2, 1, "a_c");
      glLinkProgram(p2);
      glGetProgramiv(p2, GL_LINK_STATUS, &lk);
      printf("p2 link: %d\n", lk);
   }

   GLuint vbo3 = 0;
   glGenBuffers(1, &vbo3);
   glBindBuffer(GL_ARRAY_BUFFER, vbo3);
   const float verts3[15] = {
      -0.5f, -0.5f,  0, 0, 0,
       0.5f, -0.5f,  0, 0, 0,
       0.0f,  0.5f,  0, 0, 0,
   };
   glBufferData(GL_ARRAY_BUFFER, sizeof verts3, verts3, GL_STATIC_DRAW);

   glViewport(0, 0, 256, 256);

   probe("drawarrays-packed", 0, prog, a_pos, vbo, 0);
   probe("drawelements-packed", 1, prog, a_pos, vbo, 0);
   probe("drawarrays-strided", 0, prog, a_pos, vbo3, 20);
   probe("drawelements-strided", 1, prog, a_pos, vbo3, 20);
   probe("AGAIN-packed-da", 0, prog, a_pos, vbo, 0);
   probe("AGAIN-packed-de", 1, prog, a_pos, vbo, 0);
   probe("AGAIN-strided-da", 0, prog, a_pos, vbo3, 20);
   probe("AGAIN-strided-de", 1, prog, a_pos, vbo3, 20);

   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(dpy, ctx);
   eglDestroySurface(dpy, surf);
   eglTerminate(dpy);
   gbm_device_destroy(gbm);
   close(fd);
   return 0;
}
