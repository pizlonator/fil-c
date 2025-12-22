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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "d3dvideosink.h"
#include "gstd3d9overlay.h"

#include <stdio.h>

GST_DEBUG_CATEGORY_EXTERN (gst_d3dvideosink_debug);
#define GST_CAT_DEFAULT gst_d3dvideosink_debug

#define ERROR_CHECK_HR(hr)                          \
  if(hr != S_OK) {                                  \
    const gchar * str_err=NULL, *t1=NULL;           \
    gchar tmp[128]="";                              \
    switch(hr)
#define CASE_HR_ERR(hr_err)                         \
      case hr_err: str_err = #hr_err; break;
#define CASE_HR_DBG_ERR_END(sink, gst_err_msg, level) \
      default:                                      \
        t1=gst_err_msg;                             \
      sprintf(tmp, "HR-SEV:%u HR-FAC:%u HR-CODE:%u", (guint)HRESULT_SEVERITY(hr), (guint)HRESULT_FACILITY(hr), (guint)HRESULT_CODE(hr)); \
        str_err = tmp;                              \
    } /* end switch */                              \
    GST_CAT_LEVEL_LOG(GST_CAT_DEFAULT, level, sink, "%s HRESULT: %s", t1?t1:"", str_err);
#define CASE_HR_ERR_END(sink, gst_err_msg)          \
  CASE_HR_DBG_ERR_END(sink, gst_err_msg, GST_LEVEL_ERROR)
#define CASE_HR_DBG_END(sink, gst_err_msg)          \
  CASE_HR_DBG_ERR_END(sink, gst_err_msg, GST_LEVEL_DEBUG)
#define D3D9_CHECK(call) hr = call; \
        ERROR_CHECK_HR (hr) { \
          CASE_HR_ERR (D3DERR_INVALIDCALL); \
          CASE_HR_ERR_END (sink,  #call); \
          goto end; \
        }

#define CHECK_D3D_DEVICE(klass, sink, goto_label)                       \
  if(!klass->d3d.d3d || !klass->d3d.device.d3d_device) {                \
    GST_ERROR_OBJECT(sink, "Direct3D device or object does not exist"); \
    goto goto_label;                                                    \
  }

typedef struct _textured_vertex
{
  float x, y, z, rhw;           // The transformed(screen space) position for the vertex.
  float tu, tv;                 // Texture coordinates
} textured_vertex;

/* Transformed vertex with 1 set of texture coordinates */
static DWORD tri_fvf = D3DFVF_XYZRHW | D3DFVF_TEX1;

static gboolean
_is_rectangle_in_overlays (GList * overlays,
    GstVideoOverlayRectangle * rectangle);
static gboolean
_is_overlay_in_composition (GstVideoOverlayComposition * composition,
    GstD3DVideoSinkOverlay * overlay);
static HRESULT
gst_d3d9_overlay_init_vb (GstD3DVideoSink * sink,
    GstD3DVideoSinkOverlay * overlay);
static gboolean gst_d3d9_overlay_resize (GstD3DVideoSink * sink);
static void
gst_d3d9_overlay_calc_dest_rect (GstD3DVideoSink * sink, RECT * dest_rect);
static void gst_d3d9_overlay_free_overlay (GstD3DVideoSink * sink,
    GstD3DVideoSinkOverlay * overlay);

static void
gst_d3d9_overlay_calc_dest_rect (GstD3DVideoSink * sink, RECT * dest_rect)
{
  if (sink->force_aspect_ratio) {
    gint window_width;
    gint window_height;
    GstVideoRectangle src;
    GstVideoRectangle dst;
    GstVideoRectangle result;

    memset (&dst, 0, sizeof (dst));
    memset (&src, 0, sizeof (src));

    /* Set via GstXOverlay set_render_rect */
    if (sink->d3d.render_rect) {
      memcpy (&dst, sink->d3d.render_rect, sizeof (dst));
    } else {
      d3d_get_hwnd_window_size (sink->d3d.window_handle, &window_width,
          &window_height);
      dst.w = window_width;
      dst.h = window_height;
    }

    src.w = GST_VIDEO_SINK_WIDTH (sink);
    src.h = GST_VIDEO_SINK_HEIGHT (sink);

    gst_video_sink_center_rect (src, dst, &result, TRUE);

    dest_rect->left = result.x;
    dest_rect->top = result.y;
    dest_rect->right = result.x + result.w;
    dest_rect->bottom = result.y + result.h;
  } else if (sink->d3d.render_rect) {
    dest_rect->left = 0;
    dest_rect->top = 0;
    dest_rect->right = sink->d3d.render_rect->w;
    dest_rect->bottom = sink->d3d.render_rect->h;
  } else {
    /* get client window size */
    GetClientRect (sink->d3d.window_handle, dest_rect);
  }
}

static void
gst_d3d9_overlay_free_overlay (GstD3DVideoSink * sink,
    GstD3DVideoSinkOverlay * overlay)
{
  if (G_LIKELY (overlay)) {
    if (overlay->texture) {
      HRESULT hr = IDirect3DTexture9_Release (overlay->texture);
      if (hr != D3D_OK) {
        GST_ERROR_OBJECT (sink, "Failed to release D3D texture");
      }
    }
    if (overlay->g_list_vb) {
      HRESULT hr = IDirect3DVertexBuffer9_Release (overlay->g_list_vb);
      if (hr != D3D_OK) {
        GST_ERROR_OBJECT (sink, "Failed to release D3D vertex buffer");
      }
    }
    gst_video_overlay_rectangle_unref (overlay->rectangle);
    g_free (overlay);
  }
}

static gboolean
_is_rectangle_in_overlays (GList * overlays,
    GstVideoOverlayRectangle * rectangle)
{
  GList *l;

  for (l = overlays; l != NULL; l = l->next) {
    GstD3DVideoSinkOverlay *overlay = (GstD3DVideoSinkOverlay *) l->data;
    if (overlay->rectangle == rectangle)
      return TRUE;
  }
  return FALSE;
}

static gboolean
_is_overlay_in_composition (GstVideoOverlayComposition * composition,
    GstD3DVideoSinkOverlay * overlay)
{
  guint i;

  for (i = 0; i < gst_video_overlay_composition_n_rectangles (composition); i++) {
    GstVideoOverlayRectangle *rectangle =
        gst_video_overlay_composition_get_rectangle (composition, i);
    if (overlay->rectangle == rectangle)
      return TRUE;
  }
  return FALSE;
}

GstFlowReturn
gst_d3d9_overlay_prepare (GstD3DVideoSink * sink, GstBuffer * buf)
{
  GstD3DVideoSinkClass *klass = GST_D3DVIDEOSINK_GET_CLASS (sink);
  GList *l = NULL;
  GstVideoOverlayComposition *composition = NULL;
  guint num_overlays, i;
  GstVideoOverlayCompositionMeta *composition_meta =
      gst_buffer_get_video_overlay_composition_meta (buf);
  gboolean found_new_overlay_rectangle = FALSE;

  if (!composition_meta) {
    gst_d3d9_overlay_free (sink);
    return GST_FLOW_OK;
  }
  l = sink->d3d.overlay;
  composition = composition_meta->overlay;
  num_overlays = gst_video_overlay_composition_n_rectangles (composition);

  GST_DEBUG_OBJECT (sink, "GstVideoOverlayCompositionMeta found.");

  /* check for new overlays */
  for (i = 0; i < num_overlays; i++) {
    GstVideoOverlayRectangle *rectangle =
        gst_video_overlay_composition_get_rectangle (composition, i);

    if (!_is_rectangle_in_overlays (sink->d3d.overlay, rectangle)) {
      found_new_overlay_rectangle = TRUE;
      break;
    }
  }

  /* add new overlays to list */
  if (found_new_overlay_rectangle) {
    GST_DEBUG_OBJECT (sink, "New overlay composition rectangles found.");
    LOCK_CLASS (sink, klass);
    if (!klass->d3d.refs) {
      GST_ERROR_OBJECT (sink, "Direct3D object ref count = 0");
      gst_d3d9_overlay_free (sink);
      UNLOCK_CLASS (sink, klass);
      return GST_FLOW_ERROR;
    }
    for (i = 0; i < num_overlays; i++) {
      GstVideoOverlayRectangle *rectangle =
          gst_video_overlay_composition_get_rectangle (composition, i);

      if (!_is_rectangle_in_overlays (sink->d3d.overlay, rectangle)) {
        GstVideoOverlayFormatFlags flags;
        gint x, y;
        guint width, height;
        HRESULT hr = 0;
        GstMapInfo info;
        GstBuffer *from = NULL;
        GstD3DVideoSinkOverlay *overlay = g_new0 (GstD3DVideoSinkOverlay, 1);
        overlay->rectangle = gst_video_overlay_rectangle_ref (rectangle);
        if (!gst_video_overlay_rectangle_get_render_rectangle
            (overlay->rectangle, &x, &y, &width, &height)) {
          GST_ERROR_OBJECT (sink,
              "Failed to get overlay rectangle of dimension (%d,%d)", width,
              height);
          g_free (overlay);
          continue;
        }
        hr = IDirect3DDevice9_CreateTexture (klass->d3d.device.d3d_device,
            width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
            &overlay->texture, NULL);
        if (hr != D3D_OK) {
          GST_ERROR_OBJECT (sink,
              "Failed to create D3D texture of dimensions (%d,%d)", width,
              height);
          g_free (overlay);
          continue;
        }
        flags = gst_video_overlay_rectangle_get_flags (rectangle);
        /* FIXME: investigate support for pre-multiplied vs. non-pre-multiplied alpha */
        from = gst_video_overlay_rectangle_get_pixels_unscaled_argb
            (rectangle, flags);
        if (gst_buffer_map (from, &info, GST_MAP_READ)) {
          /* 1. lock texture */
          D3DLOCKED_RECT rect;
          hr = IDirect3DTexture9_LockRect (overlay->texture, 0, &rect, NULL,
              D3DUSAGE_WRITEONLY);
          if (hr != D3D_OK) {
            GST_ERROR_OBJECT (sink, "Failed to lock D3D texture");
            gst_buffer_unmap (from, &info);
            gst_d3d9_overlay_free_overlay (sink, overlay);
            continue;
          }
          /* 2. copy */
          memcpy (rect.pBits, info.data, info.size);
          /* 3. unlock texture */
          hr = IDirect3DTexture9_UnlockRect (overlay->texture, 0);
          if (hr != D3D_OK) {
            GST_ERROR_OBJECT (sink, "Failed to unlock D3D texture");
            gst_buffer_unmap (from, &info);
            gst_d3d9_overlay_free_overlay (sink, overlay);
            continue;
          }
          gst_buffer_unmap (from, &info);
          hr = gst_d3d9_overlay_init_vb (sink, overlay);
          if (FAILED (hr)) {
            gst_d3d9_overlay_free_overlay (sink, overlay);
            continue;
          }
        }
        sink->d3d.overlay = g_list_append (sink->d3d.overlay, overlay);
      }
    }
    UNLOCK_CLASS (sink, klass);
  }
  /* remove old overlays from list */
  while (l != NULL) {
    GList *next = l->next;
    GstD3DVideoSinkOverlay *overlay = (GstD3DVideoSinkOverlay *) l->data;

    if (!_is_overlay_in_composition (composition, overlay)) {
      gst_d3d9_overlay_free_overlay (sink, overlay);
      sink->d3d.overlay = g_list_delete_link (sink->d3d.overlay, l);
    }
    l = next;
  }

  return GST_FLOW_OK;
}

gboolean
gst_d3d9_overlay_resize (GstD3DVideoSink * sink)
{
  GList *l = sink->d3d.overlay;

  while (l != NULL) {
    GList *next = l->next;
    GstD3DVideoSinkOverlay *overlay = (GstD3DVideoSinkOverlay *) l->data;
    HRESULT hr = gst_d3d9_overlay_init_vb (sink, overlay);

    if (FAILED (hr)) {
      return FALSE;
    }
    l = next;
  }

  return TRUE;
}

void
gst_d3d9_overlay_free (GstD3DVideoSink * sink)
{
  GList *l = sink->d3d.overlay;

  while (l != NULL) {
    GList *next = l->next;
    GstD3DVideoSinkOverlay *overlay = (GstD3DVideoSinkOverlay *) l->data;

    gst_d3d9_overlay_free_overlay (sink, overlay);
    sink->d3d.overlay = g_list_delete_link (sink->d3d.overlay, l);
    l = next;
  }
  g_list_free (sink->d3d.overlay);
  sink->d3d.overlay = NULL;
}

static HRESULT
gst_d3d9_overlay_init_vb (GstD3DVideoSink * sink,
    GstD3DVideoSinkOverlay * overlay)
{
  GstD3DVideoSinkClass *klass = GST_D3DVIDEOSINK_GET_CLASS (sink);
  gint x = 0, y = 0;
  guint width = 0, height = 0;
  guint sink_width = GST_VIDEO_SINK_WIDTH (sink);
  guint sink_height = GST_VIDEO_SINK_HEIGHT (sink);
  float scaleX = 1.0f, scaleY = 1.0f;
  RECT dest_rect;
  guint dest_width, dest_height;
  void *vb_vertices = NULL;
  HRESULT hr = 0;
  int vert_count, byte_count;

  if (sink_width < 1 || sink_height < 1) {
    return D3D_OK;
  }

  if (!gst_video_overlay_rectangle_get_render_rectangle
      (overlay->rectangle, &x, &y, &width, &height)) {
    GST_ERROR_OBJECT (sink, "Failed to get overlay rectangle");
    return 0;
  }
  if (width < 1 || height < 1) {
    return D3D_OK;
  }
  memset (&dest_rect, 0, sizeof (dest_rect));
  gst_d3d9_overlay_calc_dest_rect (sink, &dest_rect);
  dest_width = dest_rect.right - dest_rect.left;
  dest_height = dest_rect.bottom - dest_rect.top;
  scaleX = (float) dest_width / sink_width;
  scaleY = (float) dest_height / sink_height;
  x = dest_rect.left + x * scaleX;
  y = dest_rect.top + y * scaleY;
  width *= scaleX;
  height *= scaleY;

  /* a quad is composed of six vertices */
  vert_count = 6;
  byte_count = vert_count * sizeof (textured_vertex);
  overlay->g_list_count = vert_count / 3;

  /* destroy existing buffer */
  if (overlay->g_list_vb) {
    hr = IDirect3DVertexBuffer9_Release (overlay->g_list_vb);
    if (hr != D3D_OK) {
      GST_ERROR_OBJECT (sink, "Failed to release D3D vertex buffer");
    }
  }
  CHECK_D3D_DEVICE (klass, sink, error);
  hr = IDirect3DDevice9_CreateVertexBuffer (klass->d3d.device.d3d_device, byte_count,   /* Length */
      D3DUSAGE_WRITEONLY,       /* Usage */
      tri_fvf,                  /* FVF */
      D3DPOOL_MANAGED,          /* Pool */
      &overlay->g_list_vb,      /* ppVertexBuffer */
      NULL);                    /* Handle */
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (sink, "Error Creating vertex buffer");
    return hr;
  }

  hr = IDirect3DVertexBuffer9_Lock (overlay->g_list_vb, 0,      /* Offset */
      0,                        /* SizeToLock */
      &vb_vertices,             /* Vertices */
      0);                       /* Flags */
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (sink, "Error Locking vertex buffer");
    return hr;
  }
  {
    textured_vertex data[] = {

      {x, y + height, 1, 1, 0, 1}
      , {x, y, 1, 1, 0, 0}
      , {x + width, y, 1, 1, 1, 0}
      ,
      {x, y + height, 1, 1, 0, 1}
      , {x + width, y, 1, 1, 1, 0}
      , {x + width,
          y + height, 1, 1, 1, 1}

    };
    memcpy (vb_vertices, data, byte_count);
  }
  hr = IDirect3DVertexBuffer9_Unlock (overlay->g_list_vb);
  if (FAILED (hr)) {
    GST_ERROR_OBJECT (sink, "Error Unlocking vertex buffer");
    return hr;
  }

  return D3D_OK;

error:
  return hr;
}

gboolean
gst_d3d9_overlay_set_render_state (GstD3DVideoSink * sink)
{
  HRESULT hr = 0;
  GstD3DVideoSinkClass *klass = GST_D3DVIDEOSINK_GET_CLASS (sink);
  gboolean ret = FALSE;

  D3D9_CHECK (IDirect3DDevice9_SetRenderState (klass->d3d.device.d3d_device,
          D3DRS_ALPHABLENDENABLE, TRUE));
  D3D9_CHECK (IDirect3DDevice9_SetRenderState (klass->d3d.device.d3d_device,
          D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
  D3D9_CHECK (IDirect3DDevice9_SetRenderState (klass->d3d.device.d3d_device,
          D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));

  ret = TRUE;
end:
  return ret;
}

gboolean
gst_d3d9_overlay_render (GstD3DVideoSink * sink)
{
  HRESULT hr = 0;
  GList *iter = NULL;
  gboolean ret = FALSE;
  GstD3DVideoSinkClass *klass = GST_D3DVIDEOSINK_GET_CLASS (sink);

  if (!sink->d3d.overlay)
    return TRUE;

  if (sink->d3d.overlay_needs_resize && !gst_d3d9_overlay_resize (sink))
    return FALSE;
  sink->d3d.overlay_needs_resize = FALSE;
  iter = sink->d3d.overlay;
  while (iter != NULL) {
    GList *next = iter->next;
    GstD3DVideoSinkOverlay *overlay = (GstD3DVideoSinkOverlay *) iter->data;

    if (!overlay->g_list_vb) {
      GST_ERROR_OBJECT (sink, "Overlay is missing vertex buffer");
      goto end;
    }
    if (!overlay->texture) {
      GST_ERROR_OBJECT (sink, "Overlay is missing texture");
      goto end;
    }
    D3D9_CHECK (IDirect3DDevice9_SetTexture (klass->d3d.device.d3d_device, 0,
            (IDirect3DBaseTexture9 *) overlay->texture))
        /* Bind our Vertex Buffer */
        D3D9_CHECK (IDirect3DDevice9_SetFVF (klass->d3d.device.d3d_device,
            tri_fvf))
        D3D9_CHECK (IDirect3DDevice9_SetStreamSource (klass->d3d.device.d3d_device, 0,  /* StreamNumber */
            overlay->g_list_vb, /* StreamData */
            0,                  /* OffsetInBytes */
            sizeof (textured_vertex)))
        /* Stride */
        //Render from our Vertex Buffer
        D3D9_CHECK (IDirect3DDevice9_DrawPrimitive (klass->d3d.device.d3d_device, D3DPT_TRIANGLELIST,   /* PrimitiveType */
            0,                  /* StartVertex */
            overlay->g_list_count))     /* PrimitiveCount */
        iter = next;
  }
  ret = TRUE;
end:
  return ret;
}
