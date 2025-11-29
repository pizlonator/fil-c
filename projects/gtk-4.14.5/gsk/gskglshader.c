/* GSK - The GTK Scene Kit
 *
 * Copyright 2020, Red Hat Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GskGLShader:
 *
 * A `GskGLShader` is a snippet of GLSL that is meant to run in the
 * fragment shader of the rendering pipeline.
 *
 * A fragment shader gets the coordinates being rendered as input and
 * produces the pixel values for that particular pixel. Additionally,
 * the shader can declare a set of other input arguments, called
 * uniforms (as they are uniform over all the calls to your shader in
 * each instance of use). A shader can also receive up to 4
 * textures that it can use as input when producing the pixel data.
 *
 * `GskGLShader` is usually used with gtk_snapshot_push_gl_shader()
 * to produce a [class@Gsk.GLShaderNode] in the rendering hierarchy,
 * and then its input textures are constructed by rendering the child
 * nodes to textures before rendering the shader node itself. (You can
 * pass texture nodes as children if you want to directly use a texture
 * as input).
 *
 * The actual shader code is GLSL code that gets combined with
 * some other code into the fragment shader. Since the exact
 * capabilities of the GPU driver differs between different OpenGL
 * drivers and hardware, GTK adds some defines that you can use
 * to ensure your GLSL code runs on as many drivers as it can.
 *
 * If the OpenGL driver is GLES, then the shader language version
 * is set to 100, and GSK_GLES will be defined in the shader.
 *
 * Otherwise, if the OpenGL driver does not support the 3.2 core profile,
 * then the shader will run with language version 110 for GL2 and 130 for GL3,
 * and GSK_LEGACY will be defined in the shader.
 *
 * If the OpenGL driver supports the 3.2 code profile, it will be used,
 * the shader language version is set to 150, and GSK_GL3 will be defined
 * in the shader.
 *
 * The main function the shader must implement is:
 *
 * ```glsl
 *  void mainImage(out vec4 fragColor,
 *                 in vec2 fragCoord,
 *                 in vec2 resolution,
 *                 in vec2 uv)
 * ```
 *
 * Where the input @fragCoord is the coordinate of the pixel we're
 * currently rendering, relative to the boundary rectangle that was
 * specified in the `GskGLShaderNode`, and @resolution is the width and
 * height of that rectangle. This is in the typical GTK coordinate
 * system with the origin in the top left. @uv contains the u and v
 * coordinates that can be used to index a texture at the
 * corresponding point. These coordinates are in the [0..1]x[0..1]
 * region, with 0, 0 being in the lower left corder (which is typical
 * for OpenGL).
 *
 * The output @fragColor should be a RGBA color (with
 * premultiplied alpha) that will be used as the output for the
 * specified pixel location. Note that this output will be
 * automatically clipped to the clip region of the glshader node.
 *
 * In addition to the function arguments the shader can define
 * up to 4 uniforms for textures which must be called u_textureN
 * (i.e. u_texture1 to u_texture4) as well as any custom uniforms
 * you want of types int, uint, bool, float, vec2, vec3 or vec4.
 *
 * All textures sources contain premultiplied alpha colors, but if some
 * there are outer sources of colors there is a gsk_premultiply() helper
 * to compute premultiplication when needed.
 *
 * Note that GTK parses the uniform declarations, so each uniform has to
 * be on a line by itself with no other code, like so:
 *
 * ```glsl
 * uniform float u_time;
 * uniform vec3 u_color;
 * uniform sampler2D u_texture1;
 * uniform sampler2D u_texture2;
 * ```
 *
 * GTK uses the "gsk" namespace in the symbols it uses in the
 * shader, so your code should not use any symbols with the prefix gsk
 * or GSK. There are some helper functions declared that you can use:
 *
 * ```glsl
 * vec4 GskTexture(sampler2D sampler, vec2 texCoords);
 * ```
 *
 * This samples a texture (e.g. u_texture1) at the specified
 * coordinates, and contains some helper ifdefs to ensure that
 * it works on all OpenGL versions.
 *
 * You can compile the shader yourself using [method@Gsk.GLShader.compile],
 * otherwise the GSK renderer will do it when it handling the glshader
 * node. If errors occurs, the returned @error will include the glsl
 * sources, so you can see what GSK was passing to the compiler. You
 * can also set GSK_DEBUG=shaders in the environment to see the sources
 * and other relevant information about all shaders that GSK is handling.
 *
 * # An example shader
 *
 * ```glsl
 * uniform float position;
 * uniform sampler2D u_texture1;
 * uniform sampler2D u_texture2;
 *
 * void mainImage(out vec4 fragColor,
 *                in vec2 fragCoord,
 *                in vec2 resolution,
 *                in vec2 uv) {
 *   vec4 source1 = GskTexture(u_texture1, uv);
 *   vec4 source2 = GskTexture(u_texture2, uv);
 *
 *   fragColor = position * source1 + (1.0 - position) * source2;
 * }
 * ```
 */

#include "config.h"
#include "gskglshader.h"
#include "gskglshaderprivate.h"
#include "gskdebugprivate.h"

#include "gl/gskglrendererprivate.h"

static GskGLUniformType
uniform_type_from_glsl (const char *str)
{
  if (strcmp (str, "int") == 0)
    return GSK_GL_UNIFORM_TYPE_INT;
  if (strcmp (str, "uint") == 0)
    return GSK_GL_UNIFORM_TYPE_UINT;
  if (strcmp (str, "bool") == 0)
    return GSK_GL_UNIFORM_TYPE_BOOL;
  if (strcmp (str, "float") == 0)
    return GSK_GL_UNIFORM_TYPE_FLOAT;
  if (strcmp (str, "vec2") == 0)
    return GSK_GL_UNIFORM_TYPE_VEC2;
  if (strcmp (str, "vec3") == 0)
    return GSK_GL_UNIFORM_TYPE_VEC3;
  if (strcmp (str, "vec4") == 0)
    return GSK_GL_UNIFORM_TYPE_VEC4;

  return  GSK_GL_UNIFORM_TYPE_NONE;
}

static const char *
uniform_type_name (GskGLUniformType type)
{
  switch (type)
    {
    case GSK_GL_UNIFORM_TYPE_FLOAT:
      return "float";

    case GSK_GL_UNIFORM_TYPE_INT:
      return "int";

    case GSK_GL_UNIFORM_TYPE_UINT:
      return "uint";

    case GSK_GL_UNIFORM_TYPE_BOOL:
      return "bool";

    case GSK_GL_UNIFORM_TYPE_VEC2:
      return "vec2";

    case GSK_GL_UNIFORM_TYPE_VEC3:
      return "vec3";

    case GSK_GL_UNIFORM_TYPE_VEC4:
      return "vec4";

    case GSK_GL_UNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static int
uniform_type_size (GskGLUniformType type)
{
  switch (type)
    {
    case GSK_GL_UNIFORM_TYPE_FLOAT:
      return sizeof (float);

    case GSK_GL_UNIFORM_TYPE_INT:
      return sizeof (gint32);

    case GSK_GL_UNIFORM_TYPE_UINT:
    case GSK_GL_UNIFORM_TYPE_BOOL:
      return sizeof (guint32);

    case GSK_GL_UNIFORM_TYPE_VEC2:
      return sizeof (float) * 2;

    case GSK_GL_UNIFORM_TYPE_VEC3:
      return sizeof (float) * 3;

    case GSK_GL_UNIFORM_TYPE_VEC4:
      return sizeof (float) * 4;

    case GSK_GL_UNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      return 0;
    }
}

struct _GskGLShader
{
  GObject parent_instance;
  GBytes *source;
  char *resource;
  int n_textures;
  int uniforms_size;
  GArray *uniforms;
};

G_DEFINE_TYPE (GskGLShader, gsk_gl_shader, G_TYPE_OBJECT)

enum {
  GLSHADER_PROP_0,
  GLSHADER_PROP_SOURCE,
  GLSHADER_PROP_RESOURCE,
  GLSHADER_N_PROPS
};
static GParamSpec *gsk_gl_shader_properties[GLSHADER_N_PROPS];

static void
gsk_gl_shader_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_SOURCE:
      g_value_set_boxed (value, shader->source);
      break;

    case GLSHADER_PROP_RESOURCE:
      g_value_set_string (value, shader->resource);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_gl_shader_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  switch (prop_id)
    {
    case GLSHADER_PROP_SOURCE:
      g_clear_pointer (&shader->source, g_bytes_unref);
      shader->source = g_value_dup_boxed (value);
      break;

    case GLSHADER_PROP_RESOURCE:
      {
        GError *error = NULL;
        GBytes *source;
        const char *resource;

        resource = g_value_get_string (value);
        if (resource == NULL)
          break;

        source = g_resources_lookup_data (resource, 0, &error);
        if (source)
          {
            g_clear_pointer (&shader->source, g_bytes_unref);
            shader->source = source;
            shader->resource = g_strdup (resource);
          }
        else
          {
            g_critical ("Unable to load resource %s for glshader: %s", resource, error->message);
            g_error_free (error);
            if (shader->source == NULL)
              shader->source = g_bytes_new_static ("", 1);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gsk_gl_shader_finalize (GObject *object)
{
  GskGLShader *shader = GSK_GL_SHADER (object);

  g_bytes_unref (shader->source);
  g_free (shader->resource);
  for (int i = 0; i < shader->uniforms->len; i ++)
    g_free (g_array_index (shader->uniforms, GskGLUniform, i).name);
  g_array_free (shader->uniforms, TRUE);

  G_OBJECT_CLASS (gsk_gl_shader_parent_class)->finalize (object);
}

static GRegex *uniform_regexp = NULL; /* Initialized in class_init */


static void
gsk_gl_shader_add_uniform (GskGLShader     *shader,
                           const char      *name,
                           GskGLUniformType type)
{
  GskGLUniform uniform = {
    g_strdup (name),
    type,
    shader->uniforms_size
  };

  shader->uniforms_size += uniform_type_size (type);

  g_array_append_val (shader->uniforms, uniform);
}

static void
gsk_gl_shader_constructed (GObject *object)
{
  GskGLShader *shader = GSK_GL_SHADER (object);
  gsize string_len;
  const char *string = g_bytes_get_data (shader->source, &string_len);
  GMatchInfo *match_info;
  int max_texture_seen = 0;

  g_regex_match_full (uniform_regexp,
                      string, string_len, 0, 0,
                      &match_info, NULL);
  while (g_match_info_matches (match_info))
    {
      char *type = g_match_info_fetch (match_info, 1);
      char *name = g_match_info_fetch (match_info, 2);

      if (strcmp (type, "sampler2D") == 0)
        {
          /* Textures are special cased */

          if (g_str_has_prefix (name, "u_texture") &&
              strlen (name) == strlen ("u_texture")+1)
            {
              char digit = name[strlen("u_texture")];
              if (digit >= '1' && digit <= '9')
                max_texture_seen = MAX(max_texture_seen, digit - '0');
            }
          else
            g_debug ("Unhandled shader texture uniform '%s', use uniforms of name 'u_texture[1..9]'", name);
        }
      else
        {
          GskGLUniformType utype = uniform_type_from_glsl (type);
          g_assert (utype != GSK_GL_UNIFORM_TYPE_NONE); // Shouldn't have matched the regexp
          gsk_gl_shader_add_uniform (shader, name, utype);
        }

      g_free (type);
      g_free (name);

      g_match_info_next (match_info, NULL);
    }

  g_match_info_free (match_info);

  shader->n_textures = max_texture_seen;

  if (GSK_DEBUG_CHECK(SHADERS))
    {
      GString *s;

      s = g_string_new ("");
      for (int i = 0; i < shader->uniforms->len; i++)
        {
          GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
          if (i > 0)
            g_string_append (s, ", ");
          g_string_append_printf (s, "%s %s", uniform_type_name (u->type), u->name);
        }
      g_message ("Shader constructed: %d textures, %d uniforms (%s)",
                 shader->n_textures, shader->uniforms->len,
                 s->str);
      g_string_free (s, TRUE);
    }
}

#define SPACE_RE "[ \\t]+" // Don't use \s, we don't want to match newlines
#define OPT_SPACE_RE "[ \\t]*"
#define UNIFORM_TYPE_RE "(int|uint|bool|float|vec2|vec3|vec4|sampler2D)"
#define UNIFORM_NAME_RE "([\\w]+)"
#define OPT_INIT_VALUE_RE "[-\\w(),. ]+" // This is a bit simple, but will match most initializers
#define OPT_COMMENT_RE "(//.*)?"
#define OPT_INITIALIZER_RE  "(" OPT_SPACE_RE "=" OPT_SPACE_RE  OPT_INIT_VALUE_RE ")?"
#define UNIFORM_MATCHER_RE "^uniform" SPACE_RE UNIFORM_TYPE_RE SPACE_RE UNIFORM_NAME_RE OPT_INITIALIZER_RE OPT_SPACE_RE ";" OPT_SPACE_RE OPT_COMMENT_RE "$"

static void
gsk_gl_shader_class_init (GskGLShaderClass *klass)
{
   GObjectClass *object_class = G_OBJECT_CLASS (klass);
   uniform_regexp = g_regex_new (UNIFORM_MATCHER_RE,
                                 G_REGEX_MULTILINE | G_REGEX_RAW | G_REGEX_OPTIMIZE,
                                 0, NULL);

  object_class->get_property = gsk_gl_shader_get_property;
  object_class->set_property = gsk_gl_shader_set_property;
  object_class->finalize = gsk_gl_shader_finalize;
  object_class->constructed = gsk_gl_shader_constructed;

  /**
   * GskGLShader:sourcecode: (attributes org.gtk.Property.get=gsk_gl_shader_get_source)
   *
   * The source code for the shader, as a `GBytes`.
   */
  gsk_gl_shader_properties[GLSHADER_PROP_SOURCE] =
    g_param_spec_boxed ("source", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

  /**
   * GskGLShader:resource: (attributes org.gtk.Property.get=gsk_gl_shader_get_resource)
   *
   * Resource containing the source code for the shader.
   *
   * If the shader source is not coming from a resource, this
   * will be %NULL.
   */
  gsk_gl_shader_properties[GLSHADER_PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, GLSHADER_N_PROPS, gsk_gl_shader_properties);
}

static void
gsk_gl_shader_init (GskGLShader *shader)
{
  shader->uniforms = g_array_new (FALSE, FALSE, sizeof (GskGLUniform));
}

/**
 * gsk_gl_shader_new_from_bytes:
 * @sourcecode: GLSL sourcecode for the shader, as a `GBytes`
 *
 * Creates a `GskGLShader` that will render pixels using the specified code.
 *
 * Returns: (transfer full): A new `GskGLShader`
 */
GskGLShader *
gsk_gl_shader_new_from_bytes (GBytes *sourcecode)
{
  g_return_val_if_fail (sourcecode != NULL, NULL);

  return g_object_new (GSK_TYPE_GL_SHADER,
                       "source", sourcecode,
                       NULL);
}

/**
 * gsk_gl_shader_new_from_resource:
 * @resource_path: path to a resource that contains the GLSL sourcecode for
 *     the shader
 *
 * Creates a `GskGLShader` that will render pixels using the specified code.
 *
 * Returns: (transfer full): A new `GskGLShader`
 */
GskGLShader *
gsk_gl_shader_new_from_resource (const char *resource_path)
{
  g_return_val_if_fail (resource_path != NULL, NULL);

  return g_object_new (GSK_TYPE_GL_SHADER,
                       "resource", resource_path,
                       NULL);
}

/**
 * gsk_gl_shader_compile:
 * @shader: a `GskGLShader`
 * @renderer: a `GskRenderer`
 * @error: location to store error in
 *
 * Tries to compile the @shader for the given @renderer.
 *
 * If there is a problem, this function returns %FALSE and reports
 * an error. You should use this function before relying on the shader
 * for rendering and use a fallback with a simpler shader or without
 * shaders if it fails.
 *
 * Note that this will modify the rendering state (for example
 * change the current GL context) and requires the renderer to be
 * set up. This means that the widget has to be realized. Commonly you
 * want to call this from the realize signal of a widget, or during
 * widget snapshot.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gsk_gl_shader_compile (GskGLShader  *shader,
                       GskRenderer  *renderer,
                       GError      **error)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), FALSE);

  if (GSK_IS_GL_RENDERER (renderer))
    return gsk_gl_renderer_try_compile_gl_shader (GSK_GL_RENDERER (renderer), shader, error);

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "The renderer does not support gl shaders");
  return FALSE;
}


/**
 * gsk_gl_shader_get_source: (attributes org.gtk.Method.get_property=source)
 * @shader: a `GskGLShader`
 *
 * Gets the GLSL sourcecode being used to render this shader.
 *
 * Returns: (transfer none): The source code for the shader
 */
GBytes *
gsk_gl_shader_get_source (GskGLShader *shader)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);

  return shader->source;
}

/**
 * gsk_gl_shader_get_resource: (attributes org.gtk.Method.get_property=resource)
 * @shader: a `GskGLShader`
 *
 * Gets the resource path for the GLSL sourcecode being used
 * to render this shader.
 *
 * Returns: (transfer none) (nullable): The resource path for the shader
 */
const char *
gsk_gl_shader_get_resource (GskGLShader *shader)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);

  return shader->resource;
}

/**
 * gsk_gl_shader_get_n_textures:
 * @shader: a `GskGLShader`
 *
 * Returns the number of textures that the shader requires.
 *
 * This can be used to check that the a passed shader works
 * in your usecase. It is determined by looking at the highest
 * u_textureN value that the shader defines.
 *
 * Returns: The number of texture inputs required by @shader
 */
int
gsk_gl_shader_get_n_textures (GskGLShader *shader)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  return shader->n_textures;
}

/**
 * gsk_gl_shader_get_n_uniforms:
 * @shader: a `GskGLShader`
 *
 * Get the number of declared uniforms for this shader.
 *
 * Returns: The number of declared uniforms
 */
int
gsk_gl_shader_get_n_uniforms (GskGLShader *shader)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  return shader->uniforms->len;
}

/**
 * gsk_gl_shader_get_uniform_name:
 * @shader: a `GskGLShader`
 * @idx: index of the uniform
 *
 * Get the name of the declared uniform for this shader at index @idx.
 *
 * Returns: (transfer none): The name of the declared uniform
 */
const char *
gsk_gl_shader_get_uniform_name (GskGLShader *shader,
                                int          idx)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);
  g_return_val_if_fail (0 <= idx && idx < shader->uniforms->len, NULL);

  return g_array_index (shader->uniforms, GskGLUniform, idx).name;
}

/**
 * gsk_gl_shader_find_uniform_by_name:
 * @shader: a `GskGLShader`
 * @name: uniform name
 *
 * Looks for a uniform by the name @name, and returns the index
 * of the uniform, or -1 if it was not found.
 *
 * Returns: The index of the uniform, or -1
 */
int
gsk_gl_shader_find_uniform_by_name (GskGLShader *shader,
                                    const char  *name)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), -1);

  for (int i = 0; i < shader->uniforms->len; i++)
    {
      const GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
      if (strcmp (u->name, name) == 0)
        return i;
    }

  return -1;
}

/**
 * gsk_gl_shader_get_uniform_type:
 * @shader: a `GskGLShader`
 * @idx: index of the uniform
 *
 * Get the type of the declared uniform for this shader at index @idx.
 *
 * Returns: The type of the declared uniform
 */
GskGLUniformType
gsk_gl_shader_get_uniform_type (GskGLShader *shader,
                                int          idx)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);
  g_return_val_if_fail (0 <= idx && idx < shader->uniforms->len, 0);

  return g_array_index (shader->uniforms, GskGLUniform, idx).type;
}

/**
 * gsk_gl_shader_get_uniform_offset:
 * @shader: a `GskGLShader`
 * @idx: index of the uniform
 *
 * Get the offset into the data block where data for this uniforms is stored.
 *
 * Returns: The data offset
 */
int
gsk_gl_shader_get_uniform_offset (GskGLShader *shader,
                                  int          idx)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);
  g_return_val_if_fail (0 <= idx && idx < shader->uniforms->len, 0);

  return g_array_index (shader->uniforms, GskGLUniform, idx).offset;
}

const GskGLUniform *
gsk_gl_shader_get_uniforms (GskGLShader *shader,
                            int         *n_uniforms)
{
  *n_uniforms = shader->uniforms->len;
  return &g_array_index (shader->uniforms, GskGLUniform, 0);
}

/**
 * gsk_gl_shader_get_args_size:
 * @shader: a `GskGLShader`
 *
 * Get the size of the data block used to specify arguments for this shader.
 *
 * Returns: The size of the data block
 */
gsize
gsk_gl_shader_get_args_size (GskGLShader *shader)
{
  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  return shader->uniforms_size;
}

static const GskGLUniform *
gsk_gl_shader_find_uniform (GskGLShader *shader,
                            const char  *name)
{
  for (int i = 0; i < shader->uniforms->len; i++)
    {
      const GskGLUniform *u = &g_array_index (shader->uniforms, GskGLUniform, i);
      if (strcmp (u->name, name) == 0)
        return u;
    }

  return NULL;
}

/**
 * gsk_gl_shader_get_arg_float:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of float type.
 *
 * Returns: The value
 */
float
gsk_gl_shader_get_arg_float (GskGLShader *shader,
                             GBytes      *args,
                             int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_FLOAT);

  args_src = data + u->offset;
  return *(float *)args_src;
}

/**
 * gsk_gl_shader_get_arg_int:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of int type.
 *
 * Returns: The value
 */
gint32
gsk_gl_shader_get_arg_int (GskGLShader *shader,
                           GBytes      *args,
                           int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_INT);

  args_src = data + u->offset;
  return *(gint32 *)args_src;
}

/**
 * gsk_gl_shader_get_arg_uint:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of uint type.
 *
 * Returns: The value
 */
guint32
gsk_gl_shader_get_arg_uint (GskGLShader *shader,
                            GBytes      *args,
                            int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_UINT);

  args_src = data + u->offset;
  return *(guint32 *)args_src;
}

/**
 * gsk_gl_shader_get_arg_bool:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of bool type.
 *
 * Returns: The value
 */
gboolean
gsk_gl_shader_get_arg_bool (GskGLShader *shader,
                            GBytes      *args,
                            int          idx)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), 0);

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_BOOL);

  args_src = data + u->offset;
  return *(guint32 *)args_src;
}

/**
 * gsk_gl_shader_get_arg_vec2:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 * @out_value: location to store the uniform value in
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of vec2 type.
 */
void
gsk_gl_shader_get_arg_vec2 (GskGLShader     *shader,
                            GBytes          *args,
                            int              idx,
                            graphene_vec2_t *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_if_fail (GSK_IS_GL_SHADER (shader));

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC2);

  args_src = data + u->offset;
  graphene_vec2_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_get_arg_vec3:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 * @out_value: location to store the uniform value in
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of vec3 type.
 */
void
gsk_gl_shader_get_arg_vec3 (GskGLShader     *shader,
                            GBytes          *args,
                            int              idx,
                            graphene_vec3_t *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_if_fail (GSK_IS_GL_SHADER (shader));

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC3);

  args_src = data + u->offset;
  graphene_vec3_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_get_arg_vec4:
 * @shader: a `GskGLShader`
 * @args: uniform arguments
 * @idx: index of the uniform
 * @out_value: location to store set the uniform value in
 *
 * Gets the value of the uniform @idx in the @args block.
 *
 * The uniform must be of vec4 type.
 */
void
gsk_gl_shader_get_arg_vec4 (GskGLShader     *shader,
                            GBytes          *args,
                            int              idx,
                            graphene_vec4_t *out_value)
{
  const GskGLUniform *u;
  const guchar *args_src;
  gsize size;
  const guchar *data = g_bytes_get_data (args, &size);

  g_return_if_fail (GSK_IS_GL_SHADER (shader));

  g_assert (size == shader->uniforms_size);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC4);

  args_src = data + u->offset;
  graphene_vec4_init_from_float (out_value, (float *)args_src);
}

/**
 * gsk_gl_shader_format_args_va:
 * @shader: a `GskGLShader`
 * @uniforms: name-Value pairs for the uniforms of @shader, ending
 *     with a %NULL name
 *
 * Formats the uniform data as needed for feeding the named uniforms
 * values into the shader.
 *
 * The argument list is a list of pairs of names, and values for the
 * types that match the declared uniforms (i.e. double/int/guint/gboolean
 * for primitive values and `graphene_vecN_t *` for vecN uniforms).
 *
 * It is an error to pass a uniform name that is not declared by the shader.
 *
 * Any uniforms of the shader that are not included in the argument list
 * are zero-initialized.
 *
 * Returns: (transfer full): A newly allocated block of data which can be
 *     passed to [ctor@Gsk.GLShaderNode.new].
 */
GBytes *
gsk_gl_shader_format_args_va (GskGLShader *shader,
                              va_list uniforms)
{
  guchar *args = g_malloc0 (shader->uniforms_size);
  const char *name;

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);

  while ((name = va_arg (uniforms, const char *)) != NULL)
    {
      const GskGLUniform *u;
      guchar *args_dest;

      u = gsk_gl_shader_find_uniform (shader, name);
      if (u == NULL)
        {
          g_warning ("No uniform named `%s` in shader", name);
          break;
        }

      args_dest = args + u->offset;

      switch (u->type)
        {
        case GSK_GL_UNIFORM_TYPE_FLOAT:
          *(float *)args_dest = (float)va_arg (uniforms, double); /* floats are promoted to double in varargs */
          break;

        case GSK_GL_UNIFORM_TYPE_INT:
          *(gint32 *)args_dest = (gint32)va_arg (uniforms, int);
          break;

        case GSK_GL_UNIFORM_TYPE_UINT:
          *(guint32 *)args_dest = (guint32)va_arg (uniforms, guint);
          break;

        case GSK_GL_UNIFORM_TYPE_BOOL:
          *(guint32 *)args_dest = (gboolean)va_arg (uniforms, gboolean);
          break;

        case GSK_GL_UNIFORM_TYPE_VEC2:
          graphene_vec2_to_float (va_arg (uniforms, const graphene_vec2_t *),
                                  (float *)args_dest);
          break;

        case GSK_GL_UNIFORM_TYPE_VEC3:
          graphene_vec3_to_float (va_arg (uniforms, const graphene_vec3_t *),
                                  (float *)args_dest);
          break;

        case GSK_GL_UNIFORM_TYPE_VEC4:
          graphene_vec4_to_float (va_arg (uniforms, const graphene_vec4_t *),
                                  (float *)args_dest);
          break;

        case GSK_GL_UNIFORM_TYPE_NONE:
        default:
          g_assert_not_reached ();
        }
    }

  return g_bytes_new_take (args, shader->uniforms_size);
}

/**
 * gsk_gl_shader_format_args:
 * @shader: a `GskGLShader`
 * @...: name-Value pairs for the uniforms of @shader, ending with
 *     a %NULL name
 *
 * Formats the uniform data as needed for feeding the named uniforms
 * values into the shader.
 *
 * The argument list is a list of pairs of names, and values for the types
 * that match the declared uniforms (i.e. double/int/guint/gboolean for
 * primitive values and `graphene_vecN_t *` for vecN uniforms).
 *
 * Any uniforms of the shader that are not included in the argument list
 * are zero-initialized.
 *
 * Returns: (transfer full): A newly allocated block of data which can be
 *     passed to [ctor@Gsk.GLShaderNode.new].
 */
GBytes *
gsk_gl_shader_format_args (GskGLShader *shader,
                           ...)
{
  GBytes *bytes;
  va_list args;

  va_start (args, shader);
  bytes = gsk_gl_shader_format_args_va (shader, args);
  va_end (args);

  return bytes;
}

struct _GskShaderArgsBuilder {
  guint ref_count;
  GskGLShader *shader;
  guchar *data;
};

G_DEFINE_BOXED_TYPE (GskShaderArgsBuilder, gsk_shader_args_builder,
                     gsk_shader_args_builder_ref,
                     gsk_shader_args_builder_unref);


/**
 * gsk_shader_args_builder_new:
 * @shader: a `GskGLShader`
 * @initial_values: (nullable): optional `GBytes` with initial values
 *
 * Allocates a builder that can be used to construct a new uniform data
 * chunk.
 *
 * Returns: (transfer full): The newly allocated builder, free with
 *     [method@Gsk.ShaderArgsBuilder.unref]
 */
GskShaderArgsBuilder *
gsk_shader_args_builder_new (GskGLShader *shader,
                             GBytes      *initial_values)
{
  GskShaderArgsBuilder *builder = g_new0 (GskShaderArgsBuilder, 1);
  builder->ref_count = 1;
  builder->shader = g_object_ref (shader);
  builder->data = g_malloc0 (shader->uniforms_size);

  if (initial_values)
    {
      gsize size;
      const guchar *data = g_bytes_get_data (initial_values, &size);

      g_assert (size == shader->uniforms_size);
      memcpy (builder->data, data, size);
    }

  return builder;
}

/**
 * gsk_shader_args_builder_to_args:
 * @builder: a `GskShaderArgsBuilder`
 *
 * Creates a new `GBytes` args from the current state of the
 * given @builder.
 *
 * Any uniforms of the shader that have not been explicitly set on
 * the @builder are zero-initialized.
 *
 * The given `GskShaderArgsBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same @builder instance.
 *
 * This function is intended primarily for bindings. C code should use
 * [method@Gsk.ShaderArgsBuilder.free_to_args].
 *
 *
 * Returns: (transfer full): the newly allocated buffer with
 *   all the args added to @builder
 */
GBytes *
gsk_shader_args_builder_to_args (GskShaderArgsBuilder *builder)
{
  return g_bytes_new_take (g_steal_pointer (&builder->data),
                           builder->shader->uniforms_size);
}

/**
 * gsk_shader_args_builder_free_to_args: (skip)
 * @builder: a `GskShaderArgsBuilder`
 *
 * Creates a new `GBytes` args from the current state of the
 * given @builder, and frees the @builder instance.
 *
 * Any uniforms of the shader that have not been explicitly set
 * on the @builder are zero-initialized.
 *
 * Returns: (transfer full): the newly allocated buffer with
 *   all the args added to @builder
 */
GBytes *
gsk_shader_args_builder_free_to_args (GskShaderArgsBuilder *builder)
{
  GBytes *res;

  g_return_val_if_fail (builder != NULL, NULL);

  res = gsk_shader_args_builder_to_args (builder);

  gsk_shader_args_builder_unref (builder);

  return res;
}


/**
 * gsk_shader_args_builder_unref:
 * @builder: a `GskShaderArgsBuilder`
 *
 * Decreases the reference count of a `GskShaderArgBuilder` by one.
 *
 * If the resulting reference count is zero, frees the builder.
 */
void
gsk_shader_args_builder_unref (GskShaderArgsBuilder *builder)

{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->ref_count > 0);

  builder->ref_count--;
  if (builder->ref_count > 0)
    return;

  g_object_unref (builder->shader);
  g_free (builder->data);
  g_free (builder);
}

/**
 * gsk_shader_args_builder_ref:
 * @builder: a `GskShaderArgsBuilder`
 *
 * Increases the reference count of a `GskShaderArgsBuilder` by one.
 *
 * Returns: the passed in `GskShaderArgsBuilder`
 */
GskShaderArgsBuilder *
gsk_shader_args_builder_ref (GskShaderArgsBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);

  builder->ref_count++;
  return builder;
}

/**
 * gsk_shader_args_builder_set_float:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform to
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of float type.
 */
void
gsk_shader_args_builder_set_float (GskShaderArgsBuilder *builder,
                                   int                   idx,
                                   float                 value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_FLOAT);

  args_dest = builder->data + u->offset;
  *(float *)args_dest = value;
}

/**
 * gsk_shader_args_builder_set_int:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform to
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of int type.
 */
void
gsk_shader_args_builder_set_int (GskShaderArgsBuilder *builder,
                                 int                   idx,
                                 gint32                value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_INT);

  args_dest = builder->data + u->offset;
  *(gint32 *)args_dest = value;
}

/**
 * gsk_shader_args_builder_set_uint:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform to
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of uint type.
 */
void
gsk_shader_args_builder_set_uint (GskShaderArgsBuilder *builder,
                                  int                   idx,
                                  guint32               value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_UINT);

  args_dest = builder->data + u->offset;
  *(guint32 *)args_dest = value;
}

/**
 * gsk_shader_args_builder_set_bool:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform to
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of bool type.
 */
void
gsk_shader_args_builder_set_bool (GskShaderArgsBuilder *builder,
                                  int                   idx,
                                  gboolean              value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_BOOL);

  args_dest = builder->data + u->offset;
  *(guint32 *)args_dest = !!value;
}

/**
 * gsk_shader_args_builder_set_vec2:
 * @builder: A `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of vec2 type.
 */
void
gsk_shader_args_builder_set_vec2 (GskShaderArgsBuilder  *builder,
                                  int                    idx,
                                  const graphene_vec2_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC2);

  args_dest = builder->data + u->offset;
  graphene_vec2_to_float (value, (float *)args_dest);
}

/**
 * gsk_shader_args_builder_set_vec3:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of vec3 type.
 */
void
gsk_shader_args_builder_set_vec3 (GskShaderArgsBuilder  *builder,
                                  int                    idx,
                                  const graphene_vec3_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC3);

  args_dest = builder->data + u->offset;
  graphene_vec3_to_float (value, (float *)args_dest);
}

/**
 * gsk_shader_args_builder_set_vec4:
 * @builder: a `GskShaderArgsBuilder`
 * @idx: index of the uniform
 * @value: value to set the uniform too
 *
 * Sets the value of the uniform @idx.
 *
 * The uniform must be of vec4 type.
 */
void
gsk_shader_args_builder_set_vec4 (GskShaderArgsBuilder  *builder,
                                  int                    idx,
                                  const graphene_vec4_t *value)
{
  GskGLShader *shader = builder->shader;
  const GskGLUniform *u;
  guchar *args_dest;

  g_assert (builder->data != NULL);
  g_assert (idx < shader->uniforms->len);
  u = &g_array_index (shader->uniforms, GskGLUniform, idx);
  g_assert (u->type == GSK_GL_UNIFORM_TYPE_VEC4);

  args_dest = builder->data + u->offset;
  graphene_vec4_to_float (value, (float *)args_dest);
}
