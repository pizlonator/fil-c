/* Fil-C llvmpipe smoke: GBM/EGL bring-up + render + timing.
 *
 * Built and run inside the Pizlix chroot:
 *   cc -enable-filc-jit -o glxsmoke glxsmoke.c -lEGL -lgbm -lGL
 * The -enable-filc-jit is REQUIRED: llvmpipe's LLVMContextCreate goes
 * through the Fil-C LLVM JIT API, whose gate (filc_allow_jit) is only set
 * by the JIT CRT entry point.
 *
 * Flow: /dev/dri/card0 -> gbm_device -> eglGetPlatformDisplay(EGL_PLATFORM_GBM)
 * -> eglInitialize -> config/context/surface -> GL_RENDERER must contain
 * "llvmpipe (LLVM 20.1.8" -> clear + one triangle through the fixed-
 * function pipeline (which JIT-compiles shader variants) -> read back and
 * verify pixels -> 100-frame clear+swap timing loop.
 */

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
/* EGL_PLATFORM_GBM_KHR / EGL_PLATFORM_GBM_MESA: 0x31D7 */
#define EGL_PLATFORM_GBM 0x31D7
#endif

static void fail(const char *what)
{
   fprintf(stderr, "GLXSMOKE FAIL: %s (egl error 0x%04x)\n", what,
           eglGetError());
   exit(1);
}

static int verify_pixel(int x, int y, unsigned char r, unsigned char g,
                        unsigned char b, const char *what)
{
   unsigned char px[4];
   glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
   if (px[0] != r || px[1] != g || px[2] != b) {
      fprintf(stderr,
              "GLXSMOKE FAIL: %s at (%d,%d): got (%u,%u,%u), want (%u,%u,%u)\n",
              what, x, y, px[0], px[1], px[2], r, g, b);
      return 0;
   }
   return 1;
}



int main(int argc, char **argv)
{
   const char *force = argc > 1 ? argv[1] : NULL;
   if (force)
      setenv("GALLIUM_DRIVER", force, 1);

   int ok = 1;
   int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      perror("GLXSMOKE: open /dev/dri/card0");
      return 1;
   }

   struct gbm_device *gbm = gbm_create_device(fd);
   if (!gbm)
      fail("gbm_create_device\n");

   eglInitialize(eglGetDisplay(EGL_DEFAULT_DISPLAY), NULL, NULL); /* warm-up */

   EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM, gbm, NULL);
   if (dpy == EGL_NO_DISPLAY)
      fail("eglGetPlatformDisplay(EGL_PLATFORM_GBM)\n");

   EGLint maj, min;
   if (!eglInitialize(dpy, &maj, &min))
      fail("eglInitialize\n");
   printf("EGL %d.%d initialized\n", maj, min);
   printf("EGL_CLIENT_APIS: %s\n", eglQueryString(dpy, EGL_CLIENT_APIS));

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
      fail("eglChooseConfig\n");

   const EGLint ctx_attrs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
   if (ctx == EGL_NO_CONTEXT)
      fail("eglCreateContext\n");

   /* Fil-C: the gbm surface format must match the config's native visual,
    * or eglCreatePlatformWindowSurface fails with EGL_BAD_MATCH. */
   EGLint vid = GBM_FORMAT_XRGB8888;
   eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_ID, &vid);
   struct gbm_surface *gs = gbm_surface_create(gbm, 256, 256,
                                               (uint32_t)vid,
                                               GBM_BO_USE_RENDERING);
   if (!gs)
      fail("gbm_surface_create\n");

   EGLSurface surf = eglCreatePlatformWindowSurface(dpy, cfg, gs, NULL);
   if (surf == EGL_NO_SURFACE)
      fail("eglCreatePlatformWindowSurface\n");

   if (!eglMakeCurrent(dpy, surf, surf, ctx))
      fail("eglMakeCurrent\n");

   const char *renderer = (const char *)glGetString(GL_RENDERER);
   const char *version = (const char *)glGetString(GL_VERSION);
   const char *vendor = (const char *)glGetString(GL_VENDOR);
   printf("GL_RENDERER: %s\n", renderer);
   printf("GL_VERSION:  %s\n", version);
   printf("GL_VENDOR:   %s\n", vendor);

   ok = ok && strstr(renderer, "llvmpipe (LLVM 20.1.8") != NULL;

   printf("native vector width: %s\n",
          strstr(renderer, "256 bits") ? "256"
          : strstr(renderer, "512 bits") ? "512" : "other");

   /* ---- render test: clear + one GLES2 shader triangle ---- */

   /* First thing: does a trivial VS with an UNUSED attribute compile?
    * (probe for a GLSL-compiler-side regression independent of JIT). */
   {
      static const char *vs_unused_src =
         "attribute vec2 a_used;\n"
         "attribute vec4 a_unused;\n"
         "void main() { gl_Position = vec4(a_used, 0.0, 1.0); }\n";
      GLuint vsu = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(vsu, 1, &vs_unused_src, NULL);
      glCompileShader(vsu);
      GLint cs = 0;
      char log[512];
      glGetShaderiv(vsu, GL_COMPILE_STATUS, &cs);
      glGetShaderInfoLog(vsu, sizeof log, NULL, log);
      printf("unused-attrib VS compile: %d log[%s]\n", cs, log);
      static const char *vs_used_src =
         "attribute vec2 a_used;\n"
         "attribute vec4 a_unused;\n"
         "void main() { gl_Position = vec4(a_used, 0.0, 1.0) + a_unused; }\n";
      GLuint vso = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(vso, 1, &vs_used_src, NULL);
      glCompileShader(vso);
      glGetShaderiv(vso, GL_COMPILE_STATUS, &cs);
      glGetShaderInfoLog(vso, sizeof log, NULL, log);
      printf("used-attrib VS compile: %d log[%s]\n", cs, log);
   }

   /* prog4: 2 inputs (pos+col), 1 output (position); FS constant. */
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
   GLuint prog4 = glCreateProgram();
   glAttachShader(prog4, vs4);
   glAttachShader(prog4, fs4);
   glBindAttribLocation(prog4, 0, "a_p");
   glBindAttribLocation(prog4, 1, "a_c");
   glLinkProgram(prog4);

   /* prog3: 1 input (pos), 2 outputs (position + constant varying). */
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
   GLuint prog3 = glCreateProgram();
   glAttachShader(prog3, vs3);
   glAttachShader(prog3, fs3);
   glBindAttribLocation(prog3, 0, "a_p");
   glLinkProgram(prog3);

   /* prog2: 2 inputs + varying (the shell/cogl shape). */
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
   GLuint prog2 = glCreateProgram();
   glAttachShader(prog2, vs2);
   glAttachShader(prog2, fs2);
   glBindAttribLocation(prog2, 0, "a_p");
   glBindAttribLocation(prog2, 1, "a_c");
   glLinkProgram(prog2);

   {
      GLint cs = 0;
      char log[256];
      glGetShaderiv(vs4, GL_COMPILE_STATUS, &cs);
      printf("vs4 compile: %d\n", cs);
      glGetShaderiv(vs3, GL_COMPILE_STATUS, &cs);
      printf("vs3 compile: %d\n", cs);
      glGetShaderiv(vs2, GL_COMPILE_STATUS, &cs);
      printf("vs2 compile: %d\n", cs);
      glGetProgramiv(prog4, GL_LINK_STATUS, &cs);
      glGetProgramInfoLog(prog4, sizeof log, NULL, log);
      printf("prog4 link: %d log[%s]\n", cs, log);
      glGetProgramiv(prog3, GL_LINK_STATUS, &cs);
      glGetProgramInfoLog(prog3, sizeof log, NULL, log);
      printf("prog3 link: %d log[%s]\n", cs, log);
      glGetProgramiv(prog2, GL_LINK_STATUS, &cs);
      glGetProgramInfoLog(prog2, sizeof log, NULL, log);
      printf("prog2 link: %d log[%s]\n", cs, log);
   }

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
   if (!linked) {
      char log[512];
      glGetProgramInfoLog(prog, sizeof log, NULL, log);
      fprintf(stderr, "GLXSMOKE: program link failed: %s\n", log);
      return 1;
   }
   glUseProgram(prog);
   glUniform4f(glGetUniformLocation(prog, "u_color"), 1.0f, 0.0f, 0.0f, 1.0f);

   GLuint vbo = 0;
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   const float verts[6] = { -0.5f, -0.5f,  0.5f, -0.5f,  0.0f, 0.5f };
   glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
   GLint a_pos = glGetAttribLocation(prog, "a_pos");
   glEnableVertexAttribArray(a_pos);
   glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

   glViewport(0, 0, 256, 256);
   glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   glFinish();
   ok = ok && verify_pixel(8, 8, 0, 255, 0, "clear");

   glDrawArrays(GL_TRIANGLES, 0, 3);
   glFinish();
   ok = ok && verify_pixel(128, 76, 255, 0, 0, "triangle");
   ok = ok && verify_pixel(8, 8, 0, 255, 0, "triangle-vs-clear");
   /* ---- timing: 100 frames of clear + triangle + glFinish ---- */
   struct timespec t0, t1;
   clock_gettime(CLOCK_MONOTONIC, &t0);
   for (int i = 0; i < 100; i++) {
      glClearColor((i % 256) / 255.0f, 1.0f, 0.0f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glFinish();
   }
   clock_gettime(CLOCK_MONOTONIC, &t1);
   double ms = (t1.tv_sec - t0.tv_sec) * 1000.0
             + (t1.tv_nsec - t0.tv_nsec) / 1e6;
   printf("TIMING driver=%s 100 frames (clear+triangle+finish): %.1f ms "
          "(%.3f ms/frame)\n",
          force ? force : "default", ms, ms / 100.0);

   /* ---- indexed draw matrix (sprint 3) ----
    * Exercises the draw module's index-buffer fetch (gallivm's AVX2 gather
    * path, which the shim lowers into predicated plain loads) plus
    * multi-input/multi-output shader variants.  Every case binds its
    * vertex buffer and attribute state EXPLICITLY: GL attributes keep
    * referencing whatever buffer was bound when the pointer was set, and
    * deleting or rebinding without resetting the pointer makes subsequent
    * draws legitimately read an empty buffer. */
   {
      GLuint ibo = 0;
      glGenBuffers(1, &ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
      const GLubyte indices[3] = { 0, 1, 2 };
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices,
                   GL_STATIC_DRAW);

      struct __attribute__((packed)) vtx {
         float x, y;
         unsigned char r, g, b, a;
      };
      const float verts3[15] = {
         -0.5f, -0.5f,  0, 0, 0,
          0.5f, -0.5f,  0, 0, 0,
          0.0f,  0.5f,  0, 0, 0,
      };
      const unsigned char verts2[60] = {
         0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x00, 0xbf,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f,
         0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f,
         0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00,
      };
      GLuint vbo2 = 0, vbo3 = 0;
      glGenBuffers(1, &vbo2);
      glBindBuffer(GL_ARRAY_BUFFER, vbo2);
      glBufferData(GL_ARRAY_BUFFER, sizeof verts2, verts2, GL_STATIC_DRAW);
      glGenBuffers(1, &vbo3);
      glBindBuffer(GL_ARRAY_BUFFER, vbo3);
      glBufferData(GL_ARRAY_BUFFER, sizeof verts3, verts3, GL_STATIC_DRAW);

      /* prog4: 2 inputs (pos+col), 1 output (position); FS constant. */
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
      GLuint prog4 = glCreateProgram();
      glAttachShader(prog4, vs4);
      glAttachShader(prog4, fs4);
      glBindAttribLocation(prog4, 0, "a_p");
      glBindAttribLocation(prog4, 1, "a_c");
      glLinkProgram(prog4);

      /* prog3: 1 input (pos), 2 outputs (position + constant varying). */
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
      GLuint prog3 = glCreateProgram();
      glAttachShader(prog3, vs3);
      glAttachShader(prog3, fs3);
      glBindAttribLocation(prog3, 0, "a_p");
      glLinkProgram(prog3);

      /* prog5: same as prog4 but WITHOUT the unused a_c declaration:
       * one declared attribute, one vertex element. */
      GLuint vs5 = glCreateShader(GL_VERTEX_SHADER);
      static const char *vs5_src =
         "attribute vec2 a_p;\n"
         "void main() { gl_Position = vec4(a_p, 0.0, 1.0); }\n";
      glShaderSource(vs5, 1, &vs5_src, NULL);
      glCompileShader(vs5);
      GLuint prog5 = glCreateProgram();
      glAttachShader(prog5, vs5);
      glAttachShader(prog5, fs4);
      glBindAttribLocation(prog5, 0, "a_p");
      glLinkProgram(prog5);

      /* prog2: 2 inputs + varying (the shell/cogl shape). */
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
      GLuint prog2 = glCreateProgram();
      glAttachShader(prog2, vs2);
      glAttachShader(prog2, fs2);
      glBindAttribLocation(prog2, 0, "a_p");
      glBindAttribLocation(prog2, 1, "a_c");
      glLinkProgram(prog2);

#define DRAW_CASE(name, prog_, vbo_, with_color, stride_, er, eg, eb)      \
      do {                                                                  \
         glUseProgram(prog_);                                               \
         glDisableVertexAttribArray(0);                                     \
         glDisableVertexAttribArray(1);                                     \
         glBindBuffer(GL_ARRAY_BUFFER, vbo_);                               \
         GLint a_p_ = glGetAttribLocation(prog_, "a_p");                    \
         if (a_p_ < 0)                                                      \
            a_p_ = glGetAttribLocation(prog_, "a_pos");                     \
         if (a_p_ >= 0) {                                                   \
            glEnableVertexAttribArray(a_p_);                                \
            glVertexAttribPointer(a_p_, 2, GL_FLOAT, GL_FALSE, stride_,     \
                                  (const void *) 0);                        \
         }                                                                  \
         if (with_color) {                                                  \
            GLint a_c_ = glGetAttribLocation(prog_, "a_c");                 \
            if (a_c_ >= 0) {                                                \
               glEnableVertexAttribArray(a_c_);                             \
               glVertexAttribPointer(a_c_, 4, GL_UNSIGNED_BYTE, GL_TRUE,    \
                                     20, (const void *) 8);                 \
            }                                                               \
         }                                                                  \
         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);                        \
         glClearColor(1.0f, 0.0f, 1.0f, 1.0f);                              \
         glClear(GL_COLOR_BUFFER_BIT);                                      \
         glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, 0);              \
         glFinish();                                                        \
         unsigned char px_[4];                                              \
         glReadPixels(128, 76, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px_);       \
         int okc_ = px_[0] == (er) && px_[1] == (eg) && px_[2] == (eb);      \
         printf("case %-14s err=0x%04x pixel=(%u,%u,%u,%u) %s\n",           \
                name, glGetError(), px_[0], px_[1], px_[2], px_[3],         \
                okc_ ? "OK" : "UNEXPECTED");                                \
      } while (0)

      glUseProgram(prog);
      glUniform4f(glGetUniformLocation(prog, "u_color"), 0.0f, 1.0f, 1.0f,
                  1.0f);
      /* read back both uploads: are the driver-side copies intact? */
      {
         typedef void (*getbufsub_t)(GLenum, GLintptr, GLsizeiptr, void *);
         getbufsub_t getbufsub = (getbufsub_t)
            eglGetProcAddress("glGetBufferSubData");
         unsigned char rb2[60], rb3[60];
         if (!getbufsub)
            fail("no glGetBufferSubData");
         glBindBuffer(GL_ARRAY_BUFFER, vbo2);
         getbufsub(GL_ARRAY_BUFFER, 0, 60, rb2);
         glBindBuffer(GL_ARRAY_BUFFER, vbo3);
         getbufsub(GL_ARRAY_BUFFER, 0, 60, rb3);
         fprintf(stderr, "RB2:");
         for (unsigned ri = 0; ri < 24; ++ri)
            fprintf(stderr, " %02x", rb2[ri]);
         fprintf(stderr, "\nRB3:");
         for (unsigned ri = 0; ri < 24; ++ri)
            fprintf(stderr, " %02x", rb3[ri]);
         fprintf(stderr, "\n");
      }
      DRAW_CASE("packed-1in1out", prog, vbo, 0, 0, 0, 255, 255);
      DRAW_CASE("strided-1in1out", prog, vbo3, 0, 20, 0, 255, 255);
      DRAW_CASE("prog3-1in2out", prog3, vbo3, 0, 20, 0, 255, 0);
      DRAW_CASE("prog4-2in1out", prog4, vbo2, 1, 20, 255, 0, 0);
      /* (a) color = ONE GL_FLOAT at offset 8, stride 20 (scalar, no ub4) */
      {
         glUseProgram(prog4);
         glDisableVertexAttribArray(0);
         glDisableVertexAttribArray(1);
         glBindBuffer(GL_ARRAY_BUFFER, vbo2);
         GLint a_p4 = glGetAttribLocation(prog4, "a_p");
         GLint a_c4 = glGetAttribLocation(prog4, "a_c");
         glEnableVertexAttribArray(a_p4);
         glVertexAttribPointer(a_p4, 2, GL_FLOAT, GL_FALSE, 20,
                               (const void *) 0);
         glEnableVertexAttribArray(a_c4);
         glVertexAttribPointer(a_c4, 1, GL_FLOAT, GL_FALSE, 20,
                               (const void *) 8);
         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
         glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, 0);
         glFinish();
         unsigned char px4[4];
         glReadPixels(128, 76, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px4);
         printf("case %-14s err=0x%04x pixel=(%u,%u,%u,%u) %s\n",
                "prog4-float1col", glGetError(), px4[0], px4[1], px4[2],
                px4[3], "see below");
         glUseProgram(prog);
         glUniform4f(glGetUniformLocation(prog, "u_color"), 0.0f, 1.0f,
                     1.0f, 1.0f);
      }
      /* (b) color = ub4 in a SEPARATE buffer with stride 4 */
      {
         const GLubyte cols[12] = { 0, 0, 255, 255,  0, 0, 255, 255,
                                    0, 0, 255, 255 };
         GLuint vbo5 = 0;
         glGenBuffers(1, &vbo5);
         glBindBuffer(GL_ARRAY_BUFFER, vbo5);
         glBufferData(GL_ARRAY_BUFFER, sizeof cols, cols, GL_STATIC_DRAW);
         glUseProgram(prog4);
         glDisableVertexAttribArray(0);
         glDisableVertexAttribArray(1);
         glBindBuffer(GL_ARRAY_BUFFER, vbo3);
         GLint a_p4 = glGetAttribLocation(prog4, "a_p");
         GLint a_c4 = glGetAttribLocation(prog4, "a_c");
         glEnableVertexAttribArray(a_p4);
         glVertexAttribPointer(a_p4, 2, GL_FLOAT, GL_FALSE, 20,
                               (const void *) 0);
         glEnableVertexAttribArray(a_c4);
         glVertexAttribPointer(a_c4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4,
                               (const void *) 0);
         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
         glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, 0);
         glFinish();
         unsigned char px4[4];
         glReadPixels(128, 76, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px4);
         printf("case %-14s err=0x%04x pixel=(%u,%u,%u,%u) %s\n",
                "prog4-sepbcol", glGetError(), px4[0], px4[1], px4[2],
                px4[3],
                (px4[2] == 255 && px4[0] == 0) ? "OK" : "UNEXPECTED");
         glDeleteBuffers(1, &vbo5);
         glUseProgram(prog);
         glUniform4f(glGetUniformLocation(prog, "u_color"), 0.0f, 1.0f,
                     1.0f, 1.0f);
      }
      /* float4 color at stride 24: same 2-input shape, no ub4 unpack */
      {
         const float verts4[9] = {
            -0.5f, -0.5f,  0, 0, 1, 1,
             0.5f, -0.5f,  0, 0, 1, 1,
             0.0f,  0.5f,  0, 0, 1, 1,
         };
         GLuint vbo4 = 0;
         glGenBuffers(1, &vbo4);
         glBindBuffer(GL_ARRAY_BUFFER, vbo4);
         glBufferData(GL_ARRAY_BUFFER, sizeof verts4, verts4, GL_STATIC_DRAW);
         glUseProgram(prog4);
         glDisableVertexAttribArray(0);
         glDisableVertexAttribArray(1);
         glBindBuffer(GL_ARRAY_BUFFER, vbo4);
         GLint a_p4 = glGetAttribLocation(prog4, "a_p");
         GLint a_c4 = glGetAttribLocation(prog4, "a_c");
         glEnableVertexAttribArray(a_p4);
         glVertexAttribPointer(a_p4, 2, GL_FLOAT, GL_FALSE, 24,
                               (const void *) 0);
         glEnableVertexAttribArray(a_c4);
         glVertexAttribPointer(a_c4, 4, GL_FLOAT, GL_FALSE, 24,
                               (const void *) 8);
         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
         glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
         glClear(GL_COLOR_BUFFER_BIT);
         glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, 0);
         glFinish();
         unsigned char px4[4];
         glReadPixels(128, 76, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px4);
         printf("case %-14s err=0x%04x pixel=(%u,%u,%u,%u) %s\n",
                "prog4-floatcol", glGetError(), px4[0], px4[1], px4[2],
                px4[3],
                (px4[0] == 255 && px4[1] == 0) ? "OK" : "UNEXPECTED");
         glDeleteBuffers(1, &vbo4);
         glUseProgram(prog);
         glUniform4f(glGetUniformLocation(prog, "u_color"), 0.0f, 1.0f,
                     1.0f, 1.0f);
      }
      DRAW_CASE("prog5-vbo2", prog5, vbo2, 0, 20, 255, 0, 0);
      DRAW_CASE("prog1-vbo3-strided", prog, vbo3, 0, 20, 0, 255, 255);
      DRAW_CASE("prog5-vbo3", prog5, vbo3, 0, 20, 255, 0, 0);
      DRAW_CASE("prog1-vbo2", prog, vbo2, 0, 20, 0, 255, 255);
      DRAW_CASE("prog2-2in2out", prog2, vbo2, 1, 20, 0, 0, 255);
      glUseProgram(prog);
      glUniform4f(glGetUniformLocation(prog, "u_color"), 0.0f, 1.0f, 1.0f,
                  1.0f);
      DRAW_CASE("REPEAT-1in1out", prog, vbo3, 0, 20, 0, 255, 255);

      glDeleteBuffers(1, &vbo2);
      glDeleteBuffers(1, &vbo3);
      glDeleteBuffers(1, &ibo);
      glUseProgram(prog);
   }

   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(dpy, ctx);
   eglDestroySurface(dpy, surf);
   eglTerminate(dpy);
   gbm_device_destroy(gbm);
   close(fd);

   printf("%s\n", ok ? "GLXSMOKE PASS" : "GLXSMOKE FAIL");
   return ok ? 0 : 1;
}
