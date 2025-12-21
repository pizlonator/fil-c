/*
 * GStreamer
 * Copyright (C) 2018 Carlos Rafael Giani <dv@pseudoterminal.org>
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

#include <poll.h>

#include "../gstgl_fwd.h"
#include <gst/gl/gstglcontext.h>
#include <gst/gl/egl/gstglcontext_egl.h>

#include "gstgldisplay_gbm.h"
#include "gstglwindow_gbm_egl.h"
#include "gstgl_gbm_utils.h"
#include "../gstglwindow_private.h"

#define GST_CAT_DEFAULT gst_gl_window_debug


#define GST_GL_WINDOW_GBM_EGL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
    GST_TYPE_GL_WINDOW_GBM_EGL, GstGLWindowGBMEGLPrivate))


G_DEFINE_TYPE (GstGLWindowGBMEGL, gst_gl_window_gbm_egl, GST_TYPE_GL_WINDOW);


static guintptr gst_gl_window_gbm_egl_get_window_handle (GstGLWindow * window);
static guintptr gst_gl_window_gbm_egl_get_display (GstGLWindow * window);
static void gst_gl_window_gbm_egl_set_window_handle (GstGLWindow * window,
    guintptr handle);
static void gst_gl_window_gbm_egl_close (GstGLWindow * window);
static void gst_gl_window_gbm_egl_draw (GstGLWindow * window);

static gboolean gst_gl_window_gbm_init_surface (GstGLWindowGBMEGL * window_egl);

static void
gst_gl_window_gbm_egl_class_init (GstGLWindowGBMEGLClass * klass)
{
  GstGLWindowClass *window_class = (GstGLWindowClass *) klass;

  window_class->get_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_gbm_egl_get_window_handle);
  window_class->get_display =
      GST_DEBUG_FUNCPTR (gst_gl_window_gbm_egl_get_display);
  window_class->set_window_handle =
      GST_DEBUG_FUNCPTR (gst_gl_window_gbm_egl_set_window_handle);
  window_class->close = GST_DEBUG_FUNCPTR (gst_gl_window_gbm_egl_close);
  window_class->draw = GST_DEBUG_FUNCPTR (gst_gl_window_gbm_egl_draw);

  /* TODO: add support for set_render_rectangle (assuming this functionality
   * is possible with libdrm/gbm) */
}


static void
gst_gl_window_gbm_egl_init (GstGLWindowGBMEGL * window_gbm)
{
  window_gbm->gbm_surf = NULL;
  window_gbm->current_bo = NULL;
  window_gbm->prev_bo = NULL;
  window_gbm->waiting_for_flip = 0;
}


static guintptr
gst_gl_window_gbm_egl_get_window_handle (GstGLWindow * window)
{
  return (guintptr) GST_GL_WINDOW_GBM_EGL (window)->gbm_surf;
}


static guintptr
gst_gl_window_gbm_egl_get_display (GstGLWindow * window)
{
  return gst_gl_display_get_handle (window->display);
}


static void
gst_gl_window_gbm_egl_set_window_handle (G_GNUC_UNUSED GstGLWindow * window,
    G_GNUC_UNUSED guintptr handle)
{
  /* TODO: Currently, it is unclear how to use external GBM buffer objects,
   * since it is not defined how this would work together with DRM page flips
   */
}


static void
gst_gl_window_gbm_egl_close (GstGLWindow * window)
{
  GstGLWindowGBMEGL *window_egl = GST_GL_WINDOW_GBM_EGL (window);

  if (window_egl->saved_crtc) {
    GstGLDisplayGBM *display = (GstGLDisplayGBM *) window->display;
    drmModeCrtc *crtc = window_egl->saved_crtc;
    gint err;

    err = drmModeSetCrtc (display->drm_fd, crtc->crtc_id, crtc->buffer_id,
        crtc->x, crtc->y, &(display->drm_mode_connector->connector_id), 1,
        &crtc->mode);
    if (err)
      GST_ERROR_OBJECT (window, "Failed to restore previous CRTC mode: %s",
          g_strerror (errno));

    drmModeFreeCrtc (crtc);
    window_egl->saved_crtc = NULL;
  }

  if (window_egl->gbm_surf != NULL) {
    if (window_egl->current_bo != NULL) {
      gbm_surface_release_buffer (window_egl->gbm_surf, window_egl->current_bo);
      window_egl->current_bo = NULL;
    }

    gbm_surface_destroy (window_egl->gbm_surf);
    window_egl->gbm_surf = NULL;
  }

  GST_GL_WINDOW_CLASS (gst_gl_window_gbm_egl_parent_class)->close (window);
}


static void
_page_flip_handler (G_GNUC_UNUSED int fd, G_GNUC_UNUSED unsigned int frame,
    G_GNUC_UNUSED unsigned int sec, G_GNUC_UNUSED unsigned int usec, void *data)
{
  /* If we reach this point, it means the page flip has been completed.
   * Signal this by clearing the flag so the poll() loop in draw_cb()
   * can exit. */
  int *waiting_for_flip = data;
  *waiting_for_flip = 0;
}

static void
draw_cb (gpointer data)
{
  GstGLWindowGBMEGL *window_egl = data;
  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContextClass *context_class = GST_GL_CONTEXT_GET_CLASS (context);
  GstGLDisplayGBM *display = (GstGLDisplayGBM *) window->display;
  struct gbm_bo *next_bo;
  GstGLDRMFramebuffer *framebuf;
  int ret;

  drmEventContext evctx = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = _page_flip_handler,
  };

  struct pollfd pfd = {
    .fd = display->drm_fd,
    .events = POLLIN,
    .revents = 0,
  };

  /* No display connected */
  if (!display->drm_mode_info) {
    GST_ERROR ("No display connected");
    gst_object_unref (context);
    return;
  };

  /* Rendering, page flipping etc. are connect this way:
   *
   * The frames are stored in buffer objects (BOs). Inside the eglSwapBuffers()
   * call, GBM creates new BOs if necessary. BOs can be "locked" for rendering,
   * meaning that EGL cannot use them as a render target. If all available
   * BOs are locked, the GBM code inside eglSwapBuffers() creates a new,
   * unlocked one. We make use of this to implement triple buffering.
   *
   * There are 3 BOs in play:
   *
   * * next_bo: The BO we just rendered into.
   * * current_bo: The currently displayed BO.
   * * prev_bo: The previously displayed BO.
   *
   * current_bo and prev_bo are involed in page flipping. next_bo is not.
   *
   * Once rendering is done, the next_bo is retrieved and locked. Then, we 
   * wait until any ongoing page flipping finishes. Once it does, the
   * current_bo is displayed on screen, and the prev_bo isn't anymore. At
   * this point, it is safe to release the prev_bo, which unlocks it and
   * makes it available again as a render target. Then we initiate the
   * next page flipping; this time, we flip to next_bo. At that point,
   * next_bo becomes current_bo, and current_bo becomes prev_bo.
   */

  /*
   * There is a special case at the beginning. There is no currently
   * displayed BO at first, so we create an empty one to get the page
   * flipping cycle going. Also, we use this first BO for setting up
   * the CRTC.
   */
  if (window_egl->current_bo == NULL) {
    /* Call eglSwapBuffers() to create a BO. */
    context_class->swap_buffers (context);

    /* Lock the BO so we get our first current_bo. */
    window_egl->current_bo =
        gbm_surface_lock_front_buffer (window_egl->gbm_surf);
    framebuf = gst_gl_gbm_drm_fb_get_from_bo (window_egl->current_bo);

    /* Save the CRTC state */
    if (!window_egl->saved_crtc)
      window_egl->saved_crtc =
          drmModeGetCrtc (display->drm_fd, display->crtc_id);

    /* Configure CRTC to show this first BO. */
    ret = drmModeSetCrtc (display->drm_fd, display->crtc_id, framebuf->fb_id,
        0, 0, &(display->drm_mode_connector->connector_id), 1,
        display->drm_mode_info);

    if (ret != 0) {
      GST_ERROR ("Could not set DRM CRTC: %s (%d)", g_strerror (errno), errno);
      gst_object_unref (context);
      /* XXX: it is not possible to communicate the error to the pipeline */
      return;
    }
  }

  if (window->queue_resize) {
    guint width, height;

    gst_gl_window_get_surface_dimensions (window, &width, &height);
    gst_gl_window_resize (window, width, height);
  }

  /* Do the actual drawing */
  if (window->draw)
    window->draw (window->draw_data);

  /* Let the context class call eglSwapBuffers(). As mentioned above,
   * if necessary, this function creates a new unlocked framebuffer
   * that can be used as render target. */
  context_class->swap_buffers (context);
  gst_object_unref (context);

  next_bo = gbm_surface_lock_front_buffer (window_egl->gbm_surf);
  framebuf = gst_gl_gbm_drm_fb_get_from_bo (next_bo);
  GST_LOG ("rendered new frame into bo %p", (gpointer) next_bo);

  /* Wait until any ongoing page flipping is done. After this is done,
   * prev_bo is no longer involved in any page flipping, and can be
   * safely released. */
  while (window_egl->waiting_for_flip) {
    ret = poll (&pfd, 1, -1);
    if (ret < 0) {
      if (errno == EINTR)
        GST_DEBUG ("Signal caught during poll() call");
      else
        GST_ERROR ("poll() failed: %s (%d)", g_strerror (errno), errno);
      /* XXX: it is not possible to communicate errors and interruptions
       * to the pipeline */
      return;
    }

    drmHandleEvent (display->drm_fd, &evctx);
  }
  GST_LOG ("now showing bo %p", (gpointer) (window_egl->current_bo));

  /* Release prev_bo, since it is no longer shown on screen. */
  if (G_LIKELY (window_egl->prev_bo != NULL)) {
    gbm_surface_release_buffer (window_egl->gbm_surf, window_egl->prev_bo);
    GST_LOG ("releasing bo %p", (gpointer) (window_egl->prev_bo));
  }

  /* Presently, current_bo is shown on screen. Schedule the next page
   * flip, this time flip to next_bo. The flip happens asynchronously, so
   * we can continue and render etc. in the meantime. */
  window_egl->waiting_for_flip = 1;
  ret = drmModePageFlip (display->drm_fd, display->crtc_id, framebuf->fb_id,
      DRM_MODE_PAGE_FLIP_EVENT, &(window_egl->waiting_for_flip));
  if (ret != 0) {
    /* NOTE: According to libdrm sources, the page is _not_
     * considered flipped if drmModePageFlip() reports an error,
     * so we do not update the priv->current_bo pointer here */
    GST_ERROR ("Could not initialize GBM surface");
    /* XXX: it is not possible to communicate the error to the pipeline */
    return;
  }

  /* At this point, we relabel the current_bo as the prev_bo.
   * This may not actually be the case yet, but it will be soon - latest
   * when the wait loop above finishes.
   * Also, next_bo becomes current_bo. */
  window_egl->prev_bo = window_egl->current_bo;
  window_egl->current_bo = next_bo;
}


static void
gst_gl_window_gbm_egl_draw (GstGLWindow * window)
{
  gst_gl_window_send_message (window, (GstGLWindowCB) draw_cb, window);
}


static gboolean
gst_gl_window_gbm_init_surface (GstGLWindowGBMEGL * window_egl)
{
  /* NOTE: This function cannot be called in the open() vmethod
   * since context_egl->egl_display and context_egl->egl_config
   * must have been set to valid values at this point, and open()
   * is called _before_ these are set.
   * Also, eglInitialize() is called _after_ the open() vmethod,
   * which means that the return value of gbm_surface_create()
   * contains some function pointers that are set to NULL and
   * shouldn't be. This is because Mesa's eglInitialize() loads
   * the DRI2 driver and the relevant functions aren't available
   * until then.
   *
   * Therefore, this function is called instead inside
   * gst_gl_window_gbm_egl_create_window(), which in turn is
   * called inside gst_gl_context_egl_create_context(). */

  GstGLWindow *window = GST_GL_WINDOW (window_egl);
  GstGLDisplayGBM *display = (GstGLDisplayGBM *) window->display;
  drmModeModeInfo *drm_mode_info = display->drm_mode_info;
  GstGLContext *context = gst_gl_window_get_context (window);
  GstGLContextEGL *context_egl = GST_GL_CONTEXT_EGL (context);
  EGLint gbm_format;
  int hdisplay, vdisplay;
  gboolean ret = TRUE;

  if (drm_mode_info) {
    vdisplay = drm_mode_info->vdisplay;
    hdisplay = drm_mode_info->hdisplay;
  } else {
    vdisplay = 0;
    hdisplay = 0;
  }

  /* With GBM-based EGL displays and configs, the native visual ID
   * is a GBM pixel format. */
  if (!eglGetConfigAttrib (context_egl->egl_display, context_egl->egl_config,
          EGL_NATIVE_VISUAL_ID, &gbm_format)) {
    GST_ERROR ("eglGetConfigAttrib failed: %s",
        gst_egl_get_error_string (eglGetError ()));
    ret = FALSE;
    goto cleanup;
  }

  /* Create a GBM surface that shall contain the BOs we are
   * going to render into. */
  window_egl->gbm_surf = gbm_surface_create (display->gbm_dev,
      hdisplay, vdisplay, gbm_format,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  gst_gl_window_resize (window, hdisplay, vdisplay);
  gst_gl_window_queue_resize (window);

  GST_DEBUG ("Successfully created GBM surface %ix%i from info %p", hdisplay,
      vdisplay, drm_mode_info);

cleanup:

  gst_object_unref (context);
  return ret;
}

/* Must be called in the gl thread */
GstGLWindowGBMEGL *
gst_gl_window_gbm_egl_new (GstGLDisplay * display)
{
  GstGLWindowGBMEGL *window_egl;

  if ((gst_gl_display_get_handle_type (display) & GST_GL_DISPLAY_TYPE_GBM) == 0)
    /* we require a GBM display to create windows */
    return NULL;

  window_egl = g_object_new (GST_TYPE_GL_WINDOW_GBM_EGL, NULL);

  return window_egl;
}


gboolean
gst_gl_window_gbm_egl_create_window (GstGLWindowGBMEGL * window_egl)
{
  return gst_gl_window_gbm_init_surface (window_egl);
}
