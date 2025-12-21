/*
 * GStreamer
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstglcontextconfig
 * @short_description: OpenGL context configuration values
 * @title: GstGLContextConfig
 * @see_also: #GstGLContext, #GstGLWindow
 *
 * A common list of well-known values for what a config retrievable from or set
 * on a `GstGLContext` may contain.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gstglcontextconfig.h"
#include <gst/gl/gl.h>

/**
 * GST_GL_CONFIG_ATTRIB_CONFIG_ID_NAME:
 *
 * The platform-specific config-id.  This value is not stable across different
 * machines or even different versions of the same underlying OpenGL
 * implementation.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_CONFIG_ID_GTYPE:
 *
 * The #GType of the config-id field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_CONFIG_ID_NAME = "config-id";
/**
 * GST_GL_CONFIG_ATTRIB_PLATFORM_NAME:
 *
 * The #GstGLPlatform this config was made for.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_PLATFORM_GTYPE:
 *
 * The #GType of the 'platform' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_PLATFORM_NAME = "platform";
/**
 * GST_GL_CONFIG_ATTRIB_CAVEAT_NAME:
 *
 * Any #GstGLConfigCaveat's applied to this configuration.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_CAVEAT_GTYPE:
 *
 * The #GType of the 'caveat' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_CAVEAT_NAME = "caveat";
/**
 * GST_GL_CONFIG_ATTRIB_SURFACE_TYPE_NAME:
 *
 * Flags of #GstGLConfigSurfaceType's that can apply to this configuration.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_SURFACE_TYPE_GTYPE:
 *
 * The #GType of the 'surface-type' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_SURFACE_TYPE_NAME = "surface-type";
/**
 * GST_GL_CONFIG_ATTRIB_CONFORMANT_API_NAME:
 *
 * The #GstGLAPI's that this configuration meets the conformance requirements
 * for.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_CONFORMANT_API_GTYPE:
 *
 * The #GType of the 'conformant-api' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_CONFORMANT_API_NAME = "conformant-api";
/**
 * GST_GL_CONFIG_ATTRIB_RENDERABLE_API_NAME:
 *
 * The #GstGLAPI's that this configuration can be rendered with.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_RENDERABLE_API_GTYPE:
 *
 * The #GType of the 'renderable-api' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_RENDERABLE_API_NAME = "renderable-api";
/**
 * GST_GL_CONFIG_ATTRIB_RED_SIZE_NAME:
 *
 * The size of the red buffer with a colour backing buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_RED_SIZE_GTYPE:
 *
 * The #GType of the 'red-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_RED_SIZE_NAME = "red-size";
/**
 * GST_GL_CONFIG_ATTRIB_GREEN_SIZE_NAME:
 *
 * The size of the green buffer with a colour backing buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_GREEN_SIZE_GTYPE:
 *
 * The #GType of the 'green-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_GREEN_SIZE_NAME = "green-size";
/**
 * GST_GL_CONFIG_ATTRIB_BLUE_SIZE_NAME:
 *
 * The size of the blue buffer with a colour backing buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_BLUE_SIZE_GTYPE:
 *
 * The #GType of the 'blue-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_BLUE_SIZE_NAME = "blue-size";
/**
 * GST_GL_CONFIG_ATTRIB_ALPHA_SIZE_NAME:
 *
 * The size of the alpha buffer with a colour backing buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_ALPHA_SIZE_GTYPE:
 *
 * The #GType of the 'alpha-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_ALPHA_SIZE_NAME = "alpha-size";
/**
 * GST_GL_CONFIG_ATTRIB_LUMINANCE_SIZE_NAME:
 *
 * The size of the backing luminance buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_LUMINANCE_SIZE_GTYPE:
 *
 * The #GType of the 'luminance-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_LUMINANCE_SIZE_NAME = "luminance-size";
/**
 * GST_GL_CONFIG_ATTRIB_DEPTH_SIZE_NAME:
 *
 * The size of the backing depth buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_DEPTH_SIZE_GTYPE:
 *
 * The #GType of the 'depth-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_DEPTH_SIZE_NAME = "depth-size";
/**
 * GST_GL_CONFIG_ATTRIB_STENCIL_SIZE_NAME:
 *
 * The size of the backing stencil buffer.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_STENCIL_SIZE_GTYPE:
 *
 * The #GType of the 'stencil-size' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_STENCIL_SIZE_NAME = "stencil-size";
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_WIDTH_NAME:
 *
 * The maximum width of a pbuffer created with this config.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_WIDTH_GTYPE:
 *
 * The #GType of the 'max-pbuffer-width' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_WIDTH_NAME = "max-pbuffer-width";
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_HEIGHT_NAME:
 *
 * The maximum height of a pbuffer created with this config.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_HEIGHT_GTYPE:
 *
 * The #GType of the 'max-pbuffer-height' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_HEIGHT_NAME =
    "max-pbuffer-height";
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_PIXELS_NAME:
 *
 * The maximum number of pixels that a pbuffer can be created with this config.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_PIXELS_GTYPE:
 *
 * The #GType of the 'max-pbuffer-pixels' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_MAX_PBUFFER_PIXELS_NAME =
    "max-pbuffer-pixels";
/**
 * GST_GL_CONFIG_ATTRIB_SAMPLE_BUFFERS_NAME:
 *
 * The number of sample buffers for this config.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_SAMPLE_BUFFERS_GTYPE:
 *
 * The #GType of the 'sample-buffers' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_SAMPLE_BUFFERS_NAME = "sample-buffers";
/**
 * GST_GL_CONFIG_ATTRIB_SAMPLES_NAME:
 *
 * The number of samples per pixel for this config.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_SAMPLES_GTYPE:
 *
 * The #GType of the 'samples' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_SAMPLES_NAME = "samples";
/**
 * GST_GL_CONFIG_ATTRIB_NATIVE_RENDERABLE_NAME:
 *
 * Whether this configuration is renderable to by the native drawing API.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_NATIVE_RENDERABLE_GTYPE:
 *
 * The #GType of the 'native-renderable' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_NATIVE_RENDERABLE_NAME = "native-renderable";
/**
 * GST_GL_CONFIG_ATTRIB_NATIVE_VISUAL_ID_NAME:
 *
 * The native visual ID of this config.  This value may not be consistent
 * across machines or even dependency versions.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_NATIVE_VISUAL_ID_GTYPE:
 *
 * The #GType of the 'native-visual-id' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_NATIVE_VISUAL_ID_NAME = "native-visual-id";
/**
 * GST_GL_CONFIG_ATTRIB_LEVEL_NAME:
 *
 * Level of the under/overlay of this config.  Positive values correspond to
 * overlay, negative values are underlay.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_LEVEL_GTYPE:
 *
 * The #GType of the 'level' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_LEVEL_NAME = "level";
/**
 * GST_GL_CONFIG_ATTRIB_MIN_SWAP_INTERVAL_NAME:
 *
 * The minimum value available for vsync synchronisation.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_MIN_SWAP_INTERVAL_GTYPE:
 *
 * The #GType of the 'min-swap-interval' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_MIN_SWAP_INTERVAL_NAME = "min-swap-interval";
/**
 * GST_GL_CONFIG_ATTRIB_MAX_SWAP_INTERVAL_NAME:
 *
 * The maximum value available for vsync synchronisation.
 *
 * Since: 1.20
 */
/**
 * GST_GL_CONFIG_ATTRIB_MAX_SWAP_INTERVAL_GTYPE:
 *
 * The #GType of the 'max-swap-interval' field.
 *
 * Since: 1.20
 */
const gchar *GST_GL_CONFIG_ATTRIB_MAX_SWAP_INTERVAL_NAME = "max-swap-interval";

static const gchar *
gst_gl_enum_value_to_const_string (GType type, guint value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  const gchar *str = NULL;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  if (enum_value)
    str = enum_value->value_nick;

  g_type_class_unref (enum_class);

  return str;
}

/**
 * gst_gl_config_caveat_to_string:
 * @caveat: the #GstGLConfigCaveat
 *
 * Returns: (nullable): a string version of @caveat or %NULL if @caveat does not
 *                      exist.
 *
 * Since: 1.20
 */
const gchar *
gst_gl_config_caveat_to_string (GstGLConfigCaveat caveat)
{
  return gst_gl_enum_value_to_const_string (GST_TYPE_GL_CONFIG_CAVEAT, caveat);
}

/**
 * gst_gl_config_surface_type_to_string:
 * @surface_type: the #GstGLConfigSurfaceType
 *
 * Returns: (nullable): a string version of @caveat or %NULL if @surface_type does not
 *                      exist.
 *
 * Since: 1.20
 */
const gchar *
gst_gl_config_surface_type_to_string (GstGLConfigSurfaceType surface_type)
{
  return gst_gl_enum_value_to_const_string (GST_TYPE_GL_CONFIG_SURFACE_TYPE,
      surface_type);
}
