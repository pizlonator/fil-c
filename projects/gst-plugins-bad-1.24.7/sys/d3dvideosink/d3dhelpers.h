/* GStreamer
 * Copyright (C) 2012 Roland Krikava <info@bluedigits.com>
 * Copyright (C) 2010-2011 David Hoyt <dhoyt@hoytsoft.org>
 * Copyright (C) 2010 Andoni Morales <ylatuya@gmail.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef _D3DHELPERS_H_
#define _D3DHELPERS_H_

#include <gst/gst.h>
#include <gst/video/video.h>

#include <windows.h>

#if defined(__MINGW32__)
# ifndef _OBJC_NO_COM_
#  if defined(__cplusplus) && !defined(CINTERFACE)
#   if defined(__GNUC__) &&  __GNUC__ < 3 && !defined(NOCOMATTRIBUTE)
#    define DECLARE_INTERFACE_IID_(i,b,d) _COM_interface __attribute__((com_interface)) i : public b
#   else
#    define DECLARE_INTERFACE_IID_(i,b,d) _COM_interface i : public b
#   endif
#  elif !defined(DECLARE_INTERFACE_IID_)
#   define DECLARE_INTERFACE_IID_(i,b,d) DECLARE_INTERFACE(i)
#  endif
# endif
# if !defined(__MSABI_LONG)
#  define __MSABI_LONG(x)  x ## l
# endif
#endif

#include <d3d9.h>

typedef struct _GstD3DVideoSink GstD3DVideoSink;
typedef struct _GstD3DVideoSinkClass GstD3DVideoSinkClass;

typedef struct _GstD3DDisplayDevice {
  UINT                   adapter;
  D3DFORMAT              format;
  D3DTEXTUREFILTERTYPE   filter_type;
  LPDIRECT3DDEVICE9      d3d_device;
  D3DPRESENT_PARAMETERS  present_params;
} GstD3DDisplayDevice;

typedef struct _GstD3DDataClass {
  guint                  refs;
  LPDIRECT3D9            d3d;
  GstD3DDisplayDevice    device;
  GList                  *supported_formats;

  /* Track individual sink instances */
  GList *                sink_list;
  gboolean               device_lost;

  /* Window class for internal windows */
  WNDCLASS               wnd_class;

  /* Windows Message Handling */
  GThread *              thread;
  GMutex                 thread_start_mutex;
  GCond                  thread_start_cond;
  HWND                   hidden_window;
  gboolean               thread_started;
  gboolean               thread_error_exit;
} GstD3DDataClass;

typedef struct _GstD3DData {
  /* Window Proc Stuff */
  HWND                   external_window_handle;
  HWND                   window_handle;
  gboolean               window_is_internal;
  WNDPROC                orig_wnd_proc;

  /* Render Constructs */
  LPDIRECT3DSWAPCHAIN9   swapchain;
  LPDIRECT3DSURFACE9     surface;
  D3DTEXTUREFILTERTYPE   filtertype;
  D3DFORMAT              format;
  GstVideoRectangle    * render_rect;
  gboolean               renderable;
  gboolean               device_lost;

  /* list of GstD3DVideoSinkOverlay structs */
  GList * overlay;
  gboolean overlay_needs_resize;
} GstD3DData;

gboolean       d3d_class_init(GstD3DVideoSink * klass);
void           d3d_class_destroy(GstD3DVideoSink * klass);

gboolean       d3d_prepare_window(GstD3DVideoSink * sink);
gboolean       d3d_stop(GstD3DVideoSink * sink);
void           d3d_set_window_handle(GstD3DVideoSink * sink, guintptr window_id, gboolean internal);
void           d3d_set_render_rectangle(GstD3DVideoSink * sink);
void           d3d_expose_window(GstD3DVideoSink * sink);
GstFlowReturn  d3d_render_buffer(GstD3DVideoSink * sink, GstBuffer * buf);
GstCaps *      d3d_supported_caps(GstD3DVideoSink * sink);
gboolean       d3d_set_render_format(GstD3DVideoSink * sink);
gboolean       d3d_get_hwnd_window_size (HWND hwnd, gint * width, gint * height);

#define GST_TYPE_D3DSURFACE_BUFFER_POOL      (gst_d3dsurface_buffer_pool_get_type())
#define GST_IS_D3DSURFACE_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_D3DSURFACE_BUFFER_POOL))
#define GST_D3DSURFACE_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_D3DSURFACE_BUFFER_POOL, GstD3DSurfaceBufferPool))
#define GST_D3DSURFACE_BUFFER_POOL_CAST(obj) ((GstD3DSurfaceBufferPool*)(obj))

typedef struct _GstD3DSurfaceBufferPool {
  GstVideoBufferPool parent;

  GstD3DVideoSink *sink;
  GstVideoInfo info;
  gboolean add_metavideo;

  GstAllocator *allocator;
} GstD3DSurfaceBufferPool;

typedef struct _GstD3DSurfaceBufferPoolClass {
  GstVideoBufferPoolClass parent_class;
} GstD3DSurfaceBufferPoolClass;

GType gst_d3dsurface_meta_api_get_type (void);
#define GST_D3DSURFACE_META_API_TYPE  (gst_d3dsurface_meta_api_get_type())
const GstMetaInfo * gst_d3dsurface_meta_get_info (void);
#define GST_D3DSURFACE_META_INFO  (gst_d3dsurface_meta_get_info())

#define gst_buffer_get_d3dsurface_meta(b) ((GstD3DSurfaceMeta*)gst_buffer_get_meta((b),GST_D3DSURFACE_META_API_TYPE))

GType gst_d3dsurface_buffer_pool_get_type (void);
GstBufferPool * gst_d3dsurface_buffer_pool_new (GstD3DVideoSink * sink);

#endif /* _D3DHELPERS_H_ */
