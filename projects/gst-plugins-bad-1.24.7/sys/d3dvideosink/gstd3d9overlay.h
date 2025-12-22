/* GStreamer
 * Copyright (C) 2019 Aaron Boxer <aaron.boxer@collabora.com>
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
#ifndef _GST_D3D9_OVERLAY_H_
#define _GST_D3D9_OVERLAY_H_

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

typedef struct _GstD3DVideoSinkOverlay
{
  GstVideoOverlayRectangle *rectangle;
  LPDIRECT3DTEXTURE9  texture;
  IDirect3DVertexBuffer9 *g_list_vb;
  int g_list_count;
} GstD3DVideoSinkOverlay;

GstFlowReturn gst_d3d9_overlay_prepare (GstD3DVideoSink *sink, GstBuffer * buf);
void gst_d3d9_overlay_free (GstD3DVideoSink * sink);
gboolean gst_d3d9_overlay_set_render_state (GstD3DVideoSink * sink);
gboolean gst_d3d9_overlay_render (GstD3DVideoSink * sink);
#endif /* _GST_D3D9_OVERLAY_H_ */
