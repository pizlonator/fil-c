/*
 * GStreamer
 * Copyright (C) 2012-2014 Matthew Waters <ystree00@gmail.com>
 * Copyright (C) 2017 Sebastian Dröge <sebastian@centricular.com>
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

#include <stdio.h>

#include "gl.h"
#include "gstglupload.h"
#include "gstglfuncs.h"

#if GST_GL_HAVE_PLATFORM_EGL
#include "egl/gsteglimage.h"
#include "egl/gsteglimage_private.h"
#include "egl/gstglmemoryegl.h"
#include "egl/gstglcontext_egl.h"
#endif

#if GST_GL_HAVE_DMABUF
#include <gst/allocators/gstdmabuf.h>
#include <libdrm/drm_fourcc.h>
#else
/* to avoid ifdef in _gst_gl_upload_set_caps_unlocked() */
#define DRM_FORMAT_MOD_LINEAR  0ULL
#endif

#if GST_GL_HAVE_VIV_DIRECTVIV
#include <gst/allocators/gstphysmemory.h>
#include <gst/gl/gstglfuncs.h>
#endif

/**
 * SECTION:gstglupload
 * @title: GstGLUpload
 * @short_description: an object that uploads to GL textures
 * @see_also: #GstGLDownload, #GstGLMemory
 *
 * #GstGLUpload is an object that uploads data from system memory into GL textures.
 *
 * A #GstGLUpload can be created with gst_gl_upload_new()
 */

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

GST_DEBUG_CATEGORY_STATIC (gst_gl_upload_debug);
#define GST_CAT_DEFAULT gst_gl_upload_debug

static void gst_gl_upload_finalize (GObject * object);

static GstGLTextureTarget
_caps_get_texture_target (GstCaps * caps, GstGLTextureTarget default_target)
{
  GstGLTextureTarget ret = 0;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_field_typed (s, "texture-target", G_TYPE_STRING)) {
    const gchar *target_str = gst_structure_get_string (s, "texture-target");
    ret = gst_gl_texture_target_from_string (target_str);
  }

  if (!ret)
    ret = default_target;

  return ret;
}

/* Define the maximum number of planes we can upload - handle 2 views per buffer */
#define GST_GL_UPLOAD_MAX_PLANES (GST_VIDEO_MAX_PLANES * 2)

typedef struct _UploadMethod UploadMethod;

struct _GstGLUploadPrivate
{
  union
  {
    GstVideoInfo in_info;
    GstVideoInfoDmaDrm in_info_drm;
  };
  GstVideoInfo out_info;
  GstCaps *in_caps;
  GstCaps *out_caps;

  GstBuffer *outbuf;

  /* all method impl pointers */
  gpointer *upload_impl;

  /* current method */
  const UploadMethod *method;
  gpointer method_impl;
  int method_i;

  /* saved method for reconfigure */
  int saved_method_i;
};

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_upload_debug, "glupload", 0, "upload");

G_DEFINE_TYPE_WITH_CODE (GstGLUpload, gst_gl_upload, GST_TYPE_OBJECT,
    G_ADD_PRIVATE (GstGLUpload) DEBUG_INIT);

static gboolean
filter_features (GstCapsFeatures * features,
    G_GNUC_UNUSED GstStructure * structure, gpointer user_data)
{
  const GstCapsFeatures *user_features = user_data;
  GQuark feature;
  guint i, num;

  if (gst_caps_features_is_any (features))
    return TRUE;

  num = gst_caps_features_get_size (user_features);
  for (i = 0; i < num; i++) {
    feature = gst_caps_features_get_nth_id (user_features, i);
    if (gst_caps_features_contains_id (features, feature))
      return TRUE;
  }

  return FALSE;
}

static gboolean
_filter_caps_with_features (const GstCaps * caps,
    const GstCapsFeatures * features, GstCaps ** ret_caps)
{
  GstCaps *tmp = NULL;
  gboolean ret = TRUE;

  if (gst_caps_is_empty (caps))
    return FALSE;

  if (gst_caps_is_any (caps)) {
    if (ret_caps) {
      tmp = gst_caps_new_empty ();
      gst_caps_set_features_simple (tmp, gst_caps_features_copy (features));
      *ret_caps = tmp;
    }

    return TRUE;
  }

  tmp = gst_caps_copy (caps);
  gst_caps_filter_and_map_in_place (tmp, filter_features, (gpointer) features);

  if (gst_caps_is_empty (tmp)) {
    gst_clear_caps (&tmp);
    ret = FALSE;
  }

  if (ret_caps)
    *ret_caps = tmp;
  else
    gst_clear_caps (&tmp);

  return ret;
}

static GstCaps *
_set_caps_features_with_passthrough (const GstCaps * caps,
    const gchar * feature_name, GstCapsFeatures * passthrough)
{
  guint i, j, m, n;
  GstCaps *tmp;

  tmp = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstCapsFeatures *features, *orig_features;
    GstStructure *s = gst_caps_get_structure (caps, i);

    orig_features = gst_caps_get_features (caps, i);
    features = gst_caps_features_new (feature_name, NULL);

    if (gst_caps_features_is_any (orig_features)) {
      /* if we have any features, we add both the features with and without @passthrough */
      gst_caps_append_structure_full (tmp, gst_structure_copy (s),
          gst_caps_features_copy (features));

      m = gst_caps_features_get_size (passthrough);
      for (j = 0; j < m; j++) {
        const gchar *feature = gst_caps_features_get_nth (passthrough, j);

        /* if we already have the features */
        if (gst_caps_features_contains (features, feature))
          continue;

        gst_caps_features_add (features, feature);
      }
    } else {
      m = gst_caps_features_get_size (orig_features);
      for (j = 0; j < m; j++) {
        const gchar *feature = gst_caps_features_get_nth (orig_features, j);

        /* if we already have the features */
        if (gst_caps_features_contains (features, feature))
          continue;

        if (g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY) == 0)
          continue;

        if (gst_caps_features_contains (passthrough, feature)) {
          gst_caps_features_add (features, feature);
        }
      }
    }

    gst_caps_append_structure_full (tmp, gst_structure_copy (s), features);
  }

  return tmp;
}

static GstCaps *
_caps_intersect_texture_target (GstCaps * caps, GstGLTextureTarget target_mask)
{
  GValue targets = G_VALUE_INIT;
  GstCaps *ret, *target;

  target = gst_caps_copy (caps);
  gst_gl_value_set_texture_target_from_mask (&targets, target_mask);
  gst_caps_set_value (target, "texture-target", &targets);

  ret = gst_caps_intersect_full (caps, target, GST_CAPS_INTERSECT_FIRST);

  g_value_unset (&targets);
  gst_caps_unref (target);
  return ret;
}

static gboolean
_structure_check_target (GstStructure * structure,
    GstGLTextureTarget target_mask)
{
  const GValue *target_val;
  const gchar *target_str;
  GstGLTextureTarget target;
  guint i;

  target_val = gst_structure_get_value (structure, "texture-target");

  /* If no texture-target set, it means a default of 2D. */
  if (!target_val)
    return (1 << GST_GL_TEXTURE_TARGET_2D) & target_mask;

  if (G_VALUE_HOLDS_STRING (target_val)) {
    target_str = g_value_get_string (target_val);
    target = gst_gl_texture_target_from_string (target_str);

    return (1 << target) & target_mask;
  } else if (GST_VALUE_HOLDS_LIST (target_val)) {
    guint num_values = gst_value_list_get_size (target_val);

    for (i = 0; i < num_values; i++) {
      const GValue *val = gst_value_list_get_value (target_val, i);

      target_str = g_value_get_string (val);
      target = gst_gl_texture_target_from_string (target_str);
      if ((1 << target) & target_mask)
        return TRUE;
    }
  }

  return FALSE;
}

typedef enum
{
  METHOD_FLAG_CAN_SHARE_CONTEXT = 1,
  METHOD_FLAG_CAN_ACCEPT_RAW = 2,       /* This method can accept raw memory input caps */
} GstGLUploadMethodFlags;

struct _UploadMethod
{
  const gchar *name;
  GstGLUploadMethodFlags flags;

  GstStaticCaps *input_template_caps;

    gpointer (*new) (GstGLUpload * upload);
  GstCaps *(*transform_caps) (gpointer impl, GstGLContext * context,
      GstPadDirection direction, GstCaps * caps);
    gboolean (*accept) (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
      GstCaps * out_caps);
  void (*propose_allocation) (gpointer impl, GstQuery * decide_query,
      GstQuery * query);
    GstGLUploadReturn (*perform) (gpointer impl, GstBuffer * buffer,
      GstBuffer ** outbuf);
  void (*free) (gpointer impl);
} _UploadMethod;

struct PassthroughUpload
{
  GstGLUpload *upload;
};

static gpointer
_passthrough_upload_new (GstGLUpload * upload)
{
  struct PassthroughUpload *passthrough = g_new0 (struct PassthroughUpload, 1);

  passthrough->upload = upload;

  return passthrough;
}

static GstStaticCaps _passthrough_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_DMA_DRM_CAPS_MAKE);

static GstCaps *
_passthrough_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *passthrough_caps = gst_static_caps_get (&_passthrough_upload_caps);
  GstCaps *out_caps;

  out_caps = gst_caps_intersect_full (caps, passthrough_caps,
      GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (passthrough_caps);

  return out_caps;
}

static gboolean
_passthrough_upload_accept (gpointer impl, GstBuffer * buffer,
    GstCaps * in_caps, GstCaps * out_caps)
{
  GstCaps *caps;
  gboolean res;

  caps = gst_caps_intersect (in_caps, out_caps);
  res = !gst_caps_is_empty (caps);
  gst_caps_unref (caps);

  return res;
}

static void
_passthrough_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
}

static GstGLUploadReturn
_passthrough_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  *outbuf = gst_buffer_ref (buffer);

  return GST_GL_UPLOAD_DONE;
}

static void
_passthrough_upload_free (gpointer impl)
{
  g_free (impl);
}

static const UploadMethod _passthrough_upload = {
  "Dmabuf Passthrough",
  0,
  &_passthrough_upload_caps,
  &_passthrough_upload_new,
  &_passthrough_upload_transform_caps,
  &_passthrough_upload_accept,
  &_passthrough_upload_propose_allocation,
  &_passthrough_upload_perform,
  &_passthrough_upload_free
};

struct GLMemoryUpload
{
  GstGLUpload *upload;
  GstGLTextureTarget input_target;
  GstGLTextureTarget output_target;
};

static gpointer
_gl_memory_upload_new (GstGLUpload * upload)
{
  struct GLMemoryUpload *mem = g_new0 (struct GLMemoryUpload, 1);

  mem->upload = upload;
  mem->input_target = GST_GL_TEXTURE_TARGET_NONE;
  mem->output_target = GST_GL_TEXTURE_TARGET_NONE;

  return mem;
}

static GstCaps *
_gl_memory_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  struct GLMemoryUpload *upload = impl;
  GstCapsFeatures *passthrough =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    GstCaps *tmp;
    GstCapsFeatures *filter_features;
    GstGLTextureTarget target_mask;

    filter_features = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, NULL);
    if (!_filter_caps_with_features (caps, filter_features, &tmp)) {
      gst_caps_features_free (filter_features);
      gst_caps_features_free (passthrough);
      return NULL;
    }
    gst_caps_features_free (filter_features);

    ret = _set_caps_features_with_passthrough (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);
    gst_caps_unref (tmp);

    if (upload->input_target != GST_GL_TEXTURE_TARGET_NONE) {
      target_mask = 1 << upload->input_target;
    } else {
      target_mask = 1 << GST_GL_TEXTURE_TARGET_2D |
          1 << GST_GL_TEXTURE_TARGET_RECTANGLE |
          1 << GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
    }

    tmp = _caps_intersect_texture_target (ret, target_mask);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;

    ret = _set_caps_features_with_passthrough (caps,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  gst_caps_features_free (passthrough);

  GST_DEBUG_OBJECT (upload->upload, "direction %s, transformed %"
      GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps, ret);

  return ret;
}

static gboolean
_gl_memory_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct GLMemoryUpload *upload = impl;
  GstCapsFeatures *features;
  int i;

  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;

  features = gst_caps_get_features (in_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY)
      && !gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY))
    return FALSE;

  if (buffer) {
    GstVideoInfo *in_info = &upload->upload->priv->in_info;
    guint expected_memories = GST_VIDEO_INFO_N_PLANES (in_info);

    /* Support stereo views for separated multiview mode */
    if (GST_VIDEO_INFO_MULTIVIEW_MODE (in_info) ==
        GST_VIDEO_MULTIVIEW_MODE_SEPARATED)
      expected_memories *= GST_VIDEO_INFO_VIEWS (in_info);

    if (gst_buffer_n_memory (buffer) != expected_memories)
      return FALSE;

    for (i = 0; i < expected_memories; i++) {
      GstMemory *mem = gst_buffer_peek_memory (buffer, i);

      if (!gst_is_gl_memory (mem))
        return FALSE;
    }
  }

  return TRUE;
}

static void
_gl_memory_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  struct GLMemoryUpload *upload = impl;
  GstBufferPool *pool = NULL;
  guint n_pools, i;
  GstCaps *caps;
  GstCapsFeatures *features_gl, *features_sys;
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  gboolean use_sys_mem = FALSE;
  const gchar *target_pool_option_str = NULL;

  gst_query_parse_allocation (query, &caps, NULL);
  if (caps == NULL)
    goto invalid_caps;

  g_assert (gst_caps_is_fixed (caps));

  features_gl = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, NULL);
  features_sys =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, NULL);
  /* Only offer our custom allocator if that type of memory was negotiated. */
  if (_filter_caps_with_features (caps, features_sys, NULL)) {
    use_sys_mem = TRUE;
  } else if (!_filter_caps_with_features (caps, features_gl, NULL)) {
    gst_caps_features_free (features_gl);
    gst_caps_features_free (features_sys);
    return;
  }
  gst_caps_features_free (features_gl);
  gst_caps_features_free (features_sys);

  if (upload->upload->priv->out_caps) {
    GstGLTextureTarget target;

    target = _caps_get_texture_target (upload->upload->priv->out_caps,
        GST_GL_TEXTURE_TARGET_2D);

    /* Do not provide the allocator and pool for system memory caps
       because the external oes kind GL memory can not be mapped. */
    if (target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES && use_sys_mem)
      return;

    target_pool_option_str =
        gst_gl_texture_target_to_buffer_pool_option (target);
  }

  gst_allocation_params_init (&params);


  if (!use_sys_mem) {
    allocator =
        GST_ALLOCATOR (gst_gl_memory_allocator_get_default (upload->
            upload->context));
  }

  gst_query_add_allocation_param (query, allocator, &params);
  gst_clear_object (&allocator);

#if GST_GL_HAVE_PLATFORM_EGL
  if (upload->upload->context
      && gst_gl_context_get_gl_platform (upload->upload->context) ==
      GST_GL_PLATFORM_EGL) {
    if (!use_sys_mem) {
      allocator =
          GST_ALLOCATOR (gst_allocator_find (GST_GL_MEMORY_EGL_ALLOCATOR_NAME));
    }
    gst_query_add_allocation_param (query, allocator, &params);
    gst_clear_object (&allocator);
  }
#endif

  n_pools = gst_query_get_n_allocation_pools (query);
  for (i = 0; i < n_pools; i++) {
    gst_query_parse_nth_allocation_pool (query, i, &pool, NULL, NULL, NULL);
    if (!GST_IS_GL_BUFFER_POOL (pool)) {
      gst_object_unref (pool);
      pool = NULL;
    }
  }

  if (!pool) {
    GstStructure *config;
    GstVideoInfo info;
    gsize size;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    pool = gst_gl_buffer_pool_new (upload->upload->context);
    config = gst_buffer_pool_get_config (pool);

    /* the normal size of a frame */
    size = info.size;
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    /* keep one buffer around before allowing acquire */
    gst_buffer_pool_config_set_gl_min_free_queue_size (config, 1);
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);
    if (target_pool_option_str)
      gst_buffer_pool_config_add_option (config, target_pool_option_str);

    if (!gst_buffer_pool_set_config (pool, config)) {
      gst_object_unref (pool);
      goto config_failed;
    }

    gst_query_add_allocation_pool (query, pool, size, 1, 0);
  }

  if (pool)
    gst_object_unref (pool);

  return;

invalid_caps:
  {
    GST_WARNING_OBJECT (upload->upload, "invalid caps specified");
    return;
  }
config_failed:
  {
    GST_WARNING_OBJECT (upload->upload, "failed setting config");
    return;
  }
}

static GstGLUploadReturn
_gl_memory_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct GLMemoryUpload *upload = impl;
  GstGLMemory *gl_mem;
  int i, n;

  n = gst_buffer_n_memory (buffer);
  for (i = 0; i < n; i++) {
    GstMemory *mem = gst_buffer_peek_memory (buffer, i);

    gl_mem = (GstGLMemory *) mem;
    if (!gst_gl_context_can_share (upload->upload->context,
            gl_mem->mem.context))
      return GST_GL_UPLOAD_UNSHARED_GL_CONTEXT;

    if (upload->output_target == GST_GL_TEXTURE_TARGET_NONE &&
        upload->upload->priv->out_caps) {
      upload->output_target =
          _caps_get_texture_target (upload->upload->priv->out_caps,
          GST_GL_TEXTURE_TARGET_NONE);
    }

    /* always track the last input texture target so ::transform_caps() can
     * use it to build the output caps */
    upload->input_target = gl_mem->tex_target;
    if (upload->output_target != gl_mem->tex_target) {
      *outbuf = NULL;
      return GST_GL_UPLOAD_RECONFIGURE;
    }

    if (gst_is_gl_memory_pbo (mem))
      gst_gl_memory_pbo_upload_transfer ((GstGLMemoryPBO *) mem);
  }

  *outbuf = gst_buffer_ref (buffer);

  return GST_GL_UPLOAD_DONE;
}

static void
_gl_memory_upload_free (gpointer impl)
{
  g_free (impl);
}


static GstStaticCaps _gl_memory_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, GST_GL_MEMORY_VIDEO_FORMATS_STR));

static const UploadMethod _gl_memory_upload = {
  "GLMemory",
  METHOD_FLAG_CAN_SHARE_CONTEXT,
  &_gl_memory_upload_caps,
  &_gl_memory_upload_new,
  &_gl_memory_upload_transform_caps,
  &_gl_memory_upload_accept,
  &_gl_memory_upload_propose_allocation,
  &_gl_memory_upload_perform,
  &_gl_memory_upload_free
};

#if GST_GL_HAVE_DMABUF

typedef enum
{
  INCLUDE_EXTERNAL = 1 << 1,
  LINEAR_ONLY = 2 << 1,
} GstGLUploadDrmFormatFlags;

typedef struct _GstEGLImageCacheEntry
{
  GstEGLImage *eglimage[GST_VIDEO_MAX_PLANES];
} GstEGLImageCacheEntry;

typedef struct _GstEGLImageCache
{
  gint ref_count;
  GHashTable *hash_table;       /* for GstMemory -> GstEGLImageCacheEntry lookup */
  GMutex lock;                  /* protects hash_table */
} GstEGLImageCache;

struct DmabufUpload
{
  GstGLUpload *upload;

  GstEGLImage *eglimage[GST_VIDEO_MAX_PLANES];
  GstEGLImageCache *eglimage_cache;
  GstGLFormat formats[GST_VIDEO_MAX_PLANES];
  GstBuffer *outbuf;
  GstGLVideoAllocationParams *params;
  guint n_mem;

  gboolean direct;
  GstGLTextureTarget target;
  GstVideoInfo out_info;
  /* only used for pointer comparison */
  gpointer out_caps;
};

static void
gst_egl_image_cache_ref (GstEGLImageCache * cache)
{
  g_atomic_int_inc (&cache->ref_count);
}

static void
gst_egl_image_cache_unref (GstEGLImageCache * cache)
{
  if (g_atomic_int_dec_and_test (&cache->ref_count)) {
    g_hash_table_unref (cache->hash_table);
    g_mutex_clear (&cache->lock);
    g_free (cache);
  }
}

static void
gst_egl_image_cache_entry_remove (GstEGLImageCache * cache, GstMiniObject * mem)
{
  g_mutex_lock (&cache->lock);
  g_hash_table_remove (cache->hash_table, mem);
  g_mutex_unlock (&cache->lock);
  gst_egl_image_cache_unref (cache);
}

static GstEGLImageCacheEntry *
gst_egl_image_cache_entry_new (GstEGLImageCache * cache, GstMemory * mem)
{
  GstEGLImageCacheEntry *cache_entry;

  cache_entry = g_new0 (GstEGLImageCacheEntry, 1);
  gst_egl_image_cache_ref (cache);
  gst_mini_object_weak_ref (GST_MINI_OBJECT (mem),
      (GstMiniObjectNotify) gst_egl_image_cache_entry_remove, cache);
  g_mutex_lock (&cache->lock);
  g_hash_table_insert (cache->hash_table, mem, cache_entry);
  g_mutex_unlock (&cache->lock);

  return cache_entry;
}

static void
gst_egl_image_cache_entry_free (GstEGLImageCacheEntry * cache_entry)
{
  gint i;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++) {
    if (cache_entry->eglimage[i])
      gst_egl_image_unref (cache_entry->eglimage[i]);
  }
  g_free (cache_entry);
}

/*
 * Looks up a cache_entry for mem if mem is different from previous_mem.
 * If mem is the same as previous_mem, the costly lookup is skipped and the
 * provided (previous) cache_entry is used instead.
 *
 * Returns the cached eglimage for the given plane from the cache_entry, or
 * NULL. previous_mem is set to mem.
 */
static GstEGLImage *
gst_egl_image_cache_lookup (GstEGLImageCache * cache, GstMemory * mem,
    gint plane, GstMemory ** previous_mem, GstEGLImageCacheEntry ** cache_entry)
{
  if (mem != *previous_mem) {
    g_mutex_lock (&cache->lock);
    *cache_entry = g_hash_table_lookup (cache->hash_table, mem);
    g_mutex_unlock (&cache->lock);
    *previous_mem = mem;
  }

  if (*cache_entry)
    return (*cache_entry)->eglimage[plane];

  return NULL;
}

/*
 * Creates a new cache_entry for mem if no cache_entry is provided.
 * Stores the eglimage for the given plane in the cache_entry.
 */
static void
gst_egl_image_cache_store (GstEGLImageCache * cache, GstMemory * mem,
    gint plane, GstEGLImage * eglimage, GstEGLImageCacheEntry ** cache_entry)
{
  if (!(*cache_entry))
    *cache_entry = gst_egl_image_cache_entry_new (cache, mem);
  (*cache_entry)->eglimage[plane] = eglimage;
}

static GstEGLImageCache *
gst_egl_image_cache_new (void)
{
  GstEGLImageCache *cache;

  cache = g_new0 (GstEGLImageCache, 1);
  cache->ref_count = 1;

  cache->hash_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) gst_egl_image_cache_entry_free);
  g_mutex_init (&cache->lock);

  return cache;
}

static GstStaticCaps _dma_buf_upload_caps =
    GST_STATIC_CAPS (GST_VIDEO_DMA_DRM_CAPS_MAKE ";"
    GST_VIDEO_CAPS_MAKE (GST_GL_MEMORY_VIDEO_FORMATS_STR));

static gpointer
_dma_buf_upload_new (GstGLUpload * upload)
{
  struct DmabufUpload *dmabuf = g_new0 (struct DmabufUpload, 1);
  dmabuf->upload = upload;
  dmabuf->eglimage_cache = gst_egl_image_cache_new ();
  dmabuf->target = GST_GL_TEXTURE_TARGET_2D;
  return dmabuf;
}

/* Append all drm format strings to drm_formats array. */
static void
_append_drm_formats_from_video_format (GstGLContext * context,
    GstVideoFormat format, GstGLUploadDrmFormatFlags flags,
    GPtrArray * drm_formats)
{
  gint32 i, fourcc;
  const GArray *dma_modifiers = NULL;
  char *drm_format;

  fourcc = gst_video_dma_drm_fourcc_from_format (format);
  if (fourcc == DRM_FORMAT_INVALID)
    return;

  if (!gst_gl_context_egl_get_format_modifiers (context, fourcc,
          &dma_modifiers))
    return;

  /* No modifier info, lets warn and move on */
  if (!dma_modifiers) {
    GST_WARNING_OBJECT (context, "Undefined modifiers list for %"
        GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
    return;
  }

  for (i = 0; i < dma_modifiers->len; i++) {
    GstGLDmaModifier *mod = &g_array_index (dma_modifiers, GstGLDmaModifier, i);

    if (!(flags & INCLUDE_EXTERNAL) && mod->external_only)
      continue;

    if (flags & LINEAR_ONLY && mod->modifier != DRM_FORMAT_MOD_LINEAR)
      continue;

    drm_format = gst_video_dma_drm_fourcc_to_string (fourcc, mod->modifier);
    g_ptr_array_add (drm_formats, drm_format);
  }
}

/* Given the video formats in src GValue, collecting all the according
   drm formats to dst GValue. Return FALSE if no valid drm formats found. */
static gboolean
_dma_buf_transform_gst_formats_to_drm_formats (GstGLContext * context,
    const GValue * video_value, GstGLUploadDrmFormatFlags flags,
    GValue * drm_value)
{
  GstVideoFormat gst_format;
  GPtrArray *all_drm_formats = NULL;
  guint i;

  all_drm_formats = g_ptr_array_new ();

  if (G_VALUE_HOLDS_STRING (video_value)) {
    gst_format =
        gst_video_format_from_string (g_value_get_string (video_value));
    if (gst_format != GST_VIDEO_FORMAT_UNKNOWN) {
      _append_drm_formats_from_video_format (context, gst_format,
          flags, all_drm_formats);
    }
  } else if (GST_VALUE_HOLDS_LIST (video_value)) {
    guint num_values = gst_value_list_get_size (video_value);

    for (i = 0; i < num_values; i++) {
      const GValue *val = gst_value_list_get_value (video_value, i);

      gst_format = gst_video_format_from_string (g_value_get_string (val));
      if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      _append_drm_formats_from_video_format (context, gst_format,
          flags, all_drm_formats);
    }
  }

  if (all_drm_formats->len == 0) {
    g_ptr_array_unref (all_drm_formats);
    return FALSE;
  }

  if (all_drm_formats->len == 1) {
    g_value_init (drm_value, G_TYPE_STRING);
    g_value_take_string (drm_value, g_ptr_array_index (all_drm_formats, 0));
  } else {
    GValue item = G_VALUE_INIT;

    gst_value_list_init (drm_value, all_drm_formats->len);

    for (i = 0; i < all_drm_formats->len; i++) {
      g_value_init (&item, G_TYPE_STRING);
      g_value_take_string (&item, g_ptr_array_index (all_drm_formats, i));
      gst_value_list_append_value (drm_value, &item);
      g_value_unset (&item);
    }
  }

  /* The strings are already token by the GValue, no need to free. */
  g_ptr_array_unref (all_drm_formats);

  return TRUE;
}

static gboolean
_check_modifier (GstGLContext * context, guint32 fourcc,
    guint64 modifier, gboolean include_external)
{
  const GArray *dma_modifiers;
  guint i;

  /* If no context provide, no further check. */
  if (!context)
    return TRUE;

  if (!gst_gl_context_egl_get_format_modifiers (context, fourcc,
          &dma_modifiers))
    return FALSE;

  if (!dma_modifiers) {
    /* recognize the fourcc but no modifier info, consider it as linear */
    if (modifier == DRM_FORMAT_MOD_LINEAR)
      return TRUE;

    return FALSE;
  }

  for (i = 0; i < dma_modifiers->len; i++) {
    GstGLDmaModifier *mod = &g_array_index (dma_modifiers, GstGLDmaModifier, i);

    if (!mod->external_only || include_external) {
      if (mod->modifier == modifier)
        return TRUE;
    }
  }

  return FALSE;
}

static void
_set_default_formats_list (GstStructure * structure)
{
  GValue formats = G_VALUE_INIT;

  g_value_init (&formats, GST_TYPE_LIST);
  gst_value_deserialize (&formats, GST_GL_MEMORY_VIDEO_FORMATS_STR);
  gst_structure_take_value (structure, "format", &formats);
}

static GstVideoFormat
_get_video_format_from_drm_format (GstGLContext * context,
    const gchar * drm_format, GstGLUploadDrmFormatFlags flags)
{
  GstVideoFormat gst_format;
  guint32 fourcc;
  guint64 modifier;

  fourcc = gst_video_dma_drm_fourcc_from_string (drm_format, &modifier);
  if (fourcc == DRM_FORMAT_INVALID)
    return GST_VIDEO_FORMAT_UNKNOWN;

  if (flags & LINEAR_ONLY && modifier != DRM_FORMAT_MOD_LINEAR)
    return GST_VIDEO_FORMAT_UNKNOWN;

  gst_format = gst_video_dma_drm_fourcc_to_format (fourcc);
  if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
    return GST_VIDEO_FORMAT_UNKNOWN;

  if (!_check_modifier (context, fourcc, modifier, flags & INCLUDE_EXTERNAL))
    return GST_VIDEO_FORMAT_UNKNOWN;

  return gst_format;
}

/* Given the drm formats in src GValue, collecting all the according
   gst formats to dst GValue. Return FALSE if no valid drm formats found. */
static gboolean
_dma_buf_transform_drm_formats_to_gst_formats (GstGLContext * context,
    const GValue * drm_value, GstGLUploadDrmFormatFlags flags,
    GValue * video_value)
{
  GstVideoFormat gst_format;
  GArray *all_formats = NULL;
  guint i;

  all_formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));

  if (G_VALUE_HOLDS_STRING (drm_value)) {
    gst_format = _get_video_format_from_drm_format (context,
        g_value_get_string (drm_value), flags);

    if (gst_format != GST_VIDEO_FORMAT_UNKNOWN)
      g_array_append_val (all_formats, gst_format);
  } else if (GST_VALUE_HOLDS_LIST (drm_value)) {
    guint num_values = gst_value_list_get_size (drm_value);

    for (i = 0; i < num_values; i++) {
      const GValue *val = gst_value_list_get_value (drm_value, i);

      gst_format = _get_video_format_from_drm_format (context,
          g_value_get_string (val), flags);
      if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      g_array_append_val (all_formats, gst_format);
    }
  }

  if (all_formats->len == 0) {
    g_array_unref (all_formats);
    return FALSE;
  }

  if (all_formats->len == 1) {
    g_value_init (video_value, G_TYPE_STRING);
    gst_format = g_array_index (all_formats, GstVideoFormat, 0);
    g_value_set_string (video_value, gst_video_format_to_string (gst_format));
  } else {
    GValue item = G_VALUE_INIT;

    gst_value_list_init (video_value, all_formats->len);

    for (i = 0; i < all_formats->len; i++) {
      g_value_init (&item, G_TYPE_STRING);
      gst_format = g_array_index (all_formats, GstVideoFormat, i);
      g_value_set_string (&item, gst_video_format_to_string (gst_format));
      gst_value_list_append_value (video_value, &item);
      g_value_unset (&item);
    }
  }

  g_array_unref (all_formats);

  return TRUE;
}

static gboolean
_dma_buf_convert_format_field_in_structure (GstGLContext * context,
    GstStructure * structure, GstPadDirection direction,
    GstGLUploadDrmFormatFlags flags)
{
  const GValue *val;

  if (direction == GST_PAD_SRC) {
    GValue drm_formats = G_VALUE_INIT;

    /* No context available, we can not know the real modifiers.
       Just leaving all format related fields blank. */
    if (!context) {
      gst_structure_set (structure, "format", G_TYPE_STRING, "DMA_DRM", NULL);
      gst_structure_remove_field (structure, "drm-format");

      return TRUE;
    }

    /* When no format provided, just list all supported formats
       and find all the possible drm-format. */
    if (!(val = gst_structure_get_value (structure, "format"))) {
      _set_default_formats_list (structure);
      val = gst_structure_get_value (structure, "format");
    }

    if (_dma_buf_transform_gst_formats_to_drm_formats (context,
            val, flags, &drm_formats)) {
      gst_structure_take_value (structure, "drm-format", &drm_formats);
    } else {
      return FALSE;
    }

    gst_structure_set (structure, "format", G_TYPE_STRING, "DMA_DRM", NULL);
  } else {
    GValue gst_formats = G_VALUE_INIT;

    /* Reject the traditional "format" field directly. */
    if (g_strcmp0 (gst_structure_get_string (structure, "format"),
            "DMA_DRM") != 0)
      return FALSE;

    /* If no drm-field in the src, we just list all
       supported formats in dst. */
    if (!(val = gst_structure_get_value (structure, "drm-format"))) {
      gst_structure_remove_field (structure, "format");
      gst_structure_remove_field (structure, "drm-format");
      _set_default_formats_list (structure);
      return TRUE;
    }

    if (_dma_buf_transform_drm_formats_to_gst_formats (context,
            val, flags, &gst_formats)) {
      gst_structure_take_value (structure, "format", &gst_formats);
    } else {
      return FALSE;
    }

    gst_structure_remove_field (structure, "drm-format");
  }

  return TRUE;
}

static gboolean
_dma_buf_check_formats_in_structure (GstGLContext * context,
    GstStructure * structure, gboolean include_external)
{
  const GValue *all_formats;
  GstVideoFormat gst_format;
  guint32 fourcc;

  all_formats = gst_structure_get_value (structure, "format");
  if (!all_formats)
    return FALSE;

  if (G_VALUE_HOLDS_STRING (all_formats)) {
    gst_format =
        gst_video_format_from_string (g_value_get_string (all_formats));
    if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
      return FALSE;

    fourcc = gst_video_dma_drm_fourcc_from_format (gst_format);
    if (fourcc == DRM_FORMAT_INVALID)
      return FALSE;

    if (!_check_modifier (context, fourcc,
            DRM_FORMAT_MOD_LINEAR, include_external))
      return FALSE;

    return TRUE;
  } else if (GST_VALUE_HOLDS_LIST (all_formats)) {
    GValue video_value = G_VALUE_INIT;
    guint num_values = gst_value_list_get_size (all_formats);
    GArray *gst_formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));
    guint i;

    for (i = 0; i < num_values; i++) {
      const GValue *val = gst_value_list_get_value (all_formats, i);

      gst_format = gst_video_format_from_string (g_value_get_string (val));
      if (gst_format == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      fourcc = gst_video_dma_drm_fourcc_from_format (gst_format);
      if (fourcc == DRM_FORMAT_INVALID)
        continue;

      if (!_check_modifier (context, fourcc,
              DRM_FORMAT_MOD_LINEAR, include_external))
        continue;

      g_array_append_val (gst_formats, gst_format);
    }

    if (gst_formats->len == 0) {
      g_array_unref (gst_formats);
      return FALSE;
    }

    if (gst_formats->len == 1) {
      g_value_init (&video_value, G_TYPE_STRING);
      gst_format = g_array_index (gst_formats, GstVideoFormat, 0);
      g_value_set_string (&video_value,
          gst_video_format_to_string (gst_format));
    } else {
      GValue item = G_VALUE_INIT;

      gst_value_list_init (&video_value, gst_formats->len);

      for (i = 0; i < gst_formats->len; i++) {
        g_value_init (&item, G_TYPE_STRING);

        gst_format = g_array_index (gst_formats, GstVideoFormat, i);
        g_value_set_string (&item, gst_video_format_to_string (gst_format));
        gst_value_list_append_value (&video_value, &item);
        g_value_unset (&item);
      }
    }

    g_array_unref (gst_formats);

    gst_structure_take_value (structure, "format", &video_value);

    return TRUE;
  }

  return FALSE;
}

static GstCaps *
_dma_buf_upload_transform_caps_common (GstCaps * caps,
    GstGLContext * context, GstPadDirection direction,
    GstGLUploadDrmFormatFlags flags,
    GstGLTextureTarget target_mask,
    const gchar * from_feature, const gchar * to_feature)
{
  guint i, n;
  GstCaps *ret_caps, *tmp_caps, *caps_to_transform;
  GstCapsFeatures *passthrough, *features;

  if (direction == GST_PAD_SINK) {
    g_return_val_if_fail
        (!g_strcmp0 (from_feature, GST_CAPS_FEATURE_MEMORY_DMABUF) ||
        !g_strcmp0 (from_feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY), NULL);
    g_return_val_if_fail
        (!g_strcmp0 (to_feature, GST_CAPS_FEATURE_MEMORY_GL_MEMORY), NULL);
  } else {
    g_return_val_if_fail
        (!g_strcmp0 (to_feature, GST_CAPS_FEATURE_MEMORY_DMABUF) ||
        !g_strcmp0 (to_feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY), NULL);
    g_return_val_if_fail
        (!g_strcmp0 (from_feature, GST_CAPS_FEATURE_MEMORY_GL_MEMORY), NULL);
  }

  features = gst_caps_features_new (from_feature, NULL);
  if (!_filter_caps_with_features (caps, features, &caps_to_transform)) {
    gst_caps_features_free (features);
    return NULL;
  }
  gst_caps_features_free (features);

  if (gst_caps_is_any (caps_to_transform)) {
    tmp_caps = caps_to_transform;
    goto passthrough;
  }

  tmp_caps = gst_caps_new_empty ();
  n = gst_caps_get_size (caps_to_transform);

  for (i = 0; i < n; i++) {
    GstStructure *s;
    GstCapsFeatures *features;

    features = gst_caps_get_features (caps_to_transform, i);
    g_assert (gst_caps_features_contains (features, from_feature));

    s = gst_caps_get_structure (caps_to_transform, i);

    if (direction == GST_PAD_SRC && !_structure_check_target (s, target_mask))
      continue;

    s = gst_structure_copy (s);

    if (!g_strcmp0 (from_feature, GST_CAPS_FEATURE_MEMORY_DMABUF) ||
        !g_strcmp0 (to_feature, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
      /* Convert drm-format/format fields for DMABuf */
      if (!_dma_buf_convert_format_field_in_structure (context, s,
              direction, flags)) {
        gst_structure_free (s);
        continue;
      }
    } else {
      if (!_dma_buf_check_formats_in_structure (context, s,
              flags & INCLUDE_EXTERNAL)) {
        gst_structure_free (s);
        continue;
      }
    }

    gst_caps_append_structure_full (tmp_caps, s,
        gst_caps_features_copy (features));
  }

  gst_caps_unref (caps_to_transform);

  if (gst_caps_is_empty (tmp_caps)) {
    gst_caps_unref (tmp_caps);
    return NULL;
  }

passthrough:
  /* Change the feature name. */
  passthrough = gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  ret_caps = _set_caps_features_with_passthrough (tmp_caps,
      to_feature, passthrough);

  gst_caps_features_free (passthrough);
  gst_caps_unref (tmp_caps);

  return ret_caps;
}

static GstCaps *
_dma_buf_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  struct DmabufUpload *dmabuf = impl;
  GstCaps *ret, *tmp;

  if (context) {
    const GstGLFuncs *gl = context->gl_vtable;

    if (!gl->EGLImageTargetTexture2D)
      return NULL;

    /* Don't propose DMABuf caps feature unless it can be supported */
    if (gst_gl_context_get_gl_platform (context) != GST_GL_PLATFORM_EGL)
      return NULL;

    if (!gst_gl_context_check_feature (context, "EGL_KHR_image_base"))
      return NULL;

    if (!gst_gl_context_egl_supports_modifier (context))
      return NULL;
  }

  g_assert (dmabuf->target == GST_GL_TEXTURE_TARGET_2D);

  if (direction == GST_PAD_SINK) {
    GstGLUploadDrmFormatFlags flags = INCLUDE_EXTERNAL | LINEAR_ONLY;

    ret = _dma_buf_upload_transform_caps_common (caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_DMABUF,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    tmp = _dma_buf_upload_transform_caps_common (caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    if (!ret) {
      ret = tmp;
      tmp = NULL;
    }
    if (tmp)
      ret = gst_caps_merge (ret, tmp);

    if (!ret) {
      GST_DEBUG_OBJECT (dmabuf->upload,
          "direction %s, fails to transformed DMA caps %" GST_PTR_FORMAT,
          "sink", caps);
      return NULL;
    }

    tmp = _caps_intersect_texture_target (ret, 1 << GST_GL_TEXTURE_TARGET_2D);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;

    ret = _dma_buf_upload_transform_caps_common (caps, context, direction,
        INCLUDE_EXTERNAL | LINEAR_ONLY, 1 << dmabuf->target,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, GST_CAPS_FEATURE_MEMORY_DMABUF);
    tmp = _dma_buf_upload_transform_caps_common (caps, context, direction,
        INCLUDE_EXTERNAL | LINEAR_ONLY, 1 << dmabuf->target,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    if (!ret) {
      ret = tmp;
      tmp = NULL;
    }
    if (tmp)
      ret = gst_caps_merge (ret, tmp);

    if (!ret) {
      GST_DEBUG_OBJECT (dmabuf->upload,
          "direction %s, fails to transformed DMA caps %" GST_PTR_FORMAT,
          "src", caps);
      return NULL;
    }

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  GST_DEBUG_OBJECT (dmabuf->upload, "direction %s, \n\ttransformed %"
      GST_PTR_FORMAT "\n\tinto %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps, ret);

  return ret;
}

static gboolean
_dma_buf_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct DmabufUpload *dmabuf = impl;
  GstVideoInfoDmaDrm *in_info_drm = &dmabuf->upload->priv->in_info_drm;
  GstVideoInfo *in_info = &in_info_drm->vinfo;
  GstVideoInfo *out_info = &dmabuf->out_info;
  guint n_planes;
  GstVideoMeta *meta;
  guint n_mem;
  GstMemory *mems[GST_VIDEO_MAX_PLANES];
  GstMemory *previous_mem = NULL;
  GstEGLImageCacheEntry *cache_entry = NULL;
  gsize offset[GST_VIDEO_MAX_PLANES];
  gint fd[GST_VIDEO_MAX_PLANES];
  guint i;

  n_mem = gst_buffer_n_memory (buffer);
  meta = gst_buffer_get_video_meta (buffer);

  if (!dmabuf->upload->context->gl_vtable->EGLImageTargetTexture2D)
    return FALSE;

  /* dmabuf upload is only supported with EGL contexts. */
  if (gst_gl_context_get_gl_platform (dmabuf->upload->context) !=
      GST_GL_PLATFORM_EGL)
    return FALSE;

  if (!gst_gl_context_check_feature (dmabuf->upload->context,
          "EGL_KHR_image_base")) {
    GST_DEBUG_OBJECT (dmabuf->upload, "no EGL_KHR_image_base extension");
    return FALSE;
  }

  if (!gst_gl_context_egl_supports_modifier (dmabuf->upload->context)) {
    GST_DEBUG_OBJECT (dmabuf->upload, "no modifier support");
    return FALSE;
  }

  if (dmabuf->target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES &&
      !gst_gl_context_check_feature (dmabuf->upload->context,
          "GL_OES_EGL_image_external")) {
    GST_DEBUG_OBJECT (dmabuf->upload, "no GL_OES_EGL_image_external extension");
    return FALSE;
  }

  if (!gst_caps_features_contains (gst_caps_get_features (in_caps, 0),
          GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY) &&
      !gst_caps_features_contains (gst_caps_get_features (in_caps, 0),
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    GST_DEBUG_OBJECT (dmabuf->upload,
        "Not a DMABuf or SystemMemory caps %" GST_PTR_FORMAT, in_caps);
    return FALSE;
  }

  if (dmabuf->direct && !gst_egl_image_check_dmabuf_direct_with_dma_drm
      (dmabuf->upload->context, in_info_drm, dmabuf->target)) {
    GST_DEBUG_OBJECT (dmabuf->upload,
        "Direct mode does not support %" GST_FOURCC_FORMAT ":0x%016"
        G_GINT64_MODIFIER "x with target: %s",
        GST_FOURCC_ARGS (in_info_drm->drm_fourcc), in_info_drm->drm_modifier,
        gst_gl_texture_target_to_string (dmabuf->target));
    return FALSE;
  }

  if (!dmabuf->direct && in_info_drm->drm_modifier != DRM_FORMAT_MOD_LINEAR) {
    GST_DEBUG_OBJECT (dmabuf->upload,
        "Indirect uploads are only support for linear formats.");
    return FALSE;
  }

  /* This will eliminate most non-dmabuf out there */
  if (!gst_is_dmabuf_memory (gst_buffer_peek_memory (buffer, 0))) {
    GST_DEBUG_OBJECT (dmabuf->upload, "input not dmabuf");
    return FALSE;
  }

  n_planes = GST_VIDEO_INFO_N_PLANES (in_info);

  /* Update video info based on video meta */
  if (meta) {
    in_info->width = meta->width;
    in_info->height = meta->height;
    n_planes = meta->n_planes;

    for (i = 0; i < meta->n_planes; i++) {
      in_info->offset[i] = meta->offset[i];
      in_info->stride[i] = meta->stride[i];
    }
  }

  /* We cannot have multiple dmabuf per plane */
  if (n_mem > n_planes) {
    GST_DEBUG_OBJECT (dmabuf->upload,
        "number of memory (%u) != number of planes (%u)", n_mem, n_planes);
    return FALSE;
  }

  if (out_caps != dmabuf->out_caps) {
    dmabuf->out_caps = out_caps;
    if (!gst_video_info_from_caps (out_info, out_caps))
      return FALSE;

    /*
     * When we zero-copy tiles, we need to propagate the strides, which contains
     * the tile dimension. This is because the shader needs to know the padded
     * size in order to correctly sample into these special buffer.
     */
    if (meta && GST_VIDEO_FORMAT_INFO_IS_TILED (out_info->finfo)) {
      out_info->width = meta->width;
      out_info->height = meta->height;

      for (i = 0; i < meta->n_planes; i++) {
        out_info->offset[i] = meta->offset[i];
        out_info->stride[i] = meta->stride[i];
      }
    }
  }

  if (dmabuf->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) dmabuf->params);
  if (!(dmabuf->params = gst_gl_video_allocation_params_new_wrapped_gl_handle
          (dmabuf->upload->context, NULL, out_info, -1, NULL, dmabuf->target,
              0, NULL, NULL, NULL)))
    return FALSE;

  /* Find and validate all memories */
  for (i = 0; i < n_planes; i++) {
    guint plane_size;
    guint length;
    guint mem_idx;
    gsize mem_skip;

    if (GST_VIDEO_INFO_FORMAT (in_info) == GST_VIDEO_FORMAT_DMA_DRM)
      plane_size = 1;
    else
      plane_size = gst_gl_get_plane_data_size (in_info, NULL, i);

    if (!gst_buffer_find_memory (buffer, in_info->offset[i], plane_size,
            &mem_idx, &length, &mem_skip)) {
      GST_DEBUG_OBJECT (dmabuf->upload, "could not find memory %u", i);
      return FALSE;
    }

    /* We can't have more then one dmabuf per plane */
    if (length != 1) {
      GST_DEBUG_OBJECT (dmabuf->upload, "data for plane %u spans %u memories",
          i, length);
      return FALSE;
    }

    mems[i] = gst_buffer_peek_memory (buffer, mem_idx);

    /* And all memory found must be dmabuf */
    if (!gst_is_dmabuf_memory (mems[i])) {
      GST_DEBUG_OBJECT (dmabuf->upload, "memory %u is not dmabuf", i);
      return FALSE;
    }

    offset[i] = mems[i]->offset + mem_skip;
    fd[i] = gst_dmabuf_memory_get_fd (mems[i]);
  }

  if (dmabuf->direct) {
    dmabuf->n_mem = 1;
  } else {
    dmabuf->n_mem = n_planes;
  }

  /* Now create an EGLImage for each dmabuf */
  for (i = 0; i < dmabuf->n_mem; i++) {
    /*
     * Check if an EGLImage is cached. Remember the previous memory and cache
     * entry to avoid repeated lookups if all mems[i] point to the same memory.
     */
    dmabuf->eglimage[i] = gst_egl_image_cache_lookup (dmabuf->eglimage_cache,
        mems[i], i, &previous_mem, &cache_entry);
    if (dmabuf->eglimage[i]) {
      dmabuf->formats[i] = dmabuf->eglimage[i]->format;
      continue;
    }

    /* otherwise create one and cache it */
    if (dmabuf->direct) {
      dmabuf->eglimage[i] = gst_egl_image_from_dmabuf_direct_target_with_dma_drm
          (dmabuf->upload->context, n_planes, fd, offset, in_info_drm,
          dmabuf->target);
    } else {
      dmabuf->eglimage[i] = gst_egl_image_from_dmabuf
          (dmabuf->upload->context, fd[i], in_info, i, offset[i]);
    }

    if (!dmabuf->eglimage[i]) {
      GST_DEBUG_OBJECT (dmabuf->upload, "could not create eglimage");
      return FALSE;
    }

    gst_egl_image_cache_store (dmabuf->eglimage_cache, mems[i], i,
        dmabuf->eglimage[i], &cache_entry);
    dmabuf->formats[i] = dmabuf->eglimage[i]->format;
  }

  return TRUE;
}

static void
_dma_buf_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  /* nothing to do for now. */
}

static void
_dma_buf_upload_perform_gl_thread (GstGLContext * context,
    struct DmabufUpload *dmabuf)
{
  GstGLMemoryAllocator *allocator;

  allocator =
      GST_GL_MEMORY_ALLOCATOR (gst_allocator_find
      (GST_GL_MEMORY_EGL_ALLOCATOR_NAME));

  /* FIXME: buffer pool */
  dmabuf->outbuf = gst_buffer_new ();
  gst_gl_memory_setup_buffer (allocator, dmabuf->outbuf, dmabuf->params,
      dmabuf->formats, (gpointer *) dmabuf->eglimage, dmabuf->n_mem);
  gst_object_unref (allocator);
}

static GstGLUploadReturn
_dma_buf_upload_perform (gpointer impl, GstBuffer * buffer, GstBuffer ** outbuf)
{
  struct DmabufUpload *dmabuf = impl;

  /* The direct path sets sinkpad caps to RGBA but this may be incorrect for
   * the non-direct path, if that path fails to accept. In that case, we need
   * to reconfigure.
   */
  if (!dmabuf->direct &&
      GST_VIDEO_INFO_FORMAT (&dmabuf->upload->priv->in_info) !=
      GST_VIDEO_INFO_FORMAT (&dmabuf->out_info))
    return GST_GL_UPLOAD_RECONFIGURE;

  gst_gl_context_thread_add (dmabuf->upload->context,
      (GstGLContextThreadFunc) _dma_buf_upload_perform_gl_thread, dmabuf);

  if (!dmabuf->outbuf)
    return GST_GL_UPLOAD_ERROR;

  gst_buffer_add_parent_buffer_meta (dmabuf->outbuf, buffer);

  *outbuf = dmabuf->outbuf;
  dmabuf->outbuf = NULL;

  return GST_GL_UPLOAD_DONE;
}

static void
_dma_buf_upload_free (gpointer impl)
{
  struct DmabufUpload *dmabuf = impl;

  if (dmabuf->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) dmabuf->params);
  gst_egl_image_cache_unref (dmabuf->eglimage_cache);

  g_free (impl);
}

static const UploadMethod _dma_buf_upload = {
  "Dmabuf",
  0,
  &_dma_buf_upload_caps,
  &_dma_buf_upload_new,
  &_dma_buf_upload_transform_caps,
  &_dma_buf_upload_accept,
  &_dma_buf_upload_propose_allocation,
  &_dma_buf_upload_perform,
  &_dma_buf_upload_free
};

/* a variant of the DMABuf uploader that relies on HW color conversion instead
 * of shaders */

static gpointer
_direct_dma_buf_upload_new (GstGLUpload * upload)
{
  struct DmabufUpload *dmabuf = _dma_buf_upload_new (upload);
  dmabuf->direct = TRUE;
  gst_video_info_init (&dmabuf->out_info);
  return dmabuf;
}

static GstCaps *
_direct_dma_buf_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  struct DmabufUpload *dmabuf = impl;
  GstCaps *ret, *tmp;
  GstGLUploadDrmFormatFlags flags = 0;

  if (dmabuf->target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES)
    flags |= INCLUDE_EXTERNAL;

  if (context) {
    const GstGLFuncs *gl = context->gl_vtable;

    if (!gl->EGLImageTargetTexture2D)
      return NULL;

    /* Don't propose direct DMABuf caps feature unless it can be supported */
    if (gst_gl_context_get_gl_platform (context) != GST_GL_PLATFORM_EGL)
      return NULL;

    if (dmabuf->target == GST_GL_TEXTURE_TARGET_EXTERNAL_OES &&
        !gst_gl_context_check_feature (context, "GL_OES_EGL_image_external"))
      return NULL;

    if (!gst_gl_context_egl_supports_modifier (context))
      return NULL;
  }

  if (direction == GST_PAD_SINK) {
    gint i, n;
    GstGLTextureTarget target_mask;

    ret = _dma_buf_upload_transform_caps_common (caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_DMABUF,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    tmp = _dma_buf_upload_transform_caps_common (caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    if (!ret) {
      ret = tmp;
      tmp = NULL;
    }
    if (tmp)
      ret = gst_caps_merge (ret, tmp);

    if (!ret) {
      GST_DEBUG_OBJECT (dmabuf->upload,
          "direction %s, fails to transformed DMA caps %" GST_PTR_FORMAT,
          "sink", caps);
      return NULL;
    }

    /* The direct mode, sampling an imported texture will return an RGBA
       vector in the same colorspace as the source image. If the source
       image is stored in YUV(or some other basis) then the YUV values will
       be transformed to RGB values. So, any input format is transformed to:
       "video/x-raw(memory:GLMemory), format=(string)RGBA" as output. */
    gst_caps_set_simple (ret, "format", G_TYPE_STRING, "RGBA", NULL);

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "chroma-site", NULL);
      gst_structure_remove_fields (s, "colorimetry", NULL);
    }

    target_mask = 1 << dmabuf->target;
    tmp = _caps_intersect_texture_target (ret, target_mask);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;
    GstCaps *tmp_caps;

    /* The src caps may only contain RGBA format, and we should list
       all possible supported formats to detect the conversion for
       DMABuf kind memory. */
    tmp_caps = gst_caps_copy (caps);
    for (i = 0; i < gst_caps_get_size (tmp_caps); i++)
      _set_default_formats_list (gst_caps_get_structure (tmp_caps, i));

    ret = _dma_buf_upload_transform_caps_common (tmp_caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        GST_CAPS_FEATURE_MEMORY_DMABUF);
    gst_caps_unref (tmp_caps);

    tmp = _dma_buf_upload_transform_caps_common (caps, context, direction,
        flags, 1 << dmabuf->target, GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
        GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);

    if (!ret) {
      ret = tmp;
      tmp = NULL;
    }
    if (tmp)
      ret = gst_caps_merge (ret, tmp);

    if (!ret) {
      GST_DEBUG_OBJECT (dmabuf->upload,
          "direction %s, fails to transformed DMA caps %" GST_PTR_FORMAT,
          "src", caps);
      return NULL;
    }

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  GST_DEBUG_OBJECT (dmabuf->upload, "direction %s, transformed %"
      GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps, ret);

  return ret;
}

static const UploadMethod _direct_dma_buf_upload = {
  "DirectDmabuf",
  0,
  &_dma_buf_upload_caps,
  &_direct_dma_buf_upload_new,
  &_direct_dma_buf_upload_transform_caps,
  &_dma_buf_upload_accept,
  &_dma_buf_upload_propose_allocation,
  &_dma_buf_upload_perform,
  &_dma_buf_upload_free
};

/* a variant of the direct DMABuf uploader that uses external OES textures */

static gpointer
_direct_dma_buf_external_upload_new (GstGLUpload * upload)
{
  struct DmabufUpload *dmabuf = _direct_dma_buf_upload_new (upload);
  dmabuf->target = GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
  return dmabuf;
}

static const UploadMethod _direct_dma_buf_external_upload = {
  "DirectDmabufExternal",
  0,
  &_dma_buf_upload_caps,
  &_direct_dma_buf_external_upload_new,
  &_direct_dma_buf_upload_transform_caps,
  &_dma_buf_upload_accept,
  &_dma_buf_upload_propose_allocation,
  &_dma_buf_upload_perform,
  &_dma_buf_upload_free
};

#endif /* GST_GL_HAVE_DMABUF */

struct GLUploadMeta
{
  GstGLUpload *upload;

  gboolean result;
  GstVideoGLTextureUploadMeta *meta;
  guint texture_ids[GST_GL_UPLOAD_MAX_PLANES];
  GstBufferPool *pool;
};

static gpointer
_upload_meta_upload_new (GstGLUpload * upload)
{
  struct GLUploadMeta *meta = g_new0 (struct GLUploadMeta, 1);

  meta->upload = upload;
  meta->pool = NULL;

  return meta;
}

static GstCaps *
_upload_meta_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCapsFeatures *passthrough =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  GstCapsFeatures *filter_features;
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    GstCaps *tmp;

    filter_features = gst_caps_features_from_string
        (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META);
    if (!_filter_caps_with_features (caps, filter_features, &tmp)) {
      gst_caps_features_free (filter_features);
      gst_caps_features_free (passthrough);
      return NULL;
    }
    gst_caps_features_free (filter_features);

    ret = _set_caps_features_with_passthrough (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);
    gst_caps_unref (tmp);

    tmp = _caps_intersect_texture_target (ret, 1 << GST_GL_TEXTURE_TARGET_2D);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;

    ret =
        _set_caps_features_with_passthrough (caps,
        GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META, passthrough);
    gst_caps_set_simple (ret, "format", G_TYPE_STRING, "RGBA", NULL);

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  gst_caps_features_free (passthrough);

  return ret;
}

static gboolean
_upload_meta_upload_accept (gpointer impl, GstBuffer * buffer,
    GstCaps * in_caps, GstCaps * out_caps)
{
  struct GLUploadMeta *upload = impl;
  GstCapsFeatures *features;
  GstVideoGLTextureUploadMeta *meta;
  gboolean ret = TRUE;
  GstStructure *config;
  gsize size;

  features = gst_caps_get_features (in_caps, 0);

  if (!gst_caps_features_contains (features,
          GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META))
    ret = FALSE;

  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    ret = FALSE;

  if (!ret)
    return ret;

  if (upload->pool == NULL)
    upload->pool = gst_gl_buffer_pool_new (upload->upload->context);

  if (!gst_buffer_pool_is_active (upload->pool)) {
    config = gst_buffer_pool_get_config (upload->pool);

    size = upload->upload->priv->in_info.size;
    gst_buffer_pool_config_set_params (config, in_caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (upload->pool, config)) {
      GST_WARNING_OBJECT (upload->upload, "failed to set bufferpool config");
      return FALSE;
    }
    gst_buffer_pool_set_active (upload->pool, TRUE);
  }

  if (buffer) {
    if ((meta = gst_buffer_get_video_gl_texture_upload_meta (buffer)) == NULL)
      return FALSE;

    if (meta->texture_type[0] != GST_VIDEO_GL_TEXTURE_TYPE_RGBA) {
      GST_FIXME_OBJECT (upload, "only single rgba texture supported");
      return FALSE;
    }

    if (meta->texture_orientation !=
        GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_NORMAL) {
      GST_FIXME_OBJECT (upload, "only x-normal, y-normal textures supported");
      return FALSE;
    }
  }

  return TRUE;
}

static void
_upload_meta_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  struct GLUploadMeta *upload = impl;
  GstStructure *gl_context;
  gchar *platform, *gl_apis;
  gpointer handle;

  gl_apis =
      gst_gl_api_to_string (gst_gl_context_get_gl_api (upload->upload->
          context));
  platform =
      gst_gl_platform_to_string (gst_gl_context_get_gl_platform (upload->
          upload->context));
  handle = (gpointer) gst_gl_context_get_gl_context (upload->upload->context);

  gl_context =
      gst_structure_new ("GstVideoGLTextureUploadMeta", "gst.gl.GstGLContext",
      GST_TYPE_GL_CONTEXT, upload->upload->context, "gst.gl.context.handle",
      G_TYPE_POINTER, handle, "gst.gl.context.type", G_TYPE_STRING, platform,
      "gst.gl.context.apis", G_TYPE_STRING, gl_apis, NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, gl_context);

  g_free (gl_apis);
  g_free (platform);
  gst_structure_free (gl_context);
}

/*
 * Uploads using gst_video_gl_texture_upload_meta_upload().
 * i.e. consumer of GstVideoGLTextureUploadMeta
 */
static void
_do_upload_with_meta (GstGLContext * context, struct GLUploadMeta *upload)
{
  if (!gst_video_gl_texture_upload_meta_upload (upload->meta,
          upload->texture_ids)) {
    upload->result = FALSE;
    return;
  }

  upload->result = TRUE;
}

static GstGLUploadReturn
_upload_meta_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct GLUploadMeta *upload = impl;
  int i;
  GstVideoInfo *in_info = &upload->upload->priv->in_info;
  guint max_planes = GST_VIDEO_INFO_N_PLANES (in_info);

  /* Support stereo views for separated multiview mode */
  if (GST_VIDEO_INFO_MULTIVIEW_MODE (in_info) ==
      GST_VIDEO_MULTIVIEW_MODE_SEPARATED)
    max_planes *= GST_VIDEO_INFO_VIEWS (in_info);

  GST_LOG_OBJECT (upload, "Attempting upload with GstVideoGLTextureUploadMeta");

  upload->meta = gst_buffer_get_video_gl_texture_upload_meta (buffer);

  if (gst_buffer_pool_acquire_buffer (upload->pool, outbuf,
          NULL) != GST_FLOW_OK) {
    GST_WARNING_OBJECT (upload, "failed to acquire buffer from bufferpool");
    return GST_GL_UPLOAD_ERROR;
  }

  for (i = 0; i < GST_GL_UPLOAD_MAX_PLANES; i++) {
    guint tex_id = 0;

    if (i < max_planes) {
      GstMemory *mem = gst_buffer_peek_memory (*outbuf, i);
      tex_id = ((GstGLMemory *) mem)->tex_id;
    }

    upload->texture_ids[i] = tex_id;
  }

  GST_LOG ("Uploading with GLTextureUploadMeta with textures "
      "%i,%i,%i,%i / %i,%i,%i,%i",
      upload->texture_ids[0], upload->texture_ids[1],
      upload->texture_ids[2], upload->texture_ids[3],
      upload->texture_ids[4], upload->texture_ids[5],
      upload->texture_ids[6], upload->texture_ids[7]);

  gst_gl_context_thread_add (upload->upload->context,
      (GstGLContextThreadFunc) _do_upload_with_meta, upload);

  if (!upload->result)
    return GST_GL_UPLOAD_ERROR;

  return GST_GL_UPLOAD_DONE;
}

static void
_upload_meta_upload_free (gpointer impl)
{
  struct GLUploadMeta *upload = impl;

  g_return_if_fail (impl != NULL);

  if (upload->pool)
    gst_object_unref (upload->pool);

  g_free (upload);
}

static GstStaticCaps _upload_meta_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_META_GST_VIDEO_GL_TEXTURE_UPLOAD_META, "RGBA"));

static const UploadMethod _upload_meta_upload = {
  "UploadMeta",
  METHOD_FLAG_CAN_SHARE_CONTEXT,
  &_upload_meta_upload_caps,
  &_upload_meta_upload_new,
  &_upload_meta_upload_transform_caps,
  &_upload_meta_upload_accept,
  &_upload_meta_upload_propose_allocation,
  &_upload_meta_upload_perform,
  &_upload_meta_upload_free
};

struct RawUploadFrame
{
  gint ref_count;
  GstVideoFrame frame;
};

struct RawUpload
{
  GstGLUpload *upload;
  struct RawUploadFrame *in_frame;
  GstGLVideoAllocationParams *params;
};

static struct RawUploadFrame *
_raw_upload_frame_new (struct RawUpload *raw, GstBuffer * buffer)
{
  struct RawUploadFrame *frame;
  GstVideoInfo *info;
  gint i;

  if (!buffer)
    return NULL;

  frame = g_new (struct RawUploadFrame, 1);
  frame->ref_count = 1;

  if (!gst_video_frame_map (&frame->frame, &raw->upload->priv->in_info,
          buffer, GST_MAP_READ)) {
    g_free (frame);
    return NULL;
  }

  raw->upload->priv->in_info = frame->frame.info;
  info = &raw->upload->priv->in_info;

  /* Recalculate the offsets (and size) */
  info->size = 0;
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    info->offset[i] = info->size;
    info->size += gst_gl_get_plane_data_size (info, NULL, i);
  }

  return frame;
}

static void
_raw_upload_frame_ref (struct RawUploadFrame *frame)
{
  g_atomic_int_inc (&frame->ref_count);
}

static void
_raw_upload_frame_unref (struct RawUploadFrame *frame)
{
  if (g_atomic_int_dec_and_test (&frame->ref_count)) {
    gst_video_frame_unmap (&frame->frame);
    g_free (frame);
  }
}

static gpointer
_raw_data_upload_new (GstGLUpload * upload)
{
  struct RawUpload *raw = g_new0 (struct RawUpload, 1);

  raw->upload = upload;

  return raw;
}

static GstCaps *
_raw_data_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCapsFeatures *passthrough =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    GstGLTextureTarget target_mask = 0;
    GstCapsFeatures *filter_features;
    GstCaps *tmp;

    filter_features =
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    if (!_filter_caps_with_features (caps, filter_features, &tmp)) {
      gst_caps_features_free (filter_features);
      gst_caps_features_free (passthrough);
      return NULL;
    }
    gst_caps_features_free (filter_features);

    ret = _set_caps_features_with_passthrough (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);
    gst_caps_unref (tmp);

    target_mask |= 1 << GST_GL_TEXTURE_TARGET_2D;
    target_mask |= 1 << GST_GL_TEXTURE_TARGET_RECTANGLE;
    tmp = _caps_intersect_texture_target (ret, target_mask);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;

    ret =
        _set_caps_features_with_passthrough (caps,
        GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, passthrough);

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  gst_caps_features_free (passthrough);

  return ret;
}

static gboolean
_raw_data_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct RawUpload *raw = impl;
  GstCapsFeatures *features;

  features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
  /* Also consider the omited system memory feature cases, such as
     video/x-raw(meta:GstVideoOverlayComposition) */
  if (!_filter_caps_with_features (in_caps, features, NULL)) {
    gst_caps_features_free (features);
    return FALSE;
  }
  gst_caps_features_free (features);

  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;

  if (raw->in_frame)
    _raw_upload_frame_unref (raw->in_frame);
  raw->in_frame = _raw_upload_frame_new (raw, buffer);

  if (raw->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) raw->params);
  if (!(raw->params =
          gst_gl_video_allocation_params_new_wrapped_data (raw->upload->context,
              NULL, &raw->upload->priv->in_info, -1, NULL,
              GST_GL_TEXTURE_TARGET_2D, 0, NULL, raw->in_frame,
              (GDestroyNotify) _raw_upload_frame_unref)))
    return FALSE;

  return (raw->in_frame != NULL);
}

static void
_raw_data_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, 0);
}

static GstGLUploadReturn
_raw_data_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  GstGLBaseMemoryAllocator *allocator;
  struct RawUpload *raw = impl;
  int i;
  GstVideoInfo *in_info = &raw->upload->priv->in_info;
  guint n_mem = GST_VIDEO_INFO_N_PLANES (in_info);

  allocator =
      GST_GL_BASE_MEMORY_ALLOCATOR (gst_gl_memory_allocator_get_default
      (raw->upload->context));

  /* FIXME Use a buffer pool to cache the generated textures */
  *outbuf = gst_buffer_new ();
  raw->params->parent.context = raw->upload->context;
  if (gst_gl_memory_setup_buffer ((GstGLMemoryAllocator *) allocator, *outbuf,
          raw->params, NULL, raw->in_frame->frame.data, n_mem)) {

    for (i = 0; i < n_mem; i++)
      _raw_upload_frame_ref (raw->in_frame);
    gst_buffer_add_gl_sync_meta (raw->upload->context, *outbuf);
  } else {
    GST_ERROR_OBJECT (raw->upload, "Failed to allocate wrapped texture");
    gst_buffer_unref (*outbuf);
    gst_object_unref (allocator);
    return GST_GL_UPLOAD_ERROR;
  }
  gst_object_unref (allocator);
  _raw_upload_frame_unref (raw->in_frame);
  raw->in_frame = NULL;

  return GST_GL_UPLOAD_DONE;
}

static void
_raw_data_upload_free (gpointer impl)
{
  struct RawUpload *raw = impl;

  if (raw->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) raw->params);

  g_free (raw);
}

static GstStaticCaps _raw_data_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_GL_MEMORY_VIDEO_FORMATS_STR));

static const UploadMethod _raw_data_upload = {
  "Raw Data",
  0,
  &_raw_data_upload_caps,
  &_raw_data_upload_new,
  &_raw_data_upload_transform_caps,
  &_raw_data_upload_accept,
  &_raw_data_upload_propose_allocation,
  &_raw_data_upload_perform,
  &_raw_data_upload_free
};

#if GST_GL_HAVE_VIV_DIRECTVIV
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT                                             0x80E1
#endif
#ifndef GL_VIV_YV12
#define GL_VIV_YV12                                             0x8FC0
#endif
#ifndef GL_VIV_NV12
#define GL_VIV_NV12                                             0x8FC1
#endif
#ifndef GL_VIV_YUY2
#define GL_VIV_YUY2                                             0x8FC2
#endif
#ifndef GL_VIV_UYVY
#define GL_VIV_UYVY                                             0x8FC3
#endif
#ifndef GL_VIV_NV21
#define GL_VIV_NV21                                             0x8FC4
#endif
#ifndef GL_VIV_I420
#define GL_VIV_I420                                             0x8FC5
#endif

struct DirectVIVUpload
{
  GstGLUpload *upload;

  GstGLVideoAllocationParams *params;
  GstBuffer *inbuf, *outbuf;
  void (*TexDirectVIVMap) (GLenum Target, GLsizei Width, GLsizei Height,
      GLenum Format, GLvoid ** Logical, const GLuint * Physical);
  void (*TexDirectInvalidateVIV) (GLenum Target);
  gboolean loaded_functions;
};

#define GST_GL_DIRECTVIV_FORMAT "{RGBA, I420, YV12, NV12, NV21, YUY2, UYVY, BGRA, RGB16}"

static GstStaticCaps _directviv_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_GL_DIRECTVIV_FORMAT));

static gpointer
_directviv_upload_new (GstGLUpload * upload)
{
  struct DirectVIVUpload *directviv = g_new0 (struct DirectVIVUpload, 1);
  directviv->upload = upload;
  directviv->loaded_functions = FALSE;

  return directviv;
}

static GstCaps *
_directviv_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  GstCapsFeatures *passthrough =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  GstCaps *ret;

  if (direction == GST_PAD_SINK) {
    GstCaps *tmp;
    GstCapsFeatures *filter_features;

    filter_features =
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    if (!_filter_caps_with_features (caps, filter_features, &tmp)) {
      gst_caps_features_free (filter_features);
      gst_caps_features_free (passthrough);
      return NULL;
    }
    gst_caps_features_free (filter_features);

    ret = _set_caps_features_with_passthrough (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);
    gst_caps_unref (tmp);

    gst_caps_set_simple (ret, "format", G_TYPE_STRING, "RGBA", NULL);
    tmp = _caps_intersect_texture_target (ret, 1 << GST_GL_TEXTURE_TARGET_2D);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    ret = gst_caps_from_string (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, GST_GL_DIRECTVIV_FORMAT));
  }

  gst_caps_features_free (passthrough);
  return ret;
}


static void
_directviv_upload_load_functions_gl_thread (GstGLContext * context,
    struct DirectVIVUpload *directviv)
{
  directviv->TexDirectVIVMap =
      gst_gl_context_get_proc_address (context, "glTexDirectVIVMap");
  directviv->TexDirectInvalidateVIV =
      gst_gl_context_get_proc_address (context, "glTexDirectInvalidateVIV");
}

static gboolean
_directviv_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct DirectVIVUpload *directviv = impl;
  GstCapsFeatures *features;
  guint n_mem;
  GstMemory *mem;

  if (!directviv->loaded_functions && (!directviv->TexDirectInvalidateVIV ||
          !directviv->TexDirectVIVMap)) {
    gst_gl_context_thread_add (directviv->upload->context,
        (GstGLContextThreadFunc) _directviv_upload_load_functions_gl_thread,
        directviv);
    directviv->loaded_functions = TRUE;
  }
  if (!directviv->TexDirectInvalidateVIV || !directviv->TexDirectVIVMap)
    return FALSE;

  features =
      gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
  /* Also consider the omited system memory feature cases, such as
     video/x-raw(meta:GstVideoOverlayComposition) */
  if (!_filter_caps_with_features (in_caps, features, NULL)) {
    gst_caps_features_free (features);
    return FALSE;
  }
  gst_caps_features_free (features);

  features = gst_caps_get_features (out_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    return FALSE;

  if (directviv->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) directviv->params);
  if (!(directviv->params =
          gst_gl_video_allocation_params_new (directviv->upload->context, NULL,
              &directviv->upload->priv->out_info, -1, NULL,
              GST_GL_TEXTURE_TARGET_2D, GST_VIDEO_GL_TEXTURE_TYPE_RGBA)))
    return FALSE;

  /* We only support a single memory per buffer at this point */
  n_mem = gst_buffer_n_memory (buffer);
  if (n_mem == 1) {
    mem = gst_buffer_peek_memory (buffer, 0);
  } else {
    mem = NULL;
  }

  return n_mem == 1 && mem && gst_is_phys_memory (mem);
}

static void
_directviv_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
}

static GLenum
_directviv_upload_video_format_to_gl_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
      return GL_VIV_I420;
    case GST_VIDEO_FORMAT_YV12:
      return GL_VIV_YV12;
    case GST_VIDEO_FORMAT_NV12:
      return GL_VIV_NV12;
    case GST_VIDEO_FORMAT_NV21:
      return GL_VIV_NV21;
    case GST_VIDEO_FORMAT_YUY2:
      return GL_VIV_YUY2;
    case GST_VIDEO_FORMAT_UYVY:
      return GL_VIV_UYVY;
    case GST_VIDEO_FORMAT_RGB16:
      return GL_RGB565;
    case GST_VIDEO_FORMAT_RGBA:
      return GL_RGBA;
    case GST_VIDEO_FORMAT_BGRA:
      return GL_BGRA_EXT;
    case GST_VIDEO_FORMAT_RGBx:
      return GL_RGBA;
    case GST_VIDEO_FORMAT_BGRx:
      return GL_BGRA_EXT;
    default:
      return 0;
  }
}

typedef struct
{
  GstBuffer *buffer;
  GstMemory *memory;
  GstMapInfo map;
  guintptr phys_addr;
} DirectVIVUnmapData;

static void
_directviv_memory_unmap (DirectVIVUnmapData * data)
{
  gst_memory_unmap (data->memory, &data->map);
  gst_memory_unref (data->memory);
  gst_buffer_unref (data->buffer);
  g_free (data);
}

static void
_directviv_upload_perform_gl_thread (GstGLContext * context,
    struct DirectVIVUpload *directviv)
{
  static GQuark directviv_unmap_quark = 0;
  GstGLMemoryAllocator *allocator;
  GstMemory *in_mem;
  GstGLMemory *out_gl_mem;
  GstVideoInfo *in_info;
  DirectVIVUnmapData *unmap_data;
  GstVideoMeta *vmeta;
  gint width, height, gl_format;
  const GstGLFuncs *gl;

  if (!directviv_unmap_quark)
    directviv_unmap_quark = g_quark_from_static_string ("GstGLDirectVIVUnmap");

  gl = context->gl_vtable;

  g_assert (gst_buffer_n_memory (directviv->inbuf) == 1);
  in_info = &directviv->upload->priv->in_info;
  in_mem = gst_buffer_peek_memory (directviv->inbuf, 0);
  unmap_data = g_new0 (DirectVIVUnmapData, 1);
  if (!gst_memory_map (in_mem, &unmap_data->map, GST_MAP_READ)) {
    g_free (unmap_data);
    return;
  }
  unmap_data->phys_addr = gst_phys_memory_get_phys_addr (in_mem);
  if (!unmap_data->phys_addr) {
    gst_memory_unmap (in_mem, &unmap_data->map);
    g_free (unmap_data);
    return;
  }
  unmap_data->memory = gst_memory_ref (in_mem);
  unmap_data->buffer = gst_buffer_ref (directviv->inbuf);

  allocator =
      GST_GL_MEMORY_ALLOCATOR (gst_allocator_find
      (GST_GL_MEMORY_PBO_ALLOCATOR_NAME));

  /* FIXME: buffer pool */
  directviv->outbuf = gst_buffer_new ();
  gst_gl_memory_setup_buffer (allocator, directviv->outbuf, directviv->params,
      NULL, NULL, 0);
  gst_object_unref (allocator);

  out_gl_mem = (GstGLMemory *) gst_buffer_peek_memory (directviv->outbuf, 0);

  /* Need to keep the input memory and buffer mapped and valid until
   * the GL memory is not used anymore */
  gst_mini_object_set_qdata ((GstMiniObject *) out_gl_mem,
      directviv_unmap_quark, unmap_data,
      (GDestroyNotify) _directviv_memory_unmap);
  gst_buffer_add_parent_buffer_meta (directviv->outbuf, directviv->inbuf);

  /* width/height need to compensate for stride/padding */
  vmeta = gst_buffer_get_video_meta (directviv->inbuf);
  if (vmeta) {
    width = vmeta->stride[0];
    if (GST_VIDEO_INFO_N_PLANES (in_info) == 1)
      height = gst_memory_get_sizes (in_mem, NULL, NULL) / width;
    else
      height = vmeta->offset[1] / width;
  } else {
    width = GST_VIDEO_INFO_PLANE_STRIDE (in_info, 0);
    if (GST_VIDEO_INFO_N_PLANES (in_info) == 1)
      height = gst_memory_get_sizes (in_mem, NULL, NULL) / width;
    else
      height = GST_VIDEO_INFO_PLANE_OFFSET (in_info, 1) / width;
  }
  width /= GST_VIDEO_INFO_COMP_PSTRIDE (in_info, 0);

  gl_format =
      _directviv_upload_video_format_to_gl_format (GST_VIDEO_INFO_FORMAT
      (in_info));

  gl->BindTexture (GL_TEXTURE_2D, out_gl_mem->tex_id);
  directviv->TexDirectVIVMap (GL_TEXTURE_2D, width, height,
      gl_format, (void **) &unmap_data->map.data,
      (GLuint *) & unmap_data->phys_addr);
  directviv->TexDirectInvalidateVIV (GL_TEXTURE_2D);
}

static GstGLUploadReturn
_directviv_upload_perform (gpointer impl, GstBuffer * buffer,
    GstBuffer ** outbuf)
{
  struct DirectVIVUpload *directviv = impl;

  directviv->inbuf = buffer;
  directviv->outbuf = NULL;
  gst_gl_context_thread_add (directviv->upload->context,
      (GstGLContextThreadFunc) _directviv_upload_perform_gl_thread, directviv);
  directviv->inbuf = NULL;

  if (!directviv->outbuf)
    return GST_GL_UPLOAD_ERROR;

  *outbuf = directviv->outbuf;
  directviv->outbuf = NULL;

  return GST_GL_UPLOAD_DONE;
}

static void
_directviv_upload_free (gpointer impl)
{
  struct DirectVIVUpload *directviv = impl;

  if (directviv->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) directviv->params);

  g_free (impl);
}

static const UploadMethod _directviv_upload = {
  "DirectVIV",
  0,
  &_directviv_upload_caps,
  &_directviv_upload_new,
  &_directviv_upload_transform_caps,
  &_directviv_upload_accept,
  &_directviv_upload_propose_allocation,
  &_directviv_upload_perform,
  &_directviv_upload_free
};

#endif /* GST_GL_HAVE_VIV_DIRECTVIV */

#if defined(HAVE_NVMM)
#include "nvbuf_utils.h"

struct NVMMUpload
{
  GstGLUpload *upload;

  GstGLVideoAllocationParams *params;
  guint n_mem;

  GstGLTextureTarget target;
  GstVideoInfo out_info;
  /* only used for pointer comparison */
  gpointer out_caps;
};

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"

/* FIXME: other formats? */
static GstStaticCaps _nvmm_upload_caps =
GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_NVMM,
        "RGBA"));

static gpointer
_nvmm_upload_new (GstGLUpload * upload)
{
  struct NVMMUpload *nvmm = g_new0 (struct NVMMUpload, 1);
  nvmm->upload = upload;
  nvmm->target = GST_GL_TEXTURE_TARGET_EXTERNAL_OES;
  return nvmm;
}

static GstCaps *
_nvmm_upload_transform_caps (gpointer impl, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps)
{
  struct NVMMUpload *nvmm = impl;
  GstCapsFeatures *passthrough;
  GstCaps *ret;

  if (context) {
    const GstGLFuncs *gl = context->gl_vtable;

    if (!gl->EGLImageTargetTexture2D)
      return NULL;

    /* Don't propose NVMM caps feature unless it can be supported */
    if (gst_gl_context_get_gl_platform (context) != GST_GL_PLATFORM_EGL)
      return NULL;

    if (!gst_gl_context_check_feature (context, "EGL_KHR_image_base"))
      return NULL;
  }

  passthrough = gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

  if (direction == GST_PAD_SINK) {
    GstCaps *tmp;
    GstCapsFeatures *filter_features;

    filter_features =
        gst_caps_features_from_string (GST_CAPS_FEATURE_MEMORY_NVMM);
    if (!_filter_caps_with_features (caps, filter_features, &tmp)) {
      gst_caps_features_free (filter_features);
      gst_caps_features_free (passthrough);
      return NULL;
    }
    gst_caps_features_free (filter_features);

    ret = _set_caps_features_with_passthrough (tmp,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY, passthrough);
    gst_caps_unref (tmp);

    tmp = _caps_intersect_texture_target (ret,
        1 << GST_GL_TEXTURE_TARGET_EXTERNAL_OES);
    gst_caps_unref (ret);
    ret = tmp;
  } else {
    gint i, n;

    ret =
        _set_caps_features_with_passthrough (caps,
        GST_CAPS_FEATURE_MEMORY_NVMM, passthrough);

    n = gst_caps_get_size (ret);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (ret, i);

      gst_structure_remove_fields (s, "texture-target", NULL);
    }
  }

  gst_caps_features_free (passthrough);

  GST_DEBUG_OBJECT (nvmm->upload, "transformed %" GST_PTR_FORMAT " into %"
      GST_PTR_FORMAT, caps, ret);

  return ret;
}

static gboolean
_nvmm_upload_accept (gpointer impl, GstBuffer * buffer, GstCaps * in_caps,
    GstCaps * out_caps)
{
  struct NVMMUpload *nvmm = impl;
  GstVideoInfo *in_info = &nvmm->upload->priv->in_info;
  GstVideoInfo *out_info = &nvmm->out_info;
  GstVideoMeta *meta;
  GstMapInfo in_map_info = GST_MAP_INFO_INIT;
  GstCapsFeatures *features;
  guint n_mem;
  guint i;

  n_mem = gst_buffer_n_memory (buffer);
  if (n_mem != 1) {
    GST_DEBUG_OBJECT (nvmm->upload, "NVMM uploader only supports "
        "1 memory, not %u", n_mem);
    return FALSE;
  }

  meta = gst_buffer_get_video_meta (buffer);

  if (!nvmm->upload->context->gl_vtable->EGLImageTargetTexture2D)
    return FALSE;

  /* NVMM upload is only supported with EGL contexts. */
  if (gst_gl_context_get_gl_platform (nvmm->upload->context) !=
      GST_GL_PLATFORM_EGL)
    return FALSE;

  if (!gst_gl_context_check_feature (nvmm->upload->context,
          "EGL_KHR_image_base"))
    return FALSE;

  features = gst_caps_get_features (in_caps, 0);
  if (!gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_NVMM))
    return FALSE;

  if (!gst_buffer_map (buffer, &in_map_info, GST_MAP_READ)) {
    GST_DEBUG_OBJECT (nvmm->upload, "Failed to map readonly NvBuffer");
    return FALSE;
  }
  if (in_map_info.size != NvBufferGetSize ()) {
    GST_DEBUG_OBJECT (nvmm->upload, "Memory size (%" G_GSIZE_FORMAT ") is "
        "not the same as what NvBuffer advertises (%u)", in_map_info.size,
        NvBufferGetSize ());
    gst_buffer_unmap (buffer, &in_map_info);
    return FALSE;
  }
  gst_buffer_unmap (buffer, &in_map_info);

  /* Update video info based on video meta */
  if (meta) {
    in_info->width = meta->width;
    in_info->height = meta->height;

    for (i = 0; i < meta->n_planes; i++) {
      in_info->offset[i] = meta->offset[i];
      in_info->stride[i] = meta->stride[i];
    }
  }

  if (out_caps != nvmm->out_caps) {
    nvmm->out_caps = out_caps;
    if (!gst_video_info_from_caps (out_info, out_caps))
      return FALSE;
  }

  if (nvmm->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) nvmm->params);
  if (!(nvmm->params =
          gst_gl_video_allocation_params_new_wrapped_gl_handle (nvmm->
              upload->context, NULL, out_info, -1, NULL, nvmm->target, 0, NULL,
              NULL, NULL))) {
    return FALSE;
  }

  return TRUE;
}

static void
_nvmm_upload_propose_allocation (gpointer impl, GstQuery * decide_query,
    GstQuery * query)
{
  /* nothing to do for now. */
}

static void
_egl_image_mem_unref (GstEGLImage * image, GstMemory * mem)
{
  GstGLDisplayEGL *egl_display = NULL;
  EGLDisplay display;

  egl_display = gst_gl_display_egl_from_gl_display (image->context->display);
  if (!egl_display) {
    GST_ERROR ("Could not retrieve GstGLDisplayEGL from GstGLDisplay");
    return;
  }
  display =
      (EGLDisplay) gst_gl_display_get_handle (GST_GL_DISPLAY (egl_display));

  if (NvDestroyEGLImage (display, image->image)) {
    GST_ERROR ("Failed to destroy EGLImage %p from NvBuffer", image->image);
  } else {
    GST_DEBUG ("destroyed EGLImage %p from NvBuffer", image->image);
  }

  gst_memory_unref (mem);
  gst_object_unref (egl_display);
}

static const char *
payload_type_to_string (NvBufferPayloadType ptype)
{
  switch (ptype) {
    case NvBufferPayload_SurfArray:
      return "SurfArray";
    case NvBufferPayload_MemHandle:
      return "MemHandle";
    default:
      return "<unknown>";
  }
}

static const char *
pixel_format_to_string (NvBufferColorFormat fmt)
{
  switch (fmt) {
    case NvBufferColorFormat_YUV420:
      return "YUV420";
    case NvBufferColorFormat_YVU420:
      return "YVU420";
    case NvBufferColorFormat_YUV422:
      return "YUV422";
    case NvBufferColorFormat_YUV420_ER:
      return "YUV420_ER";
    case NvBufferColorFormat_YVU420_ER:
      return "YVU420_ER";
    case NvBufferColorFormat_NV12:
      return "NV12";
    case NvBufferColorFormat_NV12_ER:
      return "NV12_ER";
    case NvBufferColorFormat_NV21:
      return "NV21";
    case NvBufferColorFormat_NV21_ER:
      return "NV21_ER";
    case NvBufferColorFormat_UYVY:
      return "UYVY";
    case NvBufferColorFormat_UYVY_ER:
      return "UYVY_ER";
    case NvBufferColorFormat_VYUY:
      return "VYUY";
    case NvBufferColorFormat_VYUY_ER:
      return "VYUY_ER";
    case NvBufferColorFormat_YUYV:
      return "YUYV";
    case NvBufferColorFormat_YUYV_ER:
      return "YUYV_ER";
    case NvBufferColorFormat_YVYU:
      return "YVYU";
    case NvBufferColorFormat_YVYU_ER:
      return "YVYU_ER";
    case NvBufferColorFormat_ABGR32:
      return "ABGR32";
    case NvBufferColorFormat_XRGB32:
      return "XRGB32";
    case NvBufferColorFormat_ARGB32:
      return "ARGB32";
    case NvBufferColorFormat_NV12_10LE:
      return "NV12_10LE";
    case NvBufferColorFormat_NV12_10LE_709:
      return "NV12_10LE_709";
    case NvBufferColorFormat_NV12_10LE_709_ER:
      return "NV12_10LE_709_ER";
    case NvBufferColorFormat_NV12_10LE_2020:
      return "NV12_2020";
    case NvBufferColorFormat_NV21_10LE:
      return "NV21_10LE";
    case NvBufferColorFormat_NV12_12LE:
      return "NV12_12LE";
    case NvBufferColorFormat_NV12_12LE_2020:
      return "NV12_12LE_2020";
    case NvBufferColorFormat_NV21_12LE:
      return "NV21_12LE";
    case NvBufferColorFormat_YUV420_709:
      return "YUV420_709";
    case NvBufferColorFormat_YUV420_709_ER:
      return "YUV420_709_ER";
    case NvBufferColorFormat_NV12_709:
      return "NV12_709";
    case NvBufferColorFormat_NV12_709_ER:
      return "NV12_709_ER";
    case NvBufferColorFormat_YUV420_2020:
      return "YUV420_2020";
    case NvBufferColorFormat_NV12_2020:
      return "NV12_2020";
    case NvBufferColorFormat_SignedR16G16:
      return "SignedR16G16";
    case NvBufferColorFormat_A32:
      return "A32";
    case NvBufferColorFormat_YUV444:
      return "YUV444";
    case NvBufferColorFormat_GRAY8:
      return "GRAY8";
    case NvBufferColorFormat_NV16:
      return "NV16";
    case NvBufferColorFormat_NV16_10LE:
      return "NV16_10LE";
    case NvBufferColorFormat_NV24:
      return "NV24";
    case NvBufferColorFormat_NV16_ER:
      return "NV16_ER";
    case NvBufferColorFormat_NV24_ER:
      return "NV24_ER";
    case NvBufferColorFormat_NV16_709:
      return "NV16_709";
    case NvBufferColorFormat_NV24_709:
      return "NV24_709";
    case NvBufferColorFormat_NV16_709_ER:
      return "NV16_709_ER";
    case NvBufferColorFormat_NV24_709_ER:
      return "NV24_709_ER";
    case NvBufferColorFormat_NV24_10LE_709:
      return "NV24_10LE_709";
    case NvBufferColorFormat_NV24_10LE_709_ER:
      return "NV24_10LE_709_ER";
    case NvBufferColorFormat_NV24_10LE_2020:
      return "NV24_10LE_2020";
    case NvBufferColorFormat_NV24_12LE_2020:
      return "NV24_12LE_2020";
    case NvBufferColorFormat_RGBA_10_10_10_2_709:
      return "RGBA_10_10_10_2_709";
    case NvBufferColorFormat_RGBA_10_10_10_2_2020:
      return "RGBA_10_10_10_2_2020";
    case NvBufferColorFormat_BGRA_10_10_10_2_709:
      return "BGRA_10_10_10_2_709";
    case NvBufferColorFormat_BGRA_10_10_10_2_2020:
      return "BGRA_10_10_10_2_2020";
    case NvBufferColorFormat_Invalid:
      return "Invalid";
    default:
      return "<unknown>";
  }
}

static void
dump_nv_buf_params (GstObject * debug_object, NvBufferParamsEx * params)
{
  GST_DEBUG_OBJECT (debug_object, "nvbuffer fd: %u size %i nv_buffer: %p of "
      "size %u, payload: (0x%x) %s, pixel format: (0x%x) %s, n_planes: %u, "
      "plane 0 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u } "
      "plane 1 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u } "
      "plane 2 { wxh: %ux%u, pitch: %u, offset: %u, psize: %u, layout: %u }",
      params->params.dmabuf_fd, params->params.memsize,
      params->params.nv_buffer, params->params.nv_buffer_size,
      params->params.payloadType,
      payload_type_to_string (params->params.payloadType),
      params->params.pixel_format,
      pixel_format_to_string (params->params.pixel_format),
      params->params.num_planes, params->params.width[0],
      params->params.height[0], params->params.pitch[0],
      params->params.offset[0], params->params.psize[0],
      params->params.offset[0], params->params.width[1],
      params->params.height[1], params->params.pitch[1],
      params->params.offset[1], params->params.psize[1],
      params->params.offset[1], params->params.width[2],
      params->params.height[2], params->params.pitch[2],
      params->params.offset[2], params->params.psize[2],
      params->params.offset[2]);
}

static GstGLUploadReturn
_nvmm_upload_perform (gpointer impl, GstBuffer * buffer, GstBuffer ** outbuf)
{
  struct NVMMUpload *nvmm = impl;
  GstGLMemoryAllocator *allocator = NULL;
  GstMapInfo in_map_info = GST_MAP_INFO_INIT;
  GstGLDisplayEGL *egl_display = NULL;
  GstEGLImage *eglimage = NULL;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLImageKHR image = EGL_NO_IMAGE;
  int in_dmabuf_fd;
  NvBufferParamsEx params = { 0, };
  GstGLUploadReturn ret = GST_GL_UPLOAD_ERROR;

  if (!gst_buffer_map (buffer, &in_map_info, GST_MAP_READ)) {
    GST_DEBUG_OBJECT (nvmm->upload, "Failed to map readonly NvBuffer");
    goto done;
  }

  if (ExtractFdFromNvBuffer (in_map_info.data, &in_dmabuf_fd)) {
    GST_DEBUG_OBJECT (nvmm->upload, "Failed to extract fd from NvBuffer");
    goto done;
  }
  if (NvBufferGetParamsEx (in_dmabuf_fd, &params)) {
    GST_WARNING_OBJECT (nvmm->upload, "Failed to get NvBuffer params");
    goto done;
  }
  dump_nv_buf_params ((GstObject *) nvmm->upload, &params);

  egl_display =
      gst_gl_display_egl_from_gl_display (nvmm->upload->context->display);
  if (!egl_display) {
    GST_WARNING ("Failed to retrieve GstGLDisplayEGL from GstGLDisplay");
    goto done;
  }
  display =
      (EGLDisplay) gst_gl_display_get_handle (GST_GL_DISPLAY (egl_display));

  image = NvEGLImageFromFd (display, in_dmabuf_fd);
  if (!image) {
    GST_DEBUG_OBJECT (nvmm->upload, "Failed construct EGLImage "
        "from NvBuffer fd %i", in_dmabuf_fd);
    goto done;
  }
  GST_DEBUG_OBJECT (nvmm->upload, "constructed EGLImage %p "
      "from NvBuffer fd %i", image, in_dmabuf_fd);

  eglimage = gst_egl_image_new_wrapped (nvmm->upload->context, image,
      GST_GL_RGBA, gst_memory_ref (in_map_info.memory),
      (GstEGLImageDestroyNotify) _egl_image_mem_unref);
  if (!eglimage) {
    GST_WARNING_OBJECT (nvmm->upload, "Failed to wrap constructed "
        "EGLImage from NvBuffer");
    goto done;
  }

  gst_buffer_unmap (buffer, &in_map_info);
  in_map_info = (GstMapInfo) GST_MAP_INFO_INIT;

  allocator =
      GST_GL_MEMORY_ALLOCATOR (gst_allocator_find
      (GST_GL_MEMORY_EGL_ALLOCATOR_NAME));

  /* TODO: buffer pool */
  *outbuf = gst_buffer_new ();
  if (!gst_gl_memory_setup_buffer (allocator, *outbuf, nvmm->params,
          NULL, (gpointer *) & eglimage, 1)) {
    GST_WARNING_OBJECT (nvmm->upload, "Failed to setup "
        "NVMM -> EGLImage buffer");
    goto done;
  }
  gst_egl_image_unref (eglimage);

  gst_buffer_add_parent_buffer_meta (*outbuf, buffer);

  /* TODO: NvBuffer has some sync functions that may be more useful here */
  {
    GstGLSyncMeta *sync_meta;

    sync_meta = gst_buffer_add_gl_sync_meta (nvmm->upload->context, *outbuf);
    if (sync_meta) {
      gst_gl_sync_meta_set_sync_point (sync_meta, nvmm->upload->context);
    }
  }

  ret = GST_GL_UPLOAD_DONE;

done:
  if (in_map_info.memory)
    gst_buffer_unmap (buffer, &in_map_info);

  gst_clear_object (&egl_display);
  gst_clear_object (&allocator);

  return ret;
}

static void
_nvmm_upload_free (gpointer impl)
{
  struct NVMMUpload *nvmm = impl;

  if (nvmm->params)
    gst_gl_allocation_params_free ((GstGLAllocationParams *) nvmm->params);

  g_free (impl);
}

static const UploadMethod _nvmm_upload = {
  "NVMM",
  0,
  &_nvmm_upload_caps,
  &_nvmm_upload_new,
  &_nvmm_upload_transform_caps,
  &_nvmm_upload_accept,
  &_nvmm_upload_propose_allocation,
  &_nvmm_upload_perform,
  &_nvmm_upload_free
};

#endif /* HAVE_NVMM */

static const UploadMethod *upload_methods[] = {
  &_passthrough_upload,
  &_gl_memory_upload,
#if GST_GL_HAVE_DMABUF
  &_direct_dma_buf_upload,
  &_direct_dma_buf_external_upload,
  &_dma_buf_upload,
#endif
#if GST_GL_HAVE_VIV_DIRECTVIV
  &_directviv_upload,
#endif
#if defined(HAVE_NVMM)
  &_nvmm_upload,
#endif /* HAVE_NVMM */
  &_upload_meta_upload,
  /* Raw data must always be last / least preferred */
  &_raw_data_upload
};

static GMutex upload_global_lock;

GstCaps *
gst_gl_upload_get_input_template_caps (void)
{
  GstCaps *ret = NULL;
  gint i;

  g_mutex_lock (&upload_global_lock);

  /* FIXME: cache this and invalidate on changes to upload_methods */
  for (i = 0; i < G_N_ELEMENTS (upload_methods); i++) {
    GstCaps *template =
        gst_static_caps_get (upload_methods[i]->input_template_caps);
    ret = ret == NULL ? template : gst_caps_merge (ret, template);
  }

  ret = gst_caps_simplify (ret);
  ret = gst_gl_overlay_compositor_add_caps (ret);
  g_mutex_unlock (&upload_global_lock);

  return ret;
}

static void
gst_gl_upload_class_init (GstGLUploadClass * klass)
{
  G_OBJECT_CLASS (klass)->finalize = gst_gl_upload_finalize;
}

static void
gst_gl_upload_init (GstGLUpload * upload)
{
  upload->priv = gst_gl_upload_get_instance_private (upload);
}

/**
 * gst_gl_upload_new:
 * @context: a #GstGLContext
 *
 * Returns: (transfer full): a new #GstGLUpload object
 */
GstGLUpload *
gst_gl_upload_new (GstGLContext * context)
{
  GstGLUpload *upload = g_object_new (GST_TYPE_GL_UPLOAD, NULL);
  gint i, n;

  gst_object_ref_sink (upload);

  if (context)
    gst_gl_upload_set_context (upload, context);
  else
    upload->context = NULL;

  n = G_N_ELEMENTS (upload_methods);
  upload->priv->upload_impl = g_malloc (sizeof (gpointer) * n);
  for (i = 0; i < n; i++) {
    upload->priv->upload_impl[i] = upload_methods[i]->new (upload);
  }

  GST_DEBUG_OBJECT (upload, "Created new GLUpload for context %" GST_PTR_FORMAT,
      context);

  return upload;
}

void
gst_gl_upload_set_context (GstGLUpload * upload, GstGLContext * context)
{
  g_return_if_fail (upload != NULL);

  gst_object_replace ((GstObject **) & upload->context, (GstObject *) context);
}

static void
gst_gl_upload_finalize (GObject * object)
{
  GstGLUpload *upload;
  gint i, n;

  upload = GST_GL_UPLOAD (object);

  upload->priv->method_i = 0;

  if (upload->context) {
    gst_object_unref (upload->context);
    upload->context = NULL;
  }

  if (upload->priv->in_caps) {
    gst_caps_unref (upload->priv->in_caps);
    upload->priv->in_caps = NULL;
  }

  if (upload->priv->out_caps) {
    gst_caps_unref (upload->priv->out_caps);
    upload->priv->out_caps = NULL;
  }

  n = G_N_ELEMENTS (upload_methods);
  for (i = 0; i < n; i++) {
    if (upload->priv->upload_impl[i])
      upload_methods[i]->free (upload->priv->upload_impl[i]);
  }
  g_free (upload->priv->upload_impl);

  G_OBJECT_CLASS (gst_gl_upload_parent_class)->finalize (object);
}

GstCaps *
gst_gl_upload_transform_caps (GstGLUpload * upload, GstGLContext * context,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;
  gint i;

  if (upload->priv->method) {
    tmp = upload->priv->method->transform_caps (upload->priv->method_impl,
        context, direction, caps);

    if (tmp) {
      /* If we're generating sink pad caps, make sure to include raw caps if needed by
       * the current method */
      if (direction == GST_PAD_SRC
          && (upload->priv->method->flags & METHOD_FLAG_CAN_ACCEPT_RAW)) {
        GstCapsFeatures *passthrough =
            gst_caps_features_from_string
            (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
        GstCaps *raw_tmp = _set_caps_features_with_passthrough (tmp,
            GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, passthrough);
        gst_caps_append (tmp, raw_tmp);
        gst_caps_features_free (passthrough);
      }

      if (filter) {
        result =
            gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref (tmp);
      } else {
        result = tmp;
      }
      if (!gst_caps_is_empty (result))
        return result;
      else
        gst_caps_unref (result);
    }
  }

  tmp = gst_caps_new_empty ();

  for (i = 0; i < G_N_ELEMENTS (upload_methods); i++) {
    GstCaps *tmp2;

    tmp2 =
        upload_methods[i]->transform_caps (upload->priv->upload_impl[i],
        context, direction, caps);

    if (tmp2)
      tmp = gst_caps_merge (tmp, tmp2);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  return result;
}

/**
 * gst_gl_upload_propose_allocation:
 * @upload: a #GstGLUpload
 * @decide_query: (allow-none): a #GstQuery from a decide allocation
 * @query: the proposed allocation query
 *
 * Adds the required allocation parameters to support uploading.
 */
void
gst_gl_upload_propose_allocation (GstGLUpload * upload, GstQuery * decide_query,
    GstQuery * query)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (upload_methods); i++)
    upload_methods[i]->propose_allocation (upload->priv->upload_impl[i],
        decide_query, query);
}

static gboolean
_gst_gl_upload_set_caps_unlocked (GstGLUpload * upload, GstCaps * in_caps,
    GstCaps * out_caps)
{
  g_return_val_if_fail (upload != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (in_caps), FALSE);

  if (upload->priv->in_caps && upload->priv->out_caps
      && gst_caps_is_equal (upload->priv->in_caps, in_caps)
      && gst_caps_is_equal (upload->priv->out_caps, out_caps))
    return TRUE;

  gst_caps_replace (&upload->priv->in_caps, in_caps);
  gst_caps_replace (&upload->priv->out_caps, out_caps);

  gst_video_info_dma_drm_init (&upload->priv->in_info_drm);
  if (gst_video_is_dma_drm_caps (in_caps)) {
    gst_video_info_dma_drm_from_caps (&upload->priv->in_info_drm, in_caps);
  } else {
    gst_video_info_from_caps (&upload->priv->in_info, in_caps);
    gst_video_info_dma_drm_from_video_info (&upload->priv->in_info_drm,
        &upload->priv->in_info, DRM_FORMAT_MOD_LINEAR);
  }
  gst_video_info_from_caps (&upload->priv->out_info, out_caps);

  upload->priv->method = NULL;
  upload->priv->method_impl = NULL;
  upload->priv->method_i = 0;

  return TRUE;
}

/**
 * gst_gl_upload_set_caps:
 * @upload: a #GstGLUpload
 * @in_caps: input #GstCaps
 * @out_caps: output #GstCaps
 *
 * Initializes @upload with the information required for upload.
 *
 * Returns: whether @in_caps and @out_caps could be set on @upload
 */
gboolean
gst_gl_upload_set_caps (GstGLUpload * upload, GstCaps * in_caps,
    GstCaps * out_caps)
{
  gboolean ret;

  GST_OBJECT_LOCK (upload);
  ret = _gst_gl_upload_set_caps_unlocked (upload, in_caps, out_caps);
  GST_OBJECT_UNLOCK (upload);

  return ret;
}

/**
 * gst_gl_upload_get_caps:
 * @upload: a #GstGLUpload
 * @in_caps: (transfer full) (allow-none) (out): the input #GstCaps
 * @out_caps: (transfer full) (allow-none) (out): the output #GstCaps
 */
void
gst_gl_upload_get_caps (GstGLUpload * upload, GstCaps ** in_caps,
    GstCaps ** out_caps)
{
  GST_OBJECT_LOCK (upload);
  if (in_caps)
    *in_caps =
        upload->priv->in_caps ? gst_caps_ref (upload->priv->in_caps) : NULL;
  if (out_caps)
    *out_caps =
        upload->priv->out_caps ? gst_caps_ref (upload->priv->out_caps) : NULL;
  GST_OBJECT_UNLOCK (upload);
}

static gboolean
_upload_find_method (GstGLUpload * upload, gpointer last_impl)
{
  gint method_i;

  /* start with the last used method after explicitly reconfiguring to
   * negotiate caps for this method */
  if (upload->priv->method_i == 0) {
    upload->priv->method_i = upload->priv->saved_method_i;
    upload->priv->saved_method_i = 0;
  }

  if (upload->priv->method_i >= G_N_ELEMENTS (upload_methods)) {
    if (last_impl)
      upload->priv->method_i = 0;
    else
      return FALSE;
  }

  method_i = upload->priv->method_i;

  if (last_impl == upload->priv->upload_impl[method_i])
    return FALSE;

  upload->priv->method = upload_methods[method_i];
  upload->priv->method_impl = upload->priv->upload_impl[method_i];

  GST_DEBUG_OBJECT (upload, "attempting upload with uploader %s",
      upload->priv->method->name);

  upload->priv->method_i++;

  return TRUE;
}

/**
 * gst_gl_upload_perform_with_buffer:
 * @upload: a #GstGLUpload
 * @buffer: input #GstBuffer
 * @outbuf_ptr: (out): resulting #GstBuffer
 *
 * Uploads @buffer using the transformation specified by
 * gst_gl_upload_set_caps() creating a new #GstBuffer in @outbuf_ptr.
 *
 * Returns: whether the upload was successful
 */
GstGLUploadReturn
gst_gl_upload_perform_with_buffer (GstGLUpload * upload, GstBuffer * buffer,
    GstBuffer ** outbuf_ptr)
{
  GstGLUploadReturn ret = GST_GL_UPLOAD_ERROR;
  GstBuffer *outbuf = NULL;
  gpointer last_impl = upload->priv->method_impl;
#if !defined (GST_DISABLE_DEBUG)
  const UploadMethod *last_method = upload->priv->method;
#endif

  g_return_val_if_fail (GST_IS_GL_UPLOAD (upload), FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);
  g_return_val_if_fail (outbuf_ptr != NULL, FALSE);

  GST_OBJECT_LOCK (upload);

#define NEXT_METHOD \
do { \
  if (!_upload_find_method (upload, last_impl)) { \
    GST_OBJECT_UNLOCK (upload); \
    return FALSE; \
  } \
  goto restart; \
} while (0)

  if (!upload->priv->method_impl)
    _upload_find_method (upload, last_impl);

restart:
  if (!upload->priv->method->accept (upload->priv->method_impl, buffer,
          upload->priv->in_caps, upload->priv->out_caps))
    NEXT_METHOD;

  ret =
      upload->priv->method->perform (upload->priv->method_impl, buffer,
      &outbuf);
  GST_LOG_OBJECT (upload, "uploader %s returned %u, buffer: %p",
      upload->priv->method->name, ret, outbuf);
  if (ret == GST_GL_UPLOAD_UNSHARED_GL_CONTEXT) {
    gint i;

    for (i = 0; i < G_N_ELEMENTS (upload_methods); i++) {
      if (upload_methods[i] == &_raw_data_upload) {
        upload->priv->method = &_raw_data_upload;
        upload->priv->method_impl = upload->priv->upload_impl[i];
        upload->priv->method_i = i;

        break;
      }
    }

    gst_buffer_replace (&outbuf, NULL);
    goto restart;
  } else if (ret == GST_GL_UPLOAD_DONE || ret == GST_GL_UPLOAD_RECONFIGURE) {
    if (last_impl != upload->priv->method_impl
        && upload->priv->method_impl != NULL) {
      /* Transform the input caps using the new method. If they are compatible with the
       * existing upload method, we can skip reconfiguration */
      GstCaps *caps =
          upload->priv->method->transform_caps (upload->priv->method_impl,
          upload->context, GST_PAD_SINK, upload->priv->in_caps);

      GST_LOG_OBJECT (upload,
          "Changing uploader from %s to %s with src caps %" GST_PTR_FORMAT
          " and old src caps %" GST_PTR_FORMAT,
          last_method != NULL ? last_method->name : "None",
          upload->priv->method->name, caps, upload->priv->out_caps);

      if (caps == NULL || !gst_caps_is_subset (caps, upload->priv->out_caps)) {
        gst_buffer_replace (&outbuf, NULL);
        ret = GST_GL_UPLOAD_RECONFIGURE;
      }
      gst_caps_replace (&caps, NULL);
    }
    /* we are done */
  } else {
    upload->priv->method_impl = NULL;
    gst_buffer_replace (&outbuf, NULL);
    NEXT_METHOD;
  }

  if (outbuf && buffer != outbuf)
    gst_buffer_copy_into (outbuf, buffer,
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
  *outbuf_ptr = outbuf;

  if (ret == GST_GL_UPLOAD_RECONFIGURE)
    upload->priv->saved_method_i = upload->priv->method_i - 1;

  GST_OBJECT_UNLOCK (upload);

  return ret;

#undef NEXT_METHOD
}

/**
 * gst_gl_upload_fixate_caps:
 * @upload: a #GstGLUpload
 * @direction: the pad #GstPadDirection
 * @caps: a #GstCaps as the reference
 * @othercaps: (transfer full): a #GstCaps to fixate
 *
 * Fixate the @othercaps based on the information of the @caps.
 *
 * Returns: (transfer full): the fixated caps
 *
 * Since: 1.24
 */
GstCaps *
gst_gl_upload_fixate_caps (GstGLUpload * upload, GstPadDirection direction,
    GstCaps * caps, GstCaps * othercaps)
{
  guint n, i;
  GstGLTextureTarget target;
  GstCaps *ret_caps = NULL;

  GST_DEBUG_OBJECT (upload, "Fixate caps %" GST_PTR_FORMAT ", using caps %"
      GST_PTR_FORMAT ", direction is %s.", othercaps, caps,
      direction == GST_PAD_SRC ? "src" : "sink");

  if (direction == GST_PAD_SRC) {
    ret_caps = gst_caps_fixate (othercaps);
    goto out;
  }

  if (gst_caps_is_fixed (othercaps)) {
    ret_caps = othercaps;
    goto out;
  }

  /* Prefer target 2D->rectangle->oes */
  for (target = GST_GL_TEXTURE_TARGET_2D;
      target <= GST_GL_TEXTURE_TARGET_EXTERNAL_OES; target++) {
    n = gst_caps_get_size (othercaps);
    for (i = 0; i < n; i++) {
      GstStructure *s;

      s = gst_caps_get_structure (othercaps, i);
      if (_structure_check_target (s, 1 << target))
        break;
    }

    /* If the target is found, fixate the other fields */
    if (i < n) {
      ret_caps = gst_caps_new_empty ();
      gst_caps_append_structure_full (ret_caps,
          gst_structure_copy (gst_caps_get_structure (othercaps, i)),
          gst_caps_features_copy (gst_caps_get_features (othercaps, i)));

      ret_caps = gst_caps_fixate (ret_caps);
      gst_caps_set_simple (ret_caps, "texture-target", G_TYPE_STRING,
          gst_gl_texture_target_to_string (target), NULL);

      gst_caps_unref (othercaps);

      goto out;
    }
  }

  ret_caps = gst_caps_fixate (othercaps);

out:
  GST_DEBUG_OBJECT (upload, "Fixate return %" GST_PTR_FORMAT, ret_caps);
  return ret_caps;
}
