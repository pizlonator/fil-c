#include "config.h"

#include "gskgpuprintprivate.h"

#include "gskgpudescriptorsprivate.h"
#include "gskgpuimageprivate.h"

void
gsk_gpu_print_indent (GString *string,
                      guint    indent)
{
  g_string_append_printf (string, "%*s", 2 * indent, "");
}

void
gsk_gpu_print_shader_info (GString          *string,
                           GskGpuShaderClip  clip)
{
  switch (clip)
    {
      case GSK_GPU_SHADER_CLIP_NONE:
        g_string_append (string, "🞨 ");
        break;
      case GSK_GPU_SHADER_CLIP_RECT:
        g_string_append (string, "□ ");
        break;
      case GSK_GPU_SHADER_CLIP_ROUNDED:
        g_string_append (string, "▢ ");
        break;
      default:
        g_assert_not_reached ();
        break;
    }
}

void
gsk_gpu_print_op (GString    *string,
                  guint       indent,
                  const char *op_name)
{
  gsk_gpu_print_indent (string, indent);
  g_string_append (string, op_name);
  g_string_append_c (string, ' ');
}

void
gsk_gpu_print_string (GString    *string,
                      const char *s)
{
  g_string_append (string, s);
  g_string_append_c (string, ' ');
}

void
gsk_gpu_print_enum (GString *string,
                    GType    type,
                    int      value)
{
  GEnumClass *class;

  class = g_type_class_ref (type);
  gsk_gpu_print_string (string, g_enum_get_value (class, value)->value_nick);
  g_type_class_unref (class);
}

void
gsk_gpu_print_rect (GString     *string,
                    const float  rect[4])
{
  g_string_append_printf (string, "%g %g %g %g ",
                          rect[0], rect[1],
                          rect[2], rect[3]);
}

void
gsk_gpu_print_int_rect (GString                     *string,
                        const cairo_rectangle_int_t *rect)
{
  g_string_append_printf (string, "%d %d %d %d ",
                          rect->x, rect->y,
                          rect->width, rect->height);
}

void
gsk_gpu_print_rounded_rect (GString     *string,
                            const float  rect[12])
{
  gsk_gpu_print_rect (string, (const float *) rect);

  if (rect[4] == 0.0 && rect[5] == 0.0 &&
      rect[6] == 0.0 && rect[7] == 0.0 &&
      rect[8] == 0.0 && rect[9] == 0.0 &&
      rect[10] == 0.0 && rect[11] == 0.0)
    return;

  g_string_append (string, "/ ");

  if (rect[4] != rect[5] ||
      rect[6] != rect[7] ||
      rect[8] != rect[9] ||
      rect[10] != rect[11])
    {
      g_string_append (string, "variable ");
    }
  else if (rect[4] != rect[6] ||
           rect[4] != rect[8] ||
           rect[4] != rect[10])
    {
      g_string_append_printf (string, "%g %g %g %g ",
                              rect[4], rect[6],
                              rect[8], rect[10]);
    }
  else
    {
      g_string_append_printf (string, "%g ", rect[4]);
    }
}

void
gsk_gpu_print_rgba (GString     *string,
                    const float  rgba[4])
{
  GdkRGBA color = { rgba[0], rgba[1], rgba[2], rgba[3] };
  char *s = gdk_rgba_to_string (&color);
  g_string_append (string, s);
  g_string_append_c (string, ' ');
  g_free (s);
}

void
gsk_gpu_print_newline (GString *string)
{
  if (string->len && string->str[string->len - 1] == ' ')
    string->str[string->len - 1] = '\n';
  else
    g_string_append_c (string, '\n');
}

void
gsk_gpu_print_image (GString     *string,
                     GskGpuImage *image)
{
  g_string_append_printf (string, "%zux%zu ",
                          gsk_gpu_image_get_width (image),
                          gsk_gpu_image_get_height (image));
}

void
gsk_gpu_print_image_descriptor (GString           *string,
                                GskGpuDescriptors *desc,
                                guint32            descriptor)
{
  gsize id = gsk_gpu_descriptors_find_image (desc, descriptor);
  gsk_gpu_print_image (string, gsk_gpu_descriptors_get_image (desc, id));
}

