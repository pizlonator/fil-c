#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#if GST_GL_HAVE_PLATFORM_EGL
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <gst/gl/egl/gstglcontext_egl.h>
#include <gst/gl/egl/gstglmemoryegl.h>
#endif

#if GST_GL_HAVE_WINDOW_X11
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#if GST_GL_HAVE_WINDOW_WAYLAND
#include <gst/gl/wayland/gstgldisplay_wayland.h>
#endif

