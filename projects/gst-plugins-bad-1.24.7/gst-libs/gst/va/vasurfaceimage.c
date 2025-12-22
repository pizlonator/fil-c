/* GStreamer
 * Copyright (C) 2021 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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

#include "vasurfaceimage.h"
#include "gstvavideoformat.h"
#include <va/va.h>

/* XXX: find a better log category */
#define GST_CAT_DEFAULT gst_va_display_debug
GST_DEBUG_CATEGORY_EXTERN (gst_va_display_debug);

gboolean
va_destroy_surfaces (GstVaDisplay * display, VASurfaceID * surfaces,
    gint num_surfaces)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  g_return_val_if_fail (num_surfaces > 0, FALSE);

  status = vaDestroySurfaces (dpy, surfaces, num_surfaces);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaDestroySurfaces: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

static gboolean
_rt_format_is_rgb (guint rt_format)
{
  switch (rt_format) {
    case VA_RT_FORMAT_RGB16:
    case VA_RT_FORMAT_RGB32:
    case VA_RT_FORMAT_RGB32_10:
      return TRUE;
    default:
      break;
  }

  return FALSE;
}

gboolean
va_create_surfaces (GstVaDisplay * display, guint rt_format, guint fourcc,
    guint width, guint height, gint usage_hint, guint64 * modifiers,
    guint num_modifiers, VADRMPRIMESurfaceDescriptor * desc,
    VASurfaceID * surfaces, guint num_surfaces)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  /* *INDENT-OFF* */
  VASurfaceAttrib attrs[6] = {
    {
      .type = VASurfaceAttribUsageHint,
      .flags = VA_SURFACE_ATTRIB_SETTABLE,
      .value.type = VAGenericValueTypeInteger,
      .value.value.i = usage_hint,
    },
    {
      .type = VASurfaceAttribMemoryType,
      .flags = VA_SURFACE_ATTRIB_SETTABLE,
      .value.type = VAGenericValueTypeInteger,
      .value.value.i = (desc && desc->num_objects > 0)
                               ? VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2
                               : VA_SURFACE_ATTRIB_MEM_TYPE_VA,
    },
  };
  VADRMFormatModifierList modifier_list = {
    .num_modifiers = num_modifiers,
    .modifiers = modifiers,
  };
  VASurfaceAttribExternalBuffers extbuf = {
    .width = width,
    .height = height,
    .num_planes = 1,
    .pixel_format = fourcc,
  };
  /* *INDENT-ON* */
  VAStatus status;
  guint num_attrs = 2;

  g_return_val_if_fail (num_surfaces > 0, FALSE);
  /* must have modifiers when num_modifiers > 0 */
  g_return_val_if_fail (num_modifiers == 0 || modifiers, FALSE);

  if (fourcc > 0) {
    /* *INDENT-OFF* */
    attrs[num_attrs++] = (VASurfaceAttrib) {
      .type = VASurfaceAttribPixelFormat,
      .flags = VA_SURFACE_ATTRIB_SETTABLE,
      .value.type = VAGenericValueTypeInteger,
      .value.value.i = fourcc,
    };
    /* *INDENT-ON* */
  }

  if (desc && desc->num_objects > 0) {
    /* *INDENT-OFF* */
    attrs[num_attrs++] = (VASurfaceAttrib) {
      .type = VASurfaceAttribExternalBufferDescriptor,
      .flags = VA_SURFACE_ATTRIB_SETTABLE,
      .value.type = VAGenericValueTypePointer,
      .value.value.p = desc,
    };
    /* *INDENT-ON* */
  } else if (GST_VA_DISPLAY_IS_IMPLEMENTATION (display, INTEL_I965)
      && _rt_format_is_rgb (rt_format)) {
    /* HACK(victor): disable tiling for i965 driver for RGB formats */
    /* *INDENT-OFF* */
     attrs[num_attrs++] = (VASurfaceAttrib) {
       .type = VASurfaceAttribExternalBufferDescriptor,
       .flags = VA_SURFACE_ATTRIB_SETTABLE,
       .value.type = VAGenericValueTypePointer,
       .value.value.p = &extbuf,
     };
    /* *INDENT-ON* */
  }

  if (num_modifiers > 0 && modifiers) {
    /* *INDENT-OFF* */
    attrs[num_attrs++] = (VASurfaceAttrib) {
      .type = VASurfaceAttribDRMFormatModifiers,
      .flags = VA_SURFACE_ATTRIB_SETTABLE,
      .value.type = VAGenericValueTypePointer,
      .value.value.p = &modifier_list,
    };
    /* *INDENT-ON* */
  }

retry:
  status = vaCreateSurfaces (dpy, rt_format, width, height, surfaces,
      num_surfaces, attrs, num_attrs);

  if (status == VA_STATUS_ERROR_ATTR_NOT_SUPPORTED
      && attrs[num_attrs - 1].type == VASurfaceAttribDRMFormatModifiers) {
    int i;

    /* if requested modifiers contain linear, let's remove the attribute and
     * "hope" the driver will create linear dmabufs */
    for (i = 0; i < num_modifiers; ++i) {
      if (modifiers[i] == DRM_FORMAT_MOD_LINEAR) {
        num_attrs--;
        goto retry;
      }
    }
  }

  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaCreateSurfaces: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

gboolean
va_export_surface_to_dmabuf (GstVaDisplay * display, VASurfaceID surface,
    guint32 flags, VADRMPRIMESurfaceDescriptor * desc)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  status = vaExportSurfaceHandle (dpy, surface,
      VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, flags, desc);
  if (status != VA_STATUS_SUCCESS) {
    GST_INFO ("vaExportSurfaceHandle: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

gboolean
va_destroy_image (GstVaDisplay * display, VAImageID image_id)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  status = vaDestroyImage (dpy, image_id);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaDestroyImage: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_get_derive_image (GstVaDisplay * display, VASurfaceID surface,
    VAImage * image)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;
  VASurfaceStatus state;

  /* When directly accessing a surface special care must be taken to insure sync
   * proper synchronization with the graphics hardware. Clients should call
   * vaQuerySurfaceStatus to insure that a surface is not the target of
   * concurrent rendering or currently being displayed by an overlay. */
  status = vaQuerySurfaceStatus (dpy, surface, &state);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaQuerySurfaceStatus: %s", vaErrorStr (status));
    return FALSE;
  }
  if (state != VASurfaceReady) {
    GST_INFO ("Surface not ready");
    return FALSE;
  }

  status = vaDeriveImage (dpy, surface, image);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaDeriveImage: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

gboolean
va_create_image (GstVaDisplay * display, GstVideoFormat format, gint width,
    gint height, VAImage * image)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  const VAImageFormat *va_format;
  VAStatus status;

  va_format = gst_va_image_format_from_video_format (format);
  if (!va_format)
    return FALSE;

  status =
      vaCreateImage (dpy, (VAImageFormat *) va_format, width, height, image);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaCreateImage: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_get_image (GstVaDisplay * display, VASurfaceID surface, VAImage * image)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  status = vaGetImage (dpy, surface, 0, 0, image->width, image->height,
      image->image_id);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaGetImage: %s", vaErrorStr (status));
    return FALSE;
  }

  return TRUE;
}

gboolean
va_sync_surface (GstVaDisplay * display, VASurfaceID surface)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  status = vaSyncSurface (dpy, surface);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaSyncSurface: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_map_buffer (GstVaDisplay * display, VABufferID buffer, GstMapFlags flags,
    gpointer * data)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

#if VA_CHECK_VERSION(1, 21, 0)
  uint32_t vaflags = 0;
  if (flags & GST_MAP_READ)
    vaflags |= VA_MAPBUFFER_FLAG_READ;
  if (flags & GST_MAP_WRITE)
    vaflags |= VA_MAPBUFFER_FLAG_WRITE;
  status = vaMapBuffer2 (dpy, buffer, data, vaflags);
#else
  status = vaMapBuffer (dpy, buffer, data);
#endif
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaMapBuffer: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_unmap_buffer (GstVaDisplay * display, VABufferID buffer)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  status = vaUnmapBuffer (dpy, buffer);
  if (status != VA_STATUS_SUCCESS) {
    GST_WARNING ("vaUnmapBuffer: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_put_image (GstVaDisplay * display, VASurfaceID surface, VAImage * image)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;

  if (!va_sync_surface (display, surface))
    return FALSE;

  status = vaPutImage (dpy, surface, image->image_id, 0, 0, image->width,
      image->height, 0, 0, image->width, image->height);
  if (status != VA_STATUS_SUCCESS) {
    GST_ERROR ("vaPutImage: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

gboolean
va_ensure_image (GstVaDisplay * display, VASurfaceID surface,
    GstVideoInfo * info, VAImage * image, gboolean derived)
{
  gboolean ret = TRUE;

  if (image->image_id != VA_INVALID_ID)
    return TRUE;

  if (!va_sync_surface (display, surface))
    return FALSE;

  if (derived) {
    ret = va_get_derive_image (display, surface, image);
  } else {
    ret = va_create_image (display, GST_VIDEO_INFO_FORMAT (info),
        GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info), image);
  }

  return ret;
}

gboolean
va_check_surface (GstVaDisplay * display, VASurfaceID surface)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  VAStatus status;
  VASurfaceStatus state;

  status = vaQuerySurfaceStatus (dpy, surface, &state);

  if (status != VA_STATUS_SUCCESS)
    GST_ERROR ("vaQuerySurfaceStatus: %s", vaErrorStr (status));

  GST_LOG ("surface %#x status %d", surface, state);

  return (status == VA_STATUS_SUCCESS);
}

gboolean
va_copy_surface (GstVaDisplay * display, VASurfaceID dst, VASurfaceID src)
{
  VADisplay dpy = gst_va_display_get_va_dpy (display);
  /* *INDENT-OFF* */
  VACopyObject obj_src = {
    .obj_type = VACopyObjectSurface,
    .object = {
      .surface_id = src,
    },
  };
  VACopyObject obj_dst = {
    .obj_type = VACopyObjectSurface,
    .object = {
      .surface_id = dst,
    },
  };
  VACopyOption option = {
    .bits = {
      .va_copy_sync = VA_EXEC_SYNC,
      .va_copy_mode = VA_EXEC_MODE_DEFAULT,
    },
  };
  /* *INDENT-ON* */
  VAStatus status;

  status = vaCopy (dpy, &obj_dst, &obj_src, option);
  if (status != VA_STATUS_SUCCESS) {
    GST_INFO ("vaCopy: %s", vaErrorStr (status));
    return FALSE;
  }
  return TRUE;
}

guint
va_get_surface_usage_hint (GstVaDisplay * display, VAEntrypoint entrypoint,
    GstPadDirection dir, gboolean is_dma)
{
  switch (entrypoint) {
    case VAEntrypointVideoProc:{
      /* For DMA kind caps, we use VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ |
         VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE to detect the modifiers.
         And in runtime, we should use the same flags in order to keep
         the same modifiers. */
      if (is_dma)
        return VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ |
            VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE;

      if (dir == GST_PAD_SINK)
        return VA_SURFACE_ATTRIB_USAGE_HINT_VPP_READ;
      else if (dir == GST_PAD_SRC)
        return VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE;

      break;
    }
    case VAEntrypointVLD:
      return VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
    case VAEntrypointEncSlice:
    case VAEntrypointEncSliceLP:
    case VAEntrypointEncPicture:
      return VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
    default:
      break;
  }

  return VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC;
}
