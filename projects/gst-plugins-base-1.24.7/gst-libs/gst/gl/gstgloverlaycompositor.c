/*
 * GStreamer
 * Copyright (C) 2015 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
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

/**
 * SECTION:gstgloverlaycompositor
 * @title: GstGLOverlayCompositor
 * @short_description: Composite multiple overlays using OpenGL
 * @see_also: #GstGLMemory, #GstGLContext
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "gstgloverlaycompositor.h"

#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

GST_DEBUG_CATEGORY_STATIC (gst_gl_overlay_compositor_debug);
#define GST_CAT_DEFAULT gst_gl_overlay_compositor_debug

/*****************************************************************************
 * GstGLCompositionOverlay object is internally used by GstGLOverlayCompositor
 *****************************************************************************/

#define GST_TYPE_GL_COMPOSITION_OVERLAY (gst_gl_composition_overlay_get_type())
#define GST_GL_COMPOSITION_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_COMPOSITION_OVERLAY,\
                              GstGLCompositionOverlay))

typedef struct _GstGLCompositionOverlay GstGLCompositionOverlay;
typedef struct _GstGLCompositionOverlayClass GstGLCompositionOverlayClass;

static GType gst_gl_composition_overlay_get_type (void);

/* *INDENT-OFF* */
const gchar *fragment_shader =
  "varying vec2 v_texcoord;\n"
  "uniform sampler2D tex;\n"
  "void main(void)\n"
  "{\n"
  "  vec4 t = texture2D(tex, v_texcoord);\n"
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  "  gl_FragColor = t.bgra;\n"
#else
  "  gl_FragColor = t.gbar;\n"
#endif
  "}";
/* *INDENT-ON* */

struct _GstGLCompositionOverlay
{
  GstObject parent;
  GstGLContext *context;

  GLuint vao;
  GLuint index_buffer;
  GLuint position_buffer;
  GLuint texcoord_buffer;
  GLint position_attrib;
  GLint texcoord_attrib;

  GLfloat positions[16];

  GLuint texture_id;
  GstGLMemory *gl_memory;
  GstVideoOverlayRectangle *rectangle;

  gboolean yinvert;
};

struct _GstGLCompositionOverlayClass
{
  GstObjectClass object_class;
};

G_DEFINE_TYPE (GstGLCompositionOverlay, gst_gl_composition_overlay,
    GST_TYPE_OBJECT);

static void
gst_gl_composition_overlay_init_vertex_buffer (GstGLContext * context,
    gpointer overlay_pointer)
{
  const GstGLFuncs *gl = context->gl_vtable;
  GstGLCompositionOverlay *overlay =
      (GstGLCompositionOverlay *) overlay_pointer;

  /* *INDENT-OFF* */
  static const GLfloat texcoords[] = {
      1.0f, 0.0f,
      0.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f
  };

  static const GLushort indices[] = {
    0, 1, 2, 0, 2, 3
  };
  /* *INDENT-ON* */

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &overlay->vao);
    gl->BindVertexArray (overlay->vao);
  }

  gl->GenBuffers (1, &overlay->position_buffer);
  gl->BindBuffer (GL_ARRAY_BUFFER, overlay->position_buffer);
  gl->BufferData (GL_ARRAY_BUFFER, 4 * 4 * sizeof (GLfloat), overlay->positions,
      GL_STATIC_DRAW);

  /* Load the vertex position */
  gl->VertexAttribPointer (overlay->position_attrib, 4, GL_FLOAT, GL_FALSE,
      4 * sizeof (GLfloat), NULL);

  gl->GenBuffers (1, &overlay->texcoord_buffer);
  gl->BindBuffer (GL_ARRAY_BUFFER, overlay->texcoord_buffer);
  gl->BufferData (GL_ARRAY_BUFFER, 4 * 2 * sizeof (GLfloat), texcoords,
      GL_STATIC_DRAW);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (overlay->texcoord_attrib, 2, GL_FLOAT, GL_FALSE,
      2 * sizeof (GLfloat), NULL);

  gl->GenBuffers (1, &overlay->index_buffer);
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, overlay->index_buffer);
  gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
      GL_STATIC_DRAW);

  gl->EnableVertexAttribArray (overlay->position_attrib);
  gl->EnableVertexAttribArray (overlay->texcoord_attrib);

  if (gl->GenVertexArrays) {
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
}

static void
gst_gl_composition_overlay_free_vertex_buffer (GstGLContext * context,
    gpointer overlay_pointer)
{
  const GstGLFuncs *gl = context->gl_vtable;
  GstGLCompositionOverlay *overlay =
      (GstGLCompositionOverlay *) overlay_pointer;
  if (overlay->vao) {
    gl->DeleteVertexArrays (1, &overlay->vao);
    overlay->vao = 0;
  }

  if (overlay->position_buffer) {
    gl->DeleteBuffers (1, &overlay->position_buffer);
    overlay->position_buffer = 0;
  }

  if (overlay->texcoord_buffer) {
    gl->DeleteBuffers (1, &overlay->texcoord_buffer);
    overlay->texcoord_buffer = 0;
  }

  if (overlay->index_buffer) {
    gl->DeleteBuffers (1, &overlay->index_buffer);
    overlay->index_buffer = 0;
  }
}

static void
gst_gl_composition_overlay_bind_vertex_buffer (GstGLCompositionOverlay *
    overlay)
{
  const GstGLFuncs *gl = overlay->context->gl_vtable;
  gl->BindBuffer (GL_ARRAY_BUFFER, overlay->position_buffer);
  gl->VertexAttribPointer (overlay->position_attrib, 4, GL_FLOAT, GL_FALSE,
      4 * sizeof (GLfloat), NULL);

  gl->BindBuffer (GL_ARRAY_BUFFER, overlay->texcoord_buffer);
  gl->VertexAttribPointer (overlay->texcoord_attrib, 2, GL_FLOAT, GL_FALSE,
      2 * sizeof (GLfloat), NULL);

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, overlay->index_buffer);

  gl->EnableVertexAttribArray (overlay->position_attrib);
  gl->EnableVertexAttribArray (overlay->texcoord_attrib);
}

static void
gst_gl_composition_overlay_finalize (GObject * object)
{
  GstGLCompositionOverlay *overlay;

  overlay = GST_GL_COMPOSITION_OVERLAY (object);

  if (overlay->gl_memory)
    gst_memory_unref ((GstMemory *) overlay->gl_memory);

  if (overlay->context) {
    gst_gl_context_thread_add (overlay->context,
        gst_gl_composition_overlay_free_vertex_buffer, overlay);
    gst_object_unref (overlay->context);
  }

  G_OBJECT_CLASS (gst_gl_composition_overlay_parent_class)->finalize (object);
}


static void
gst_gl_composition_overlay_class_init (GstGLCompositionOverlayClass * klass)
{
  G_OBJECT_CLASS (klass)->finalize = gst_gl_composition_overlay_finalize;
}

static void
gst_gl_composition_overlay_init (GstGLCompositionOverlay * overlay)
{
}

static void
gst_gl_composition_overlay_add_transformation (GstGLCompositionOverlay *
    overlay, GstBuffer * video_buffer)
{
  gint comp_x, comp_y;
  guint comp_width, comp_height;
  GstVideoMeta *meta;
  guint width, height;
  gfloat yswap;

  float rel_x, rel_y, rel_w, rel_h;

  meta = gst_buffer_get_video_meta (video_buffer);
  if (!meta) {
    GST_WARNING_OBJECT (overlay, "buffer doesn't contain video meta");
    return;
  }

  gst_video_overlay_rectangle_get_render_rectangle (overlay->rectangle,
      &comp_x, &comp_y, &comp_width, &comp_height);

  width = meta->width;
  height = meta->height;

  /* calculate relative position */
  rel_x = (float) comp_x / (float) width;
  rel_y = (float) comp_y / (float) height;

  rel_w = (float) comp_width / (float) width;
  rel_h = (float) comp_height / (float) height;

  /* transform from [0,1] to [-1,1], invert y axis */
  rel_x = rel_x * 2.0 - 1.0;
  rel_y = (1.0 - rel_y) * 2.0 - 1.0;

  rel_w = rel_w * 2.0;
  rel_h = rel_h * 2.0;

  yswap = overlay->yinvert ? -1. : 1.;

  /* initialize position array */
  overlay->positions[0] = rel_x + rel_w;
  overlay->positions[1] = rel_y * yswap;
  overlay->positions[2] = 0.0;
  overlay->positions[3] = 1.0;
  overlay->positions[4] = rel_x;
  overlay->positions[5] = rel_y * yswap;
  overlay->positions[6] = 0.0;
  overlay->positions[7] = 1.0;
  overlay->positions[8] = rel_x;
  overlay->positions[9] = (rel_y - rel_h) * yswap;
  overlay->positions[10] = 0.0;
  overlay->positions[11] = 1.0;
  overlay->positions[12] = rel_x + rel_w;
  overlay->positions[13] = (rel_y - rel_h) * yswap;
  overlay->positions[14] = 0.0;
  overlay->positions[15] = 1.0;

  gst_gl_context_thread_add (overlay->context,
      gst_gl_composition_overlay_free_vertex_buffer, overlay);

  gst_gl_context_thread_add (overlay->context,
      gst_gl_composition_overlay_init_vertex_buffer, overlay);

  GST_DEBUG
      ("overlay position: (%d,%d) size: %dx%d video size: %dx%d",
      comp_x, comp_y, comp_width, comp_height, meta->width, meta->height);
}

/* helper object API functions */

static GstGLCompositionOverlay *
gst_gl_composition_overlay_new (GstGLContext * context,
    GstVideoOverlayRectangle * rectangle,
    GLint position_attrib, GLint texcoord_attrib)
{
  GstGLCompositionOverlay *overlay =
      g_object_new (GST_TYPE_GL_COMPOSITION_OVERLAY, NULL);

  overlay->gl_memory = NULL;
  overlay->texture_id = -1;
  overlay->rectangle = rectangle;
  overlay->context = gst_object_ref (context);
  overlay->vao = 0;
  overlay->position_attrib = position_attrib;
  overlay->texcoord_attrib = texcoord_attrib;

  GST_DEBUG_OBJECT (overlay, "Created new GstGLCompositionOverlay");

  return overlay;
}

static void
_video_frame_unmap_and_free (gpointer user_data)
{
  GstVideoFrame *frame = user_data;

  gst_video_frame_unmap (frame);
  g_free (frame);
}

static void
gst_gl_composition_overlay_upload (GstGLCompositionOverlay * overlay,
    GstBuffer * buf)
{
  GstGLMemory *comp_gl_memory = NULL;
  GstBuffer *comp_buffer = NULL;
  GstBuffer *overlay_buffer = NULL;
  GstVideoInfo vinfo;
  GstVideoMeta *vmeta;
  GstVideoFrame *comp_frame;
  GstVideoFrame gl_frame;
  GstVideoOverlayFormatFlags flags;
  GstVideoOverlayFormatFlags alpha_flags;

  flags = gst_video_overlay_rectangle_get_flags (overlay->rectangle);

  if (flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA) {
    alpha_flags = GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA;
  } else if (!overlay->context->gl_vtable->BlendFuncSeparate) {
    GST_FIXME_OBJECT (overlay, "No separate blend mode function, "
        "cannot perform correct blending of unmultipled alpha in OpenGL. "
        "Software converting");
    alpha_flags = GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA;
  } else {
    alpha_flags = 0;
  }

  comp_buffer =
      gst_video_overlay_rectangle_get_pixels_unscaled_argb (overlay->rectangle,
      alpha_flags);

  comp_frame = g_new (GstVideoFrame, 1);

  vmeta = gst_buffer_get_video_meta (comp_buffer);
  gst_video_info_set_format (&vinfo, vmeta->format, vmeta->width,
      vmeta->height);
  vinfo.stride[0] = vmeta->stride[0];

  if (gst_video_frame_map (comp_frame, &vinfo, comp_buffer, GST_MAP_READ)) {
    GstGLVideoAllocationParams *params;
    GstGLBaseMemoryAllocator *mem_allocator;
    GstAllocator *allocator;

    allocator =
        GST_ALLOCATOR (gst_gl_memory_allocator_get_default (overlay->context));
    mem_allocator = GST_GL_BASE_MEMORY_ALLOCATOR (allocator);

    gst_gl_composition_overlay_add_transformation (overlay, buf);

    params = gst_gl_video_allocation_params_new_wrapped_data (overlay->context,
        NULL, &comp_frame->info, 0, NULL, GST_GL_TEXTURE_TARGET_2D,
        GST_GL_RGBA, comp_frame->data[0], comp_frame,
        _video_frame_unmap_and_free);

    comp_gl_memory =
        (GstGLMemory *) gst_gl_base_memory_alloc (mem_allocator,
        (GstGLAllocationParams *) params);

    gst_gl_allocation_params_free ((GstGLAllocationParams *) params);
    gst_object_unref (allocator);

    overlay_buffer = gst_buffer_new ();
    gst_buffer_append_memory (overlay_buffer, (GstMemory *) comp_gl_memory);

    if (!gst_video_frame_map (&gl_frame, &comp_frame->info, overlay_buffer,
            GST_MAP_READ | GST_MAP_GL)) {
      gst_buffer_unref (overlay_buffer);
      _video_frame_unmap_and_free (comp_frame);
      GST_WARNING_OBJECT (overlay, "Cannot upload overlay texture");
      return;
    }

    gst_memory_ref ((GstMemory *) comp_gl_memory);
    overlay->gl_memory = comp_gl_memory;
    overlay->texture_id = comp_gl_memory->tex_id;

    gst_buffer_unref (overlay_buffer);
    gst_video_frame_unmap (&gl_frame);

    GST_DEBUG ("uploaded overlay texture %d", overlay->texture_id);
  } else {
    g_free (comp_frame);
  }
}

static void
gst_gl_composition_overlay_draw (GstGLCompositionOverlay * overlay,
    GstGLShader * shader)
{
  const GstGLFuncs *gl = overlay->context->gl_vtable;
  if (gl->GenVertexArrays)
    gl->BindVertexArray (overlay->vao);
  else
    gst_gl_composition_overlay_bind_vertex_buffer (overlay);

  if (overlay->texture_id != -1)
    gl->BindTexture (GL_TEXTURE_2D, overlay->texture_id);
  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

typedef struct
{
  gboolean yinvert;
} GstGLOverlayCompositorPrivate;

enum
{
  PROP_0,
  PROP_YINVERT,
};

/********************************************************************
 * GstGLOverlayCompositor object, the public helper object to render
 * GstVideoCompositionOverlayMeta
 ********************************************************************/

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_overlay_compositor_debug, \
      "gloverlaycompositor", 0, "overlaycompositor");

/* this matches what glimagesink does as this was publicized before being used
 * in other elements that draw in different orientations */
#define DEFAULT_YINVERT                 FALSE

G_DEFINE_TYPE_WITH_CODE (GstGLOverlayCompositor, gst_gl_overlay_compositor,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstGLOverlayCompositor);
    DEBUG_INIT);

static void gst_gl_overlay_compositor_finalize (GObject * object);
static void gst_gl_overlay_compositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_overlay_compositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean _is_rectangle_in_overlays (GList * overlays,
    GstVideoOverlayRectangle * rectangle);
static gboolean _is_overlay_in_rectangles (GstVideoOverlayComposition *
    composition, GstGLCompositionOverlay * overlay);

static void
gst_gl_overlay_compositor_class_init (GstGLOverlayCompositorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_gl_overlay_compositor_finalize;
  gobject_class->set_property = gst_gl_overlay_compositor_set_property;
  gobject_class->get_property = gst_gl_overlay_compositor_get_property;

  g_object_class_install_property (gobject_class, PROP_YINVERT,
      g_param_spec_boolean ("yinvert",
          "Y-Invert",
          "Whether to invert the output across a horizintal axis",
          DEFAULT_YINVERT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_overlay_compositor_init (GstGLOverlayCompositor * compositor)
{
  GstGLOverlayCompositorPrivate *priv =
      gst_gl_overlay_compositor_get_instance_private (compositor);

  priv->yinvert = DEFAULT_YINVERT;
}

static void
gst_gl_overlay_compositor_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLOverlayCompositor *self = GST_GL_OVERLAY_COMPOSITOR (object);
  GstGLOverlayCompositorPrivate *priv =
      gst_gl_overlay_compositor_get_instance_private (self);

  switch (prop_id) {
    case PROP_YINVERT:
      /* XXX: invalidiate all current rectangles on a change */
      priv->yinvert = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_overlay_compositor_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLOverlayCompositor *self = GST_GL_OVERLAY_COMPOSITOR (object);
  GstGLOverlayCompositorPrivate *priv =
      gst_gl_overlay_compositor_get_instance_private (self);

  switch (prop_id) {
    case PROP_YINVERT:
      g_value_set_boolean (value, priv->yinvert);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_overlay_compositor_init_gl (GstGLContext * context,
    gpointer compositor_pointer)
{
  GstGLOverlayCompositor *compositor =
      (GstGLOverlayCompositor *) compositor_pointer;
  GError *error = NULL;
  const gchar *frag_strs[2];

  frag_strs[0] =
      gst_gl_shader_string_get_highest_precision (context,
      GST_GLSL_VERSION_NONE,
      GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);
  frag_strs[1] = fragment_shader;

  if (!(compositor->shader =
          gst_gl_shader_new_link_with_stages (context, &error,
              gst_glsl_stage_new_default_vertex (context),
              gst_glsl_stage_new_with_strings (context, GL_FRAGMENT_SHADER,
                  GST_GLSL_VERSION_NONE,
                  GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY, 2,
                  frag_strs), NULL))) {
    GST_ERROR_OBJECT (compositor, "could not initialize shader: %s",
        error->message);
    return;
  }

  compositor->position_attrib =
      gst_gl_shader_get_attribute_location (compositor->shader, "a_position");
  compositor->texcoord_attrib =
      gst_gl_shader_get_attribute_location (compositor->shader, "a_texcoord");
}

GstGLOverlayCompositor *
gst_gl_overlay_compositor_new (GstGLContext * context)
{
  GstGLOverlayCompositor *compositor =
      g_object_new (GST_TYPE_GL_OVERLAY_COMPOSITOR, NULL);

  gst_object_ref_sink (compositor);

  compositor->context = gst_object_ref (context);

  gst_gl_context_thread_add (compositor->context,
      gst_gl_overlay_compositor_init_gl, compositor);

  GST_DEBUG_OBJECT (compositor, "Created new GstGLOverlayCompositor");

  return compositor;
}

static void
gst_gl_overlay_compositor_finalize (GObject * object)
{
  GstGLOverlayCompositor *compositor;

  compositor = GST_GL_OVERLAY_COMPOSITOR (object);

  gst_gl_overlay_compositor_free_overlays (compositor);

  if (compositor->context)
    gst_object_unref (compositor->context);

  if (compositor->shader) {
    gst_object_unref (compositor->shader);
    compositor->shader = NULL;
  }

  G_OBJECT_CLASS (gst_gl_overlay_compositor_parent_class)->finalize (object);
}

static gboolean
_is_rectangle_in_overlays (GList * overlays,
    GstVideoOverlayRectangle * rectangle)
{
  GList *l;

  for (l = overlays; l != NULL; l = l->next) {
    GstGLCompositionOverlay *overlay = (GstGLCompositionOverlay *) l->data;
    if (overlay->rectangle == rectangle)
      return TRUE;
  }
  return FALSE;
}

static gboolean
_is_overlay_in_rectangles (GstVideoOverlayComposition * composition,
    GstGLCompositionOverlay * overlay)
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

void
gst_gl_overlay_compositor_free_overlays (GstGLOverlayCompositor * compositor)
{
  GList *l = compositor->overlays;
  while (l != NULL) {
    GList *next = l->next;
    GstGLCompositionOverlay *overlay = (GstGLCompositionOverlay *) l->data;
    compositor->overlays = g_list_delete_link (compositor->overlays, l);
    gst_object_unref (overlay);
    l = next;
  }
  g_list_free (compositor->overlays);
  compositor->overlays = NULL;
}

void
gst_gl_overlay_compositor_upload_overlays (GstGLOverlayCompositor * compositor,
    GstBuffer * buf)
{
  GstVideoOverlayCompositionMeta *composition_meta;
  GstGLOverlayCompositorPrivate *priv =
      gst_gl_overlay_compositor_get_instance_private (compositor);

  composition_meta = gst_buffer_get_video_overlay_composition_meta (buf);
  if (composition_meta) {
    GstVideoOverlayComposition *composition = NULL;
    guint num_overlays, i;
    GList *l = compositor->overlays;
    GstGLSyncMeta *sync_meta;

    GST_DEBUG ("GstVideoOverlayCompositionMeta found.");

    composition = composition_meta->overlay;
    num_overlays = gst_video_overlay_composition_n_rectangles (composition);

    /* add new overlays to list */
    for (i = 0; i < num_overlays; i++) {
      GstVideoOverlayRectangle *rectangle =
          gst_video_overlay_composition_get_rectangle (composition, i);

      if (!_is_rectangle_in_overlays (compositor->overlays, rectangle)) {
        GstGLCompositionOverlay *overlay =
            gst_gl_composition_overlay_new (compositor->context, rectangle,
            compositor->position_attrib, compositor->texcoord_attrib);
        gst_object_ref_sink (overlay);
        overlay->yinvert = priv->yinvert;

        gst_gl_composition_overlay_upload (overlay, buf);

        compositor->overlays = g_list_append (compositor->overlays, overlay);
      }
    }

    sync_meta = gst_buffer_get_gl_sync_meta (buf);
    if (sync_meta) {
      gst_gl_sync_meta_set_sync_point (sync_meta, compositor->context);
    }

    /* remove old overlays from list */
    while (l != NULL) {
      GList *next = l->next;
      GstGLCompositionOverlay *overlay = (GstGLCompositionOverlay *) l->data;
      if (!_is_overlay_in_rectangles (composition, overlay)) {
        compositor->overlays = g_list_delete_link (compositor->overlays, l);
        gst_object_unref (overlay);
      }
      l = next;
    }
  } else {
    gst_gl_overlay_compositor_free_overlays (compositor);
  }
}

void
gst_gl_overlay_compositor_draw_overlays (GstGLOverlayCompositor * compositor)
{
  const GstGLFuncs *gl = compositor->context->gl_vtable;
  if (compositor->overlays != NULL) {
    GList *l;

    gl->Enable (GL_BLEND);

    gst_gl_shader_use (compositor->shader);
    gl->ActiveTexture (GL_TEXTURE0);
    gst_gl_shader_set_uniform_1i (compositor->shader, "tex", 0);

    for (l = compositor->overlays; l != NULL; l = l->next) {
      GstGLCompositionOverlay *overlay = (GstGLCompositionOverlay *) l->data;
      GstVideoOverlayFormatFlags flags;

      flags = gst_video_overlay_rectangle_get_flags (overlay->rectangle);

      if (flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA
          || !gl->BlendFuncSeparate) {
        gl->BlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      } else {
        gl->BlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
            GL_ONE_MINUS_SRC_ALPHA);
      }
      gst_gl_composition_overlay_draw (overlay, compositor->shader);
    }

    gl->BindTexture (GL_TEXTURE_2D, 0);
    gl->Disable (GL_BLEND);
  }
}

GstCaps *
gst_gl_overlay_compositor_add_caps (GstCaps * caps)
{
  GstCaps *composition_caps;
  int i;

  composition_caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (composition_caps); i++) {
    GstCapsFeatures *f = gst_caps_get_features (composition_caps, i);
    if (!gst_caps_features_is_any (f))
      gst_caps_features_add (f,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  }

  caps = gst_caps_merge (composition_caps, caps);

  return caps;
}
