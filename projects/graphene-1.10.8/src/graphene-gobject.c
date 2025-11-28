/* graphene-gobject.c: Shared GObject types
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * SECTION:graphene-gobject
 * @Title: GObject integration
 * @short_description: Types for GObject properties and signals
 * @Include: graphene-gobject.h
 *
 * Graphene optionally provides information for using its own types with
 * GObject properties and signals.
 *
 * ## Using Graphene with GObject
 *
 * In order to discover at compile time if Graphene exposes type information
 * for the GType type system, you need to check if the `graphene-gobject-1.0`
 * pkg-config file exists.
 *
 * If you're using Meson to build your project, you can use a typical
 * `dependency()` object, for instance:
 *
 * |[<!-- language="plain" -->
 *   graphene_dep = dependency('graphene-gobject-1.0')
 * ]|
 *
 * If you're using Autotools to build your project, you can use the
 * `PKG_CHECK_EXISTS` m4 macro, for instance:
 *
 * |[<!-- language="plain" -->
 *   PKG_CHECK_EXISTS([graphene-gobject-1.0],
 *                    [action-if-found],
 *                    [action-if-not-found]
 * ]|
 *
 * All the types provided by Graphene are boxed types, which means you
 * will have to use the #GBoxed API when dealing with #GValue, #GParamSpec,
 * and signal marshallers. For instance, to install a property in a #GObject
 * class that uses #graphene_rect_t, you can use:
 *
 * |[<!-- language="C" -->
 *   g_object_class_install_property (object_class, PROP_BOUNDS,
 *     g_param_spec_boxed ("bounds", "Bounds", "Bounds of an object",
 *                         GRAPHENE_TYPE_RECT,
 *                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
 * ]|
 *
 * You'll then need to use g_value_set_boxed() and g_value_get_boxed()
 * in order to access the #graphene_rect_t pointer from the #GValue data
 * structure.
 *
 * Whereas for creating a new signal that has a #graphene_point_t parameter
 * you can use:
 *
 * |[<!-- language="C" -->
 *   signals[HIT_TEST] =
 *     g_signal_new ("hit-test",
 *                   G_TYPE_FROM_CLASS (object_class),
 *                   G_SIGNAL_RUN_LAST,
 *                   0,
 *                   g_signal_accumulator_true_handled, NULL,
 *                   marshal_BOOLEAN__BOXED,
 *                   G_TYPE_BOOLEAN, 1,
 *                   GRAPHENE_TYPE_POINT);
 * ]|
 *
 * ## Using Graphene via GObject introspection
 *
 * When using Graphene with another language than C, the GObject Introspection
 * bindings change the type names to the CamelCase version of the C name, minus
 * the `_t` suffix; for instance:
 *
 *  * #graphene_point_t becomes `GraphenePoint`
 *  * #graphene_point3d_t becomes `GraphenePoint3D`
 *  * #graphene_rect_t becomes `GrapheneRect`
 *  * #graphene_matrix_t becomes `GrapheneMatrix`
 *
 * There is no direct access for the low level #graphene_simd4f_t and
 * #graphene_simd4x4f_t SIMD types.
 */

#include "config.h"

#include "graphene-gobject.h"

#define GRAPHENE_ENUM_VALUE(EnumValue, EnumNick) { EnumValue, #EnumValue, EnumNick },

#define GRAPHENE_DEFINE_ENUM_TYPE(TypeName, type_name, values) \
  GType \
  type_name ## _get_type (void) \
  { \
    static gsize graphene_define_id__volatile = 0; \
    if (g_once_init_enter (&graphene_define_id__volatile)) \
      { \
        static const GEnumValue v[] = { \
          values \
          { 0, NULL, NULL }, \
        }; \
        GType graphene_define_id = \
          g_enum_register_static (g_intern_static_string (#TypeName), v); \
        g_once_init_leave (&graphene_define_id__volatile, graphene_define_id); \
      } \
    return graphene_define_id__volatile; \
  }

#define GRAPHENE_DEFINE_BOXED_TYPE(TypeName, type_name) \
  static type_name ## _t * \
  type_name ## _copy_internal (type_name ## _t * v) \
  { \
    type_name ## _t * res = NULL; \
    if (G_LIKELY (v != NULL)) \
      { \
        res = type_name ## _alloc (); \
        *res = *v; \
      } \
    return res; \
  } \
\
  GType \
    type_name ## _get_type (void) \
  { \
    static gsize graphene_define_id__volatile = 0; \
    if (g_once_init_enter (&graphene_define_id__volatile)) \
      { \
        GType graphene_define_id = \
          g_boxed_type_register_static (g_intern_static_string (#TypeName), \
                                        (GBoxedCopyFunc) type_name ## _copy_internal, \
                                        (GBoxedFreeFunc) type_name ## _free); \
        g_once_init_leave (&graphene_define_id__volatile, graphene_define_id); \
      } \
    return graphene_define_id__volatile; \
  }

GRAPHENE_DEFINE_BOXED_TYPE (GraphenePoint, graphene_point)

GRAPHENE_DEFINE_BOXED_TYPE (GraphenePoint3D, graphene_point3d)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneSize, graphene_size)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneRect, graphene_rect)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneVec2, graphene_vec2)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneVec3, graphene_vec3)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneVec4, graphene_vec4)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneQuad, graphene_quad)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneQuaternion, graphene_quaternion)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneMatrix, graphene_matrix)

GRAPHENE_DEFINE_BOXED_TYPE (GraphenePlane, graphene_plane)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneFrustum, graphene_frustum)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneSphere, graphene_sphere)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneBox, graphene_box)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneTriangle, graphene_triangle)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneEuler, graphene_euler)

GRAPHENE_DEFINE_BOXED_TYPE (GrapheneRay, graphene_ray)
