/*
 * GStreamer
 * Copyright (C) 2014 Lubosz Sarnecki <lubosz@gmail.com>
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-gltransformation
 * @title: gltransformation
 *
 * Transforms video on the GPU.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 gltestsrc ! gltransformation rotation-z=45 ! glimagesink
 * ]| A pipeline to rotate by 45 degrees
 * |[
 * gst-launch-1.0 gltestsrc ! gltransformation translation-x=0.5 ! glimagesink
 * ]| Translate the video by 0.5
 * |[
 * gst-launch-1.0 gltestsrc ! gltransformation scale-y=0.5 scale-x=0.5 ! glimagesink
 * ]| Resize the video by 0.5
 * |[
 * gst-launch-1.0 gltestsrc ! gltransformation rotation-x=-45 ortho=True ! glimagesink
 * ]| Rotate the video around the X-Axis by -45° with an orthographic projection
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstgltransformation.h"

#include <gst/gl/gstglapi.h>
#include <graphene-gobject.h>
#include "gstglutils.h"

#define GST_CAT_DEFAULT gst_gl_transformation_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define gst_gl_transformation_parent_class parent_class

#define VEC4_FORMAT "%f,%f,%f,%f"
#define VEC4_ARGS(v) graphene_vec4_get_x (v), graphene_vec4_get_y (v), graphene_vec4_get_z (v), graphene_vec4_get_w (v)
#define VEC3_FORMAT "%f,%f,%f"
#define VEC3_ARGS(v) graphene_vec3_get_x (v), graphene_vec3_get_y (v), graphene_vec3_get_z (v)
#define VEC2_FORMAT "%f,%f"
#define VEC2_ARGS(v) graphene_vec2_get_x (v), graphene_vec2_get_y (v)
#define POINT3D_FORMAT "%f,%f,%f"
#define POINT3D_ARGS(p) (p)->x, (p)->y, (p)->z

enum
{
  PROP_0,
  PROP_FOV,
  PROP_ORTHO,
  PROP_TRANSLATION_X,
  PROP_TRANSLATION_Y,
  PROP_TRANSLATION_Z,
  PROP_ROTATION_X,
  PROP_ROTATION_Y,
  PROP_ROTATION_Z,
  PROP_SCALE_X,
  PROP_SCALE_Y,
  PROP_MVP,
  PROP_PIVOT_X,
  PROP_PIVOT_Y,
  PROP_PIVOT_Z,
};

#define DEBUG_INIT \
    GST_DEBUG_CATEGORY_INIT (gst_gl_transformation_debug, "gltransformation", 0, "gltransformation element");

G_DEFINE_TYPE_WITH_CODE (GstGLTransformation, gst_gl_transformation,
    GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gltransformation, "gltransformation",
    GST_RANK_NONE, GST_TYPE_GL_TRANSFORMATION, gl_element_init (plugin));

static void gst_gl_transformation_finalize (GObject * object);
static void gst_gl_transformation_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_transformation_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_transformation_set_caps (GstGLFilter * filter,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_gl_transformation_src_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_gl_transformation_filter_meta (GstBaseTransform * trans,
    GstQuery * query, GType api, const GstStructure * params);
static gboolean gst_gl_transformation_decide_allocation (GstBaseTransform *
    trans, GstQuery * query);

static void gst_gl_transformation_gl_stop (GstGLBaseFilter * filter);
static gboolean gst_gl_transformation_gl_start (GstGLBaseFilter * base_filter);
static gboolean gst_gl_transformation_callback (gpointer stuff);
static void gst_gl_transformation_build_mvp (GstGLTransformation *
    transformation);

static GstFlowReturn
gst_gl_transformation_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf);
static gboolean gst_gl_transformation_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_gl_transformation_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex);

static void
gst_gl_transformation_class_init (GstGLTransformationClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *base_transform_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_gl_filter_add_rgba_pad_templates (GST_GL_FILTER_CLASS (klass));

  gobject_class->set_property = gst_gl_transformation_set_property;
  gobject_class->get_property = gst_gl_transformation_get_property;

  base_transform_class->src_event = gst_gl_transformation_src_event;
  base_transform_class->decide_allocation =
      gst_gl_transformation_decide_allocation;
  base_transform_class->filter_meta = gst_gl_transformation_filter_meta;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start = gst_gl_transformation_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop = gst_gl_transformation_gl_stop;

  GST_GL_FILTER_CLASS (klass)->set_caps = gst_gl_transformation_set_caps;
  GST_GL_FILTER_CLASS (klass)->filter = gst_gl_transformation_filter;
  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_transformation_filter_texture;
  GST_BASE_TRANSFORM_CLASS (klass)->prepare_output_buffer =
      gst_gl_transformation_prepare_output_buffer;

  g_object_class_install_property (gobject_class, PROP_FOV,
      g_param_spec_float ("fov", "Fov", "Field of view angle in degrees",
          0.0, G_MAXFLOAT, 90.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ORTHO,
      g_param_spec_boolean ("ortho", "Orthographic",
          "Use orthographic projection", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Rotation */
  g_object_class_install_property (gobject_class, PROP_ROTATION_X,
      g_param_spec_float ("rotation-x", "X Rotation",
          "Rotates the video around the X-Axis in degrees.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ROTATION_Y,
      g_param_spec_float ("rotation-y", "Y Rotation",
          "Rotates the video around the Y-Axis in degrees.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ROTATION_Z,
      g_param_spec_float ("rotation-z", "Z Rotation",
          "Rotates the video around the Z-Axis in degrees.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Translation */
  g_object_class_install_property (gobject_class, PROP_TRANSLATION_X,
      g_param_spec_float ("translation-x", "X Translation",
          "Translates the video at the X-Axis, in universal [0-1] coordinate.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TRANSLATION_Y,
      g_param_spec_float ("translation-y", "Y Translation",
          "Translates the video at the Y-Axis, in universal [0-1] coordinate.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TRANSLATION_Z,
      g_param_spec_float ("translation-z", "Z Translation",
          "Translates the video at the Z-Axis, in universal [0-1] coordinate.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Scale */
  g_object_class_install_property (gobject_class, PROP_SCALE_X,
      g_param_spec_float ("scale-x", "X Scale",
          "Scale multiplier for the X-Axis.",
          -G_MAXFLOAT, G_MAXFLOAT, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SCALE_Y,
      g_param_spec_float ("scale-y", "Y Scale",
          "Scale multiplier for the Y-Axis.",
          -G_MAXFLOAT, G_MAXFLOAT, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Pivot */
  g_object_class_install_property (gobject_class, PROP_PIVOT_X,
      g_param_spec_float ("pivot-x", "X Pivot",
          "Rotation pivot point X coordinate, where 0 is the center,"
          " -1 the left border, +1 the right border and <-1, >1 outside.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIVOT_Y,
      g_param_spec_float ("pivot-y", "Y Pivot",
          "Rotation pivot point X coordinate, where 0 is the center,"
          " -1 the left border, +1 the right border and <-1, >1 outside.",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIVOT_Z,
      g_param_spec_float ("pivot-z", "Z Pivot",
          "Relevant for rotation in 3D space. You look into the negative Z axis direction",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* MVP */
  g_object_class_install_property (gobject_class, PROP_MVP,
      g_param_spec_boxed ("mvp-matrix",
          "Modelview Projection Matrix",
          "The final Graphene 4x4 Matrix for transformation",
          GRAPHENE_TYPE_MATRIX, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (element_class, "OpenGL transformation filter",
      "Filter/Effect/Video", "Transform video on the GPU",
      "Lubosz Sarnecki <lubosz@gmail.com>, "
      "Matthew Waters <matthew@centricular.com>");

  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;

  gobject_class->finalize = gst_gl_transformation_finalize;
}

static void
gst_gl_transformation_init (GstGLTransformation * filter)
{
  filter->shader = NULL;
  filter->fov = 90;
  filter->aspect = 1.0;
  filter->znear = 0.1f;
  filter->zfar = 100.0;

  filter->xscale = 1.0;
  filter->yscale = 1.0;

  filter->in_tex = 0;

  /* let graphene alloc its structures with correct memory alignment, otherwise
   * unaligned memory access faults can occur */
  filter->model_matrix = graphene_matrix_alloc ();
  filter->view_matrix = graphene_matrix_alloc ();
  filter->projection_matrix = graphene_matrix_alloc ();
  filter->inv_model_matrix = graphene_matrix_alloc ();
  filter->inv_view_matrix = graphene_matrix_alloc ();
  filter->inv_projection_matrix = graphene_matrix_alloc ();
  filter->mvp_matrix = graphene_matrix_alloc ();

  filter->camera_position = graphene_vec3_alloc ();

  gst_gl_transformation_build_mvp (filter);
}

static void
gst_gl_transformation_finalize (GObject * object)
{
  GstGLTransformation *transformation;

  g_return_if_fail (GST_IS_GL_TRANSFORMATION (object));

  transformation = GST_GL_TRANSFORMATION (object);

  graphene_matrix_free (transformation->model_matrix);
  graphene_matrix_free (transformation->view_matrix);
  graphene_matrix_free (transformation->projection_matrix);
  graphene_matrix_free (transformation->inv_model_matrix);
  graphene_matrix_free (transformation->inv_view_matrix);
  graphene_matrix_free (transformation->inv_projection_matrix);
  graphene_matrix_free (transformation->mvp_matrix);

  graphene_vec3_free (transformation->camera_position);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_transformation_build_mvp (GstGLTransformation * transformation)
{
  GstGLFilter *filter = GST_GL_FILTER (transformation);
  graphene_matrix_t modelview_matrix;

  if (!filter->out_info.finfo) {
    graphene_matrix_init_identity (transformation->model_matrix);
    graphene_matrix_init_identity (transformation->view_matrix);
    graphene_matrix_init_identity (transformation->projection_matrix);
  } else {
    graphene_point3d_t translation_vector =
        GRAPHENE_POINT3D_INIT (transformation->xtranslation * 2.0 *
        transformation->aspect,
        transformation->ytranslation * 2.0,
        transformation->ztranslation * 2.0);

    graphene_point3d_t pivot_vector =
        GRAPHENE_POINT3D_INIT (-transformation->xpivot * transformation->aspect,
        transformation->ypivot,
        -transformation->zpivot);

    graphene_point3d_t negative_pivot_vector;

    graphene_vec3_t center;
    graphene_vec3_t up;

    gboolean current_passthrough;
    gboolean passthrough;

    graphene_vec3_init (transformation->camera_position, 0.f, 0.f, 1.f);
    graphene_vec3_init (&center, 0.f, 0.f, 0.f);
    graphene_vec3_init (&up, 0.f, 1.f, 0.f);

    /* Translate into pivot origin */
    graphene_matrix_init_translate (transformation->model_matrix,
        &pivot_vector);

    /* Scale */
    graphene_matrix_scale (transformation->model_matrix,
        transformation->xscale, transformation->yscale, 1.0f);

    /* Rotation */
    graphene_matrix_rotate (transformation->model_matrix,
        transformation->xrotation, graphene_vec3_x_axis ());
    graphene_matrix_rotate (transformation->model_matrix,
        transformation->yrotation, graphene_vec3_y_axis ());
    graphene_matrix_rotate (transformation->model_matrix,
        transformation->zrotation, graphene_vec3_z_axis ());

    /* Translate back from pivot origin */
    graphene_point3d_scale (&pivot_vector, -1.0, &negative_pivot_vector);
    graphene_matrix_translate (transformation->model_matrix,
        &negative_pivot_vector);

    /* Translation */
    graphene_matrix_translate (transformation->model_matrix,
        &translation_vector);

    if (transformation->ortho) {
      graphene_matrix_init_ortho (transformation->projection_matrix,
          -transformation->aspect, transformation->aspect,
          -1, 1, transformation->znear, transformation->zfar);
    } else {
      graphene_matrix_init_perspective (transformation->projection_matrix,
          transformation->fov,
          transformation->aspect, transformation->znear, transformation->zfar);
    }

    graphene_matrix_init_look_at (transformation->view_matrix,
        transformation->camera_position, &center, &up);

    current_passthrough =
        gst_base_transform_is_passthrough (GST_BASE_TRANSFORM (transformation));
    passthrough = transformation->xtranslation == 0.
        && transformation->ytranslation == 0.
        && transformation->ztranslation == 0. && transformation->xrotation == 0.
        && transformation->yrotation == 0. && transformation->zrotation == 0.
        && transformation->xscale == 1. && transformation->yscale == 1.
        && gst_video_info_is_equal (&filter->in_info, &filter->out_info);
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (transformation),
        passthrough);
    if (current_passthrough != passthrough) {
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (transformation));
    }
  }

  graphene_matrix_multiply (transformation->model_matrix,
      transformation->view_matrix, &modelview_matrix);
  graphene_matrix_multiply (&modelview_matrix,
      transformation->projection_matrix, transformation->mvp_matrix);

  graphene_matrix_inverse (transformation->model_matrix,
      transformation->inv_model_matrix);
  graphene_matrix_inverse (transformation->view_matrix,
      transformation->inv_view_matrix);
  graphene_matrix_inverse (transformation->projection_matrix,
      transformation->inv_projection_matrix);
}

static void
gst_gl_transformation_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLTransformation *filter = GST_GL_TRANSFORMATION (object);

  switch (prop_id) {
    case PROP_FOV:
      filter->fov = g_value_get_float (value);
      break;
    case PROP_ORTHO:
      filter->ortho = g_value_get_boolean (value);
      break;
    case PROP_TRANSLATION_X:
      filter->xtranslation = g_value_get_float (value);
      break;
    case PROP_TRANSLATION_Y:
      filter->ytranslation = g_value_get_float (value);
      break;
    case PROP_TRANSLATION_Z:
      filter->ztranslation = g_value_get_float (value);
      break;
    case PROP_ROTATION_X:
      filter->xrotation = g_value_get_float (value);
      break;
    case PROP_ROTATION_Y:
      filter->yrotation = g_value_get_float (value);
      break;
    case PROP_ROTATION_Z:
      filter->zrotation = g_value_get_float (value);
      break;
    case PROP_SCALE_X:
      filter->xscale = g_value_get_float (value);
      break;
    case PROP_SCALE_Y:
      filter->yscale = g_value_get_float (value);
      break;
    case PROP_PIVOT_X:
      filter->xpivot = g_value_get_float (value);
      break;
    case PROP_PIVOT_Y:
      filter->ypivot = g_value_get_float (value);
      break;
    case PROP_PIVOT_Z:
      filter->zpivot = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  gst_gl_transformation_build_mvp (filter);
}

static void
gst_gl_transformation_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLTransformation *filter = GST_GL_TRANSFORMATION (object);

  switch (prop_id) {
    case PROP_FOV:
      g_value_set_float (value, filter->fov);
      break;
    case PROP_ORTHO:
      g_value_set_boolean (value, filter->ortho);
      break;
    case PROP_TRANSLATION_X:
      g_value_set_float (value, filter->xtranslation);
      break;
    case PROP_TRANSLATION_Y:
      g_value_set_float (value, filter->ytranslation);
      break;
    case PROP_TRANSLATION_Z:
      g_value_set_float (value, filter->ztranslation);
      break;
    case PROP_ROTATION_X:
      g_value_set_float (value, filter->xrotation);
      break;
    case PROP_ROTATION_Y:
      g_value_set_float (value, filter->yrotation);
      break;
    case PROP_ROTATION_Z:
      g_value_set_float (value, filter->zrotation);
      break;
    case PROP_SCALE_X:
      g_value_set_float (value, filter->xscale);
      break;
    case PROP_SCALE_Y:
      g_value_set_float (value, filter->yscale);
      break;
    case PROP_PIVOT_X:
      g_value_set_float (value, filter->xpivot);
      break;
    case PROP_PIVOT_Y:
      g_value_set_float (value, filter->ypivot);
      break;
    case PROP_PIVOT_Z:
      g_value_set_float (value, filter->zpivot);
      break;
    case PROP_MVP:
      /* FIXME: need to decompose this to support navigation events */
      g_value_set_boxed (value, (gconstpointer) filter->mvp_matrix);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_transformation_set_caps (GstGLFilter * filter, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (filter);

  transformation->aspect =
      (gdouble) GST_VIDEO_INFO_WIDTH (&filter->out_info) /
      (gdouble) GST_VIDEO_INFO_HEIGHT (&filter->out_info);

  transformation->caps_change = TRUE;

  gst_gl_transformation_build_mvp (transformation);

  return TRUE;
}

static void
_intersect_plane_and_ray (graphene_plane_t * video_plane, graphene_ray_t * ray,
    graphene_point3d_t * result)
{
  float t = graphene_ray_get_distance_to_plane (ray, video_plane);
  GST_TRACE ("Calculated a distance of %f to the plane", t);
  graphene_ray_get_position_at (ray, t, result);
}

static void
_screen_coord_to_world_ray (GstGLTransformation * transformation, float x,
    float y, graphene_ray_t * ray)
{
  GstGLFilter *filter = GST_GL_FILTER (transformation);
  gfloat w = (gfloat) GST_VIDEO_INFO_WIDTH (&filter->in_info);
  gfloat h = (gfloat) GST_VIDEO_INFO_HEIGHT (&filter->in_info);
  graphene_vec3_t ray_eye_vec3, ray_world_dir, *ray_origin, *ray_direction;
  graphene_vec3_t ray_ortho_dir;
  graphene_point3d_t ray_clip, ray_eye;
  graphene_vec2_t screen_coord;

  /* GL is y-flipped. i.e. 0, 0 is the bottom left corner in screen space */
  graphene_vec2_init (&screen_coord, (2. * x / w - 1.) / transformation->aspect,
      1. - 2. * y / h);

  graphene_point3d_init (&ray_clip, graphene_vec2_get_x (&screen_coord),
      graphene_vec2_get_y (&screen_coord), -1.);
  graphene_matrix_transform_point3d (transformation->inv_projection_matrix,
      &ray_clip, &ray_eye);

  graphene_vec3_init (&ray_eye_vec3, ray_eye.x, ray_eye.y, -1.);

  if (transformation->ortho) {
    graphene_vec3_init (&ray_ortho_dir, 0., 0., 1.);

    ray_origin = &ray_eye_vec3;
    ray_direction = &ray_ortho_dir;
  } else {
    graphene_matrix_transform_vec3 (transformation->inv_view_matrix,
        &ray_eye_vec3, &ray_world_dir);
    graphene_vec3_normalize (&ray_world_dir, &ray_world_dir);

    ray_origin = transformation->camera_position;
    ray_direction = &ray_world_dir;
  }

  graphene_ray_init_from_vec3 (ray, ray_origin, ray_direction);

  GST_TRACE_OBJECT (transformation, "Calculated ray origin: " VEC3_FORMAT
      " direction: " VEC3_FORMAT " from screen coordinates: " VEC2_FORMAT
      " with %s projection",
      VEC3_ARGS (ray_origin), VEC3_ARGS (ray_direction),
      VEC2_ARGS (&screen_coord),
      transformation->ortho ? "ortho" : "perspection");
}

static void
_init_world_video_plane (GstGLTransformation * transformation,
    graphene_plane_t * video_plane)
{
  graphene_point3d_t bottom_left, bottom_right, top_left, top_right;
  graphene_point3d_t world_bottom_left, world_bottom_right;
  graphene_point3d_t world_top_left, world_top_right;

  graphene_point3d_init (&top_left, -transformation->aspect, 1., 0.);
  graphene_point3d_init (&top_right, transformation->aspect, 1., 0.);
  graphene_point3d_init (&bottom_left, -transformation->aspect, -1., 0.);
  graphene_point3d_init (&bottom_right, transformation->aspect, -1., 0.);

  graphene_matrix_transform_point3d (transformation->model_matrix,
      &bottom_left, &world_bottom_left);
  graphene_matrix_transform_point3d (transformation->model_matrix,
      &bottom_right, &world_bottom_right);
  graphene_matrix_transform_point3d (transformation->model_matrix,
      &top_left, &world_top_left);
  graphene_matrix_transform_point3d (transformation->model_matrix,
      &top_right, &world_top_right);

  graphene_plane_init_from_points (video_plane, &world_bottom_left,
      &world_top_right, &world_top_left);
}

static gboolean
_screen_coord_to_model_coord (GstGLTransformation * transformation,
    double x, double y, double *res_x, double *res_y)
{
  GstGLFilter *filter = GST_GL_FILTER (transformation);
  double w = (double) GST_VIDEO_INFO_WIDTH (&filter->in_info);
  double h = (double) GST_VIDEO_INFO_HEIGHT (&filter->in_info);
  graphene_point3d_t world_point, model_coord;
  graphene_plane_t video_plane;
  graphene_ray_t ray;
  double new_x, new_y;

  _init_world_video_plane (transformation, &video_plane);
  _screen_coord_to_world_ray (transformation, x, y, &ray);
  _intersect_plane_and_ray (&video_plane, &ray, &world_point);
  graphene_matrix_transform_point3d (transformation->inv_model_matrix,
      &world_point, &model_coord);

  /* ndc to pixels.  We render the frame Y-flipped so need to unflip the
   * y coordinate */
  new_x = (model_coord.x + 1.) * w / 2;
  new_y = (1. - model_coord.y) * h / 2;

  if (new_x < 0. || new_x > w || new_y < 0. || new_y > h)
    /* coords off video surface */
    return FALSE;

  GST_DEBUG_OBJECT (transformation, "converted %f,%f to %f,%f", x, y, new_x,
      new_y);

  if (res_x)
    *res_x = new_x;
  if (res_y)
    *res_y = new_y;

  return TRUE;
}

#if 0
/* debugging facilities for transforming vertices from model space to screen
 * space */
static void
_ndc_to_viewport (GstGLTransformation * transformation, graphene_vec3_t * ndc,
    int x, int y, int w, int h, float near, float far, graphene_vec3_t * result)
{
  GstGLFilter *filter = GST_GL_FILTER (transformation);
  /* center of the viewport */
  int o_x = x + w / 2;
  int o_y = y + h / 2;

  graphene_vec3_init (result, graphene_vec3_get_x (ndc) * w / 2 + o_x,
      graphene_vec3_get_y (ndc) * h / 2 + o_y,
      (far - near) * graphene_vec3_get_z (ndc) / 2 + (far + near) / 2);
}

static void
_perspective_division (graphene_vec4_t * clip, graphene_vec3_t * result)
{
  float w = graphene_vec4_get_w (clip);

  graphene_vec3_init (result, graphene_vec4_get_x (clip) / w,
      graphene_vec4_get_y (clip) / w, graphene_vec4_get_z (clip) / w);
}

static void
_vertex_to_screen_coord (GstGLTransformation * transformation,
    graphene_vec4_t * vertex, graphene_vec3_t * view)
{
  GstGLFilter *filter = GST_GL_FILTER (transformation);
  gint w = GST_VIDEO_INFO_WIDTH (&filter->in_info);
  gint h = GST_VIDEO_INFO_HEIGHT (&filter->in_info);
  graphene_vec4_t clip;
  graphene_vec3_t ndc;

  graphene_matrix_transform_vec4 (transformation->mvp_matrix, vertex, &clip);
  _perspective_division (&clip, &ndc);
  _ndc_to_viewport (transformation, &ndc, 0, 0, w, h, 0., 1., view);
}
#endif

static gboolean
gst_gl_transformation_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (trans);
  gboolean ret;

  GST_DEBUG_OBJECT (trans, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:{
      gdouble x, y;
      event = gst_event_make_writable (event);

      if (gst_navigation_event_get_coordinates (event, &x, &y)) {
        gdouble new_x, new_y;

        if (!_screen_coord_to_model_coord (transformation, x, y, &new_x,
                &new_y)) {
          gst_event_unref (event);
          return TRUE;
        }

        gst_navigation_event_set_coordinates (event, x, y);
      }
      break;
    }
    default:
      break;
  }

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->src_event (trans, event);

  return ret;
}

static gboolean
gst_gl_transformation_filter_meta (GstBaseTransform * trans, GstQuery * query,
    GType api, const GstStructure * params)
{
  if (api == GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE)
    return TRUE;

  if (api == GST_GL_SYNC_META_API_TYPE)
    return TRUE;

  return FALSE;
}

static gboolean
gst_gl_transformation_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (trans);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
          query))
    return FALSE;

  if (gst_query_find_allocation_meta (query,
          GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE, NULL)) {
    transformation->downstream_supports_affine_meta = TRUE;
  } else {
    transformation->downstream_supports_affine_meta = FALSE;
  }

  return TRUE;
}

static void
gst_gl_transformation_gl_stop (GstGLBaseFilter * base_filter)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (base_filter);
  const GstGLFuncs *gl = base_filter->context->gl_vtable;

  if (transformation->vao) {
    gl->DeleteVertexArrays (1, &transformation->vao);
    transformation->vao = 0;
  }

  if (transformation->vertex_buffer) {
    gl->DeleteBuffers (1, &transformation->vertex_buffer);
    transformation->vertex_buffer = 0;
  }

  if (transformation->vbo_indices) {
    gl->DeleteBuffers (1, &transformation->vbo_indices);
    transformation->vbo_indices = 0;
  }

  if (transformation->shader) {
    gst_object_unref (transformation->shader);
    transformation->shader = NULL;
  }

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base_filter);
}

static gboolean
gst_gl_transformation_gl_start (GstGLBaseFilter * base_filter)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (base_filter);

  if (!GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base_filter))
    return FALSE;

  if (gst_gl_context_get_gl_api (base_filter->context)) {
    gchar *frag_str;
    gboolean ret;

    frag_str =
        gst_gl_shader_string_fragment_get_default (base_filter->context,
        GST_GLSL_VERSION_NONE,
        GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY);

    /* blocking call, wait until the opengl thread has compiled the shader */
    ret = gst_gl_context_gen_shader (base_filter->context,
        gst_gl_shader_string_vertex_mat4_vertex_transform,
        frag_str, &transformation->shader);

    g_free (frag_str);

    return ret;
  }
  return TRUE;
}

static GstFlowReturn
gst_gl_transformation_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (trans);
  GstGLFilter *filter = GST_GL_FILTER (trans);

  if (transformation->downstream_supports_affine_meta &&
      gst_video_info_is_equal (&filter->in_info, &filter->out_info)) {
    GstVideoAffineTransformationMeta *af_meta;
    graphene_matrix_t upstream_matrix, tmp, tmp2, inv_aspect, yflip;
    float upstream[16], downstream[16];

    *outbuf = gst_buffer_make_writable (inbuf);

    af_meta = gst_buffer_get_video_affine_transformation_meta (inbuf);
    if (!af_meta)
      af_meta = gst_buffer_add_video_affine_transformation_meta (*outbuf);

    GST_LOG_OBJECT (trans, "applying transformation to existing affine "
        "transformation meta");

    gst_gl_get_affine_transformation_meta_as_ndc (af_meta, upstream);

    /* apply the transformation to the existing affine meta */
    graphene_matrix_init_from_float (&upstream_matrix, upstream);
    graphene_matrix_init_scale (&inv_aspect, transformation->aspect, -1., 1.);
    graphene_matrix_init_scale (&yflip, 1., -1., 1.);

    /* invert the aspect effects */
    graphene_matrix_multiply (&upstream_matrix, &inv_aspect, &tmp2);
    /* apply the transformation */
    graphene_matrix_multiply (&tmp2, transformation->mvp_matrix, &tmp);
    /* and undo yflip */
    graphene_matrix_multiply (&tmp, &yflip, &tmp2);

    graphene_matrix_to_float (&tmp2, downstream);
    gst_gl_set_affine_transformation_meta_from_ndc (af_meta, downstream);

    return GST_FLOW_OK;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      inbuf, outbuf);
}

static gboolean
gst_gl_transformation_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (filter);

  if (transformation->downstream_supports_affine_meta &&
      gst_video_info_is_equal (&filter->in_info, &filter->out_info)) {
    return TRUE;
  } else {
    return gst_gl_filter_filter_texture (filter, inbuf, outbuf);
  }
}

static gboolean
gst_gl_transformation_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex)
{
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (filter);

  transformation->in_tex = in_tex;

  gst_gl_framebuffer_draw_to_texture (filter->fbo, out_tex,
      (GstGLFramebufferFunc) gst_gl_transformation_callback, transformation);

  return TRUE;
}

static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

static void
_upload_vertices (GstGLTransformation * transformation)
{
  const GstGLFuncs *gl =
      GST_GL_BASE_FILTER (transformation)->context->gl_vtable;

/* *INDENT-OFF* */
  GLfloat vertices[] = {
     -transformation->aspect, -1.0,  0.0, 1.0, 0.0, 0.0,
      transformation->aspect, -1.0,  0.0, 1.0, 1.0, 0.0,
      transformation->aspect,  1.0,  0.0, 1.0, 1.0, 1.0,
     -transformation->aspect,  1.0,  0.0, 1.0, 0.0, 1.0,
  };
  /* *INDENT-ON* */

  gl->BindBuffer (GL_ARRAY_BUFFER, transformation->vertex_buffer);

  gl->BufferData (GL_ARRAY_BUFFER, 4 * 6 * sizeof (GLfloat), vertices,
      GL_STATIC_DRAW);
}

static void
_bind_buffer (GstGLTransformation * transformation)
{
  const GstGLFuncs *gl =
      GST_GL_BASE_FILTER (transformation)->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, transformation->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, transformation->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (transformation->attr_position, 4, GL_FLOAT,
      GL_FALSE, 6 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (transformation->attr_texture, 2, GL_FLOAT, GL_FALSE,
      6 * sizeof (GLfloat), (void *) (4 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (transformation->attr_position);
  gl->EnableVertexAttribArray (transformation->attr_texture);
}

static void
_unbind_buffer (GstGLTransformation * transformation)
{
  const GstGLFuncs *gl =
      GST_GL_BASE_FILTER (transformation)->context->gl_vtable;

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (transformation->attr_position);
  gl->DisableVertexAttribArray (transformation->attr_texture);
}

static gboolean
gst_gl_transformation_callback (gpointer stuff)
{
  GstGLFilter *filter = GST_GL_FILTER (stuff);
  GstGLTransformation *transformation = GST_GL_TRANSFORMATION (filter);
  GstGLFuncs *gl = GST_GL_BASE_FILTER (filter)->context->gl_vtable;

  GLfloat temp_matrix[16];

  gst_gl_context_clear_shader (GST_GL_BASE_FILTER (filter)->context);
  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->ClearColor (0.f, 0.f, 0.f, 0.f);
  gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gst_gl_shader_use (transformation->shader);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, transformation->in_tex->tex_id);
  gst_gl_shader_set_uniform_1i (transformation->shader, "texture", 0);

  graphene_matrix_to_float (transformation->mvp_matrix, temp_matrix);
  gst_gl_shader_set_uniform_matrix_4fv (transformation->shader,
      "u_transformation", 1, GL_FALSE, temp_matrix);

  if (!transformation->vertex_buffer) {
    transformation->attr_position =
        gst_gl_shader_get_attribute_location (transformation->shader,
        "a_position");

    transformation->attr_texture =
        gst_gl_shader_get_attribute_location (transformation->shader,
        "a_texcoord");

    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &transformation->vao);
      gl->BindVertexArray (transformation->vao);
    }

    gl->GenBuffers (1, &transformation->vertex_buffer);

    gl->GenBuffers (1, &transformation->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, transformation->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);

    transformation->caps_change = TRUE;
  }

  if (gl->GenVertexArrays)
    gl->BindVertexArray (transformation->vao);

  if (transformation->caps_change)
    _upload_vertices (transformation);
  _bind_buffer (transformation);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  else
    _unbind_buffer (transformation);

  gst_gl_context_clear_shader (GST_GL_BASE_FILTER (filter)->context);
  transformation->caps_change = FALSE;

  return TRUE;
}
